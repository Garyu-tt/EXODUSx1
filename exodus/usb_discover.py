"""Discover USB vendor and product IDs that appear after attachment on Windows."""

import argparse
from collections import Counter
import ctypes
import json
import os
import re
import sys
import time

from .mtp_probe import POLL_INTERVAL, ProbeError


CM_GETIDLIST_FILTER_ENUMERATOR = 0x1
CM_GETIDLIST_FILTER_PRESENT = 0x100
CR_SUCCESS = 0
CR_BUFFER_SMALL = 0x1A
DEVICE_ID = re.compile(r"USB\\VID_([0-9A-F]{4})&PID_([0-9A-F]{4})\\[^\\]+", re.IGNORECASE)


def count_usb_ids(instances):
    counts = Counter()
    for instance in instances:
        match = DEVICE_ID.fullmatch(instance)
        if match:
            vendor, product = match.groups()
            counts[f"{vendor.lower()}:{product.lower()}"] += 1
    return counts


def read_usb_ids():
    if os.name != "nt":
        raise ProbeError("Windows Plug and Play support is required.")
    try:
        config = ctypes.WinDLL("CfgMgr32.dll")
    except OSError as exc:
        raise ProbeError("Windows Plug and Play support is unavailable.") from exc

    get_size = config.CM_Get_Device_ID_List_SizeW
    get_size.argtypes = (ctypes.POINTER(ctypes.c_ulong), ctypes.c_wchar_p, ctypes.c_ulong)
    get_size.restype = ctypes.c_ulong
    get_ids = config.CM_Get_Device_ID_ListW
    get_ids.argtypes = (ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_wchar), ctypes.c_ulong, ctypes.c_ulong)
    get_ids.restype = ctypes.c_ulong
    flags = CM_GETIDLIST_FILTER_ENUMERATOR | CM_GETIDLIST_FILTER_PRESENT
    for _ in range(3):
        size = ctypes.c_ulong()
        status = get_size(ctypes.byref(size), "USB", flags)
        if status != CR_SUCCESS:
            break
        buffer = ctypes.create_unicode_buffer(size.value)
        status = get_ids("USB", buffer, size.value, flags)
        if status == CR_SUCCESS:
            return count_usb_ids(buffer[:].split("\0"))
        if status != CR_BUFFER_SMALL:
            break
        # A new device can use the space reserved between the size and list calls.
    raise ProbeError(f"Windows USB enumeration failed (CONFIGRET 0x{status:02x}).")


def discover(seconds, scan):
    baseline = scan()
    print("Connect the target USB device now. Keep other USB connections unchanged.", file=sys.stderr)
    start = time.monotonic()
    deadline = start + seconds
    candidates = {}
    error = None
    while (remaining := deadline - time.monotonic()) > 0:
        time.sleep(min(POLL_INTERVAL, remaining))
        if time.monotonic() >= deadline:
            break
        try:
            current = scan()
        except ProbeError as exc:
            error = exc
            break
        sampled_at = time.monotonic()
        if sampled_at >= deadline:
            break
        elapsed_ms = int((sampled_at - start) * 1000)
        for usb_id in sorted(current - baseline):
            candidates.setdefault(usb_id, elapsed_ms)

    return {
        "schema_version": 1,
        "complete": error is None,
        "candidates": [{"usb_id": usb_id, "first_seen_ms": elapsed} for usb_id, elapsed in candidates.items()],
    }, error


def main(argv=None, scan=read_usb_ids):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seconds", type=int, default=15, help="observation window of 5 to 300 seconds (default 15)")
    args = parser.parse_args(argv)
    if not 5 <= args.seconds <= 300:
        parser.error("--seconds must be between 5 and 300")

    try:
        report, error = discover(args.seconds, scan)
    except ProbeError as exc:
        print(f"USB discovery: {exc}", file=sys.stderr)
        return 2
    print(json.dumps(report))
    if error is not None:
        print(f"USB discovery: {error}", file=sys.stderr)
        return 2
    return 0 if report["candidates"] else 1


if __name__ == "__main__":
    sys.exit(main())
