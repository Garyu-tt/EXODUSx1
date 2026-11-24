#!/usr/bin/env python3
"""Regression gate for the actual exposed SteamOS Manager interface subset.

Contract source: evlav/steamos-manager@a0d399defebd60db30ba38325d9cf47dbda49bee,
steamos-manager-proxy/src/{session_management1,manager2}.rs.
This tests signatures only, not graphical session implementation.
"""
import os
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

BUS = "com.steampowered.SteamOSManager1"
PATH = "/com/steampowered/SteamOSManager1"
EXPECTED = {
    BUS + ".SessionManagement1": {
        "CleanTemporarySessions": ("method", "", ""),
        "SwitchToDesktopMode": ("method", "", ""),
        "SwitchToDesktopSession": ("method", "s", ""),
        "SwitchToGameMode": ("method", "", ""),
        "SwitchToLoginMode": ("method", "s", ""),
        "ValidDesktopSessions": ("method", "", "as"),
        "DefaultDesktopSession": ("property", "s", "readwrite"),
        "DefaultLoginMode": ("property", "s", "readwrite"),
    },
    BUS + ".Manager2": {
        "ReloadConfig": ("method", "", ""),
        "DeviceModel": ("property", "(ss)", "read"),
    },
}

def signatures(xml):
    found = {}
    for iface in ET.fromstring(xml).findall("interface"):
        name = iface.attrib["name"]
        if name not in EXPECTED:
            continue
        assert name not in found, name
        members = found[name] = {}
        for elem in iface:
            if elem.tag not in ("method", "property"):
                continue
            key = elem.attrib["name"]
            assert key not in members, key
            if elem.tag == "property":
                members[key] = ("property", elem.attrib["type"], elem.attrib["access"])
            else:
                args = elem.findall("arg")
                members[key] = (
                    "method",
                    "".join(a.attrib["type"] for a in args if a.get("direction", "in") == "in"),
                    "".join(a.attrib["type"] for a in args if a.get("direction") == "out"),
                )
    return found

def main(binary):
    assert os.environ.get("DBUS_SESSION_BUS_ADDRESS"), "use dbus-run-session"
    with tempfile.TemporaryDirectory() as tmp:
        cfg = pathlib.Path(tmp) / "config"
        cfg.write_text("generation=1\nmode=game\n")
        cfg.chmod(0o600)
        with subprocess.Popen([binary, str(cfg)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT) as proc:
            try:
                subprocess.run(["gdbus", "wait", "--session", "--timeout", "15", BUS], check=True, timeout=20)
                xml = subprocess.check_output(
                    ["gdbus", "introspect", "--session", "--xml", "--dest", BUS, "--object-path", PATH],
                    text=True, timeout=10,
                )
                actual = signatures(xml)
                if actual != EXPECTED:
                    raise AssertionError(f"contract mismatch: {actual!r}")
                print("PASS: 10 original SteamOS Manager method/property signatures")
            finally:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        proc.kill()
                        proc.wait()
if __name__ == "__main__":
    main(sys.argv[1])
