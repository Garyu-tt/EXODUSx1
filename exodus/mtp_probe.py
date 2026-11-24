"""Read Windows USB and Portable Devices enumeration for one VID/PID."""

import argparse
from dataclasses import dataclass, field
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time
from typing import Optional, Tuple


QUERY_TIMEOUT = 15
POLL_INTERVAL = 1
USB_ID = re.compile(r"[0-9a-fA-F]{4}:[0-9a-fA-F]{4}")
PNP_QUERY = r"""
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$vendor, $product = $env:EXODUS_MTP_USB_ID.Split(':')
$prefix = 'USB\VID_{0}&PID_{1}' -f $vendor, $product
$interfacePattern = "$prefix&MI_*\*"
$matchingDevices = @(Get-CimInstance -ClassName Win32_PnPEntity `
    -Filter 'Present = TRUE' `
    -Property PNPDeviceID,PNPClass,CompatibleID,ConfigManagerErrorCode |
    Where-Object { $_.PNPDeviceID -like "$prefix\*" -or $_.PNPDeviceID -like $interfacePattern })
$parents = @{}
foreach ($device in $matchingDevices) {
    if ($device.PNPDeviceID -notlike $interfacePattern) { continue }
    # A device can vanish between enumeration and the property lookup.
    $result = Invoke-CimMethod -InputObject $device -MethodName GetDeviceProperties `
        -Arguments @{ devicePropertyKeys = [string[]]@('DEVPKEY_Device_Parent') } `
        -ErrorAction SilentlyContinue
    if ($result.ReturnValue -eq 0 -and $result.deviceProperties) {
        $parents[$device.PNPDeviceID] = $result.deviceProperties[0].Data
    }
}
$devices = @($matchingDevices | Select-Object PNPDeviceID,PNPClass,CompatibleID,ConfigManagerErrorCode,
    @{Name = 'ParentInstanceId'; Expression = { $parents[$_.PNPDeviceID] }})
ConvertTo-Json -InputObject $devices -Compress
"""


class ProbeError(Exception):
    """Windows enumeration did not return a usable sample."""


class ProbeTimeout(ProbeError):
    """Windows enumeration exceeded its time budget."""


@dataclass(frozen=True)
class Snapshot:
    state: str
    usb_nodes: int
    wpd_nodes: int
    mtp_advertised: bool
    problem_codes: Tuple[int, ...]
    parent_id: Optional[str] = field(repr=False)

    def report(self):
        return {
            "state": self.state,
            "usb_nodes": self.usb_nodes,
            "wpd_nodes": self.wpd_nodes,
            "mtp_advertised": self.mtp_advertised,
            "problem_codes": list(self.problem_codes),
        }


def classify(devices, usb_id):
    vendor, product = usb_id.split(":")
    target = re.compile(
        rf"USB\\VID_{vendor}&PID_{product}(?P<interface>&MI_[0-9A-F]{{2}})?\\[^\\]+",
        re.IGNORECASE,
    )
    parents = {}
    matching_nodes = []
    if not isinstance(devices, list):
        raise ProbeError("Windows returned an invalid device list.")
    for device in devices:
        if not isinstance(device, dict) or not isinstance(device.get("PNPDeviceID"), str):
            raise ProbeError("Windows returned an invalid device entry.")
        instance_id = device["PNPDeviceID"]
        match = target.fullmatch(instance_id)
        if match is None:
            continue
        device_class = device.get("PNPClass")
        problem = device.get("ConfigManagerErrorCode")
        usb_parent = device.get("ParentInstanceId")
        compatible = device.get("CompatibleID")
        if compatible is None:
            compatible = []
        if (
            (device_class is not None and not isinstance(device_class, str))
            or (problem is not None and (type(problem) is not int or not 0 <= problem <= 0xFFFFFFFF))
            or (usb_parent is not None and not isinstance(usb_parent, str))
            or not isinstance(compatible, list)
            or not all(isinstance(value, str) for value in compatible)
        ):
            raise ProbeError("Windows returned invalid USB driver data.")

        if match.group("interface") is None:
            owner_id = instance_id.casefold()
            parents[owner_id] = problem
        else:
            owner_id = usb_parent.casefold() if usb_parent else None
        matching_nodes.append((owner_id, device_class, problem, compatible))

    wpd_nodes = 0
    mtp_advertised = False
    healthy_wpd = False
    healthy_mtp = False
    problem_codes = set()
    for owner_id, device_class, problem, compatible in matching_nodes:
        # Preserve diagnostics when Windows sees only interface rows.
        if parents and owner_id not in parents:
            continue
        if problem:
            problem_codes.add(problem)

        advertised = any(value.split("&", 1)[0].upper() == r"USB\MS_COMP_MTP" for value in compatible)
        mtp_advertised = mtp_advertised or advertised
        if (device_class or "").upper() == "WPD":
            wpd_nodes += 1
            healthy_wpd = healthy_wpd or problem == 0
            healthy_mtp = healthy_mtp or (advertised and problem == 0)

    parent_id = next(iter(parents)) if len(parents) == 1 else None
    healthy_endpoint = healthy_mtp if mtp_advertised else healthy_wpd
    if not matching_nodes:
        state = "not_present"
    elif len(parents) > 1:
        state = "ambiguous"
    elif parent_id is not None and parents[parent_id] == 0 and healthy_endpoint:
        state = "wpd_ready"
    elif problem_codes:
        state = "driver_problem"
    else:
        state = "usb_only"
    return Snapshot(state, len(matching_nodes), wpd_nodes, mtp_advertised, tuple(sorted(problem_codes)), parent_id)


