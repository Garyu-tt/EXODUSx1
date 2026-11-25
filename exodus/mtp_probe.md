# Windows USB and MTP probe

Use this read-only probe to find where the USB enumeration or Windows Portable Devices (WPD) driver path stops. It runs on Windows 10 or 11 with Python 3.8 or newer. The targeted WPD probe also requires Windows PowerShell.

In Device Manager, open the connected device's properties and select **Details** → **Hardware Ids**. Record the four hexadecimal digits following `VID_` and `PID_` while the device is visible. Replace the example values with yours.

If the USB ID is unknown, disconnect the target and run discovery from the repository root. Connect the target when prompted. Keep other devices attached as they were at the start.

```powershell
python -m exodus.usb_discover --seconds 20
```

Discovery records each newly visible VID/PID once, with the time of the first matching Windows sample in milliseconds. If an additional physical device uses a VID/PID that was already present, its ID can also appear. The JSON omits device names, serial numbers and instance IDs. Discovery can miss attachments between samples and includes any other device attached during the window. Exit code `0` means at least one candidate was found, `1` means the window finished with no candidates, and `2` means an argument or enumeration error occurred. If an error occurs after the baseline, the JSON retains the candidates already seen and sets `complete` to `false`.

Use a candidate with the targeted probe to inspect WPD readiness.

A discovery query that returns at or after the observation deadline is excluded from the candidates and reports `complete: false` with exit code `2`. Earlier candidates remain in the JSON. Ordinary expiry while waiting for the next sample still completes normally. Windows Configuration Manager calls are synchronous, so this deadline check does not interrupt a query in progress.

```powershell
python exodus/mtp_probe.py --usb-id 1234:abcd
```

| Initial state | Windows observation |
| --- | --- |
| `not_present` | No present USB nodes match the selected vendor and product IDs. |
| `usb_only` | The matching USB path has yet to expose a healthy WPD node with a healthy parent. |
| `driver_problem` | The matching USB path reports PnP problem codes and lacks a healthy WPD target. |
| `ambiguous` | More than one physical device uses those IDs. Disconnect the other matching devices and repeat. |
| `wpd_ready` | A healthy WPD node and its USB parent are visible. |

Composite USB interfaces contribute to WPD readiness only when Windows links them to a present matching parent. `usb_nodes` counts all matching USB nodes. When a matching parent is present, `wpd_nodes`, `mtp_advertised` and `problem_codes` reflect the parent and its linked interfaces.

`mtp_advertised` records whether Windows saw the `USB\MS_COMP_MTP` compatible ID. WPD also supports PTP and other protocols. Use an Explorer file round trip with a disposable file and compare the original and recovered file hashes to verify actual MTP transfer. `problem_codes` contains the numeric Windows codes for matching USB nodes. The JSON omits serial numbers, device names and instance IDs.

To check for the same Windows USB instance after reconnecting, start with the device attached, wait for the prompt, unplug the cable for a few seconds and reconnect to the same port within the observation window.

```powershell
python exodus/mtp_probe.py --usb-id 1234:abcd --reconnect 45
```

The report includes the initial and last states. `disconnect_seen` records a Windows sample where the original USB parent was absent, including a sample that still lists matching WPD interfaces. A confirmed reconnect requires the same parent and its expected WPD readiness to return. The exit code is `0` for WPD readiness or a confirmed reconnect, `1` when the requested result was unobserved, and `2` for an argument or probe error. JSON goes to stdout. Prompts and probe errors go to stderr.

If Windows enumeration fails after a successful reconnect baseline, the JSON report keeps that baseline, the last successful sample and `disconnect_seen`. The reconnect `state` is `query_failed` or `query_timeout`, and the command exits with `2`. A failed query does not count as a disconnect or a successful recovery. A query that exhausts the remaining window still reports `timed_out` and exits with `1`, retaining the last sample and any observed disconnect. If the baseline query fails, there is no JSON report.

Use `--observe 120` to record changes over two minutes, including when the device starts disconnected. A completed observation exits with `0`. If Windows enumeration fails after the first sample, stdout still contains the initial state and all recorded transitions. The observation has `complete: false` and a `reason` of `query_timeout` or `query_failed`. A query that exhausts the remaining window exits with `1`. A probe error exits with `2` and writes its diagnostic to stderr. If the first sample fails, there is no JSON report.

Both timed modes use only samples returned before the window ends. A result received at or after the deadline counts as a timeout. The report keeps the last reconnect state or observation transitions recorded within the window.

See [Microsoft's PnP codes and device properties](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-pnpentity), [PnP parent property](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-parent) and [supported WPD protocols](https://learn.microsoft.com/en-us/windows-hardware/drivers/portable/wpd-drivers-overview).