def read_windows(usb_id, timeout=QUERY_TIMEOUT):
    system_root = os.environ.get("SystemRoot")
    if os.name != "nt" or not system_root:
        raise ProbeError("Windows PowerShell and local Plug and Play support are required.")
    powershell = Path(system_root) / "System32" / "WindowsPowerShell" / "v1.0" / "powershell.exe"
    try:
        result = subprocess.run(
            [str(powershell), "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", PNP_QUERY],
            env={**os.environ, "EXODUS_MTP_USB_ID": usb_id},
            capture_output=True,
            timeout=timeout,
            check=True,
        )
    except subprocess.TimeoutExpired as exc:
        raise ProbeTimeout("Windows USB enumeration timed out.") from exc
    except (OSError, subprocess.CalledProcessError) as exc:
        raise ProbeError("Windows USB enumeration failed.") from exc
    try:
        devices = json.loads(result.stdout.decode("utf-8-sig"))
    except ValueError as exc:
        raise ProbeError("Windows returned unreadable USB enumeration data.") from exc
    return classify(devices, usb_id)


def check_reconnect(usb_id, initial, seconds, sample):
    if initial.state != "wpd_ready":
        return "baseline_not_ready", initial, False

    print("Disconnect the selected device, wait a few seconds, then reconnect to the same port.", file=sys.stderr)
    deadline = time.monotonic() + seconds
    last = initial
    disconnected = False
    while deadline - time.monotonic() > POLL_INTERVAL:
        time.sleep(POLL_INTERVAL)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        try:
            last = sample(usb_id, timeout=min(remaining, QUERY_TIMEOUT))
        except ProbeTimeout:
            if remaining > QUERY_TIMEOUT:
                raise
            break
        if last.state == "ambiguous" or (last.parent_id is not None and last.parent_id != initial.parent_id):
            return "target_changed", last, disconnected
        if last.state == "not_present":
            disconnected = True
        if disconnected and last.state == "wpd_ready" and (last.mtp_advertised or not initial.mtp_advertised):
            return "reconnected", last, True
    return "timed_out", last, disconnected


def observe(usb_id, initial, seconds, sample):
    """Observe state transitions even when the USB target starts absent.

    Do not require the same instance ID: changing ports may legitimately change
    it. Report only whether it changed, never the ID itself.
    """
    start = time.monotonic()
    deadline = start + seconds
    events = [{"elapsed_ms": 0, **initial.report(), "parent_changed": False}]
    previous = initial
    last_parent = initial.parent_id
    while deadline - time.monotonic() > POLL_INTERVAL:
        time.sleep(POLL_INTERVAL)
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            break
        try:
            current = sample(usb_id, timeout=min(remaining, QUERY_TIMEOUT))
        except ProbeTimeout:
            if remaining > QUERY_TIMEOUT:
                raise
            return {"events": events, "complete": False, "reason": "query_timeout"}
        changed = (current.parent_id is not None and last_parent is not None
                   and current.parent_id != last_parent)
        if current.parent_id is not None:
            last_parent = current.parent_id
        if current.report() != previous.report() or changed:
            events.append({"elapsed_ms": int((time.monotonic() - start) * 1000),
                           **current.report(), "parent_changed": changed})
        previous = current
    return {"events": events, "complete": True, "reason": "window_finished"}


def main(argv=None, sample=read_windows):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--usb-id", required=True, metavar="VID:PID", help="four hexadecimal digits per field")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--reconnect", type=int, metavar="SECONDS", help="observe a disconnect and reconnect in 5–300 seconds")
    mode.add_argument("--observe", type=int, metavar="SECONDS", help="record state transitions from any baseline for 5–300 seconds")
    args = parser.parse_args(argv)
    if USB_ID.fullmatch(args.usb_id) is None:
        parser.error("--usb-id must have the form VID:PID using four hexadecimal digits per field")
    if args.reconnect is not None and not 5 <= args.reconnect <= 300:
        parser.error("--reconnect must be between 5 and 300 seconds")

    if args.observe is not None and not 5 <= args.observe <= 300:
        parser.error("--observe must be between 5 and 300 seconds")

    usb_id = args.usb_id.lower()
    reconnect = None
    observation = None
    try:
        initial = sample(usb_id)
        success = initial.state == "wpd_ready"
        if args.reconnect is not None:
            state, last, disconnected = check_reconnect(usb_id, initial, args.reconnect, sample)
            reconnect = {"state": state, "disconnect_seen": disconnected, "last": last.report()}
            success = state == "reconnected"
        elif args.observe is not None:
            observation = observe(usb_id, initial, args.observe, sample)
            success = observation["complete"]
    except ProbeError as exc:
        print(f"MTP probe: {exc}", file=sys.stderr)
        return 2

    report = {"schema_version": 1, "usb_id": usb_id, "initial": initial.report(), "reconnect": reconnect}
    if observation is not None:
        report["observation"] = observation
    print(json.dumps(report))
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
