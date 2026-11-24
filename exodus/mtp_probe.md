# Windows USB and MTP probe

Use this read-only probe to find where the USB enumeration or Windows Portable Devices (WPD) driver path stops. It runs on Windows 10 or 11 with Windows PowerShell and Python 3.8 or newer.

In Device Manager, open the connected device's properties and select **Details** → **Hardware Ids**. Record the four hexadecimal digits following `VID_` and `PID_` while the device is visible. Replace the example values with yours.

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

The report includes the initial and last states and whether a disconnect was observed. The exit code is `0` for WPD readiness or a confirmed reconnect, `1` when the requested result was unobserved, and `2` for an argument or probe error. JSON goes to stdout. Prompts and probe errors go to stderr.

See [Microsoft's PnP codes and device properties](https://learn.microsoft.com/en-us/windows/win32/cimwin32prov/win32-pnpentity), [PnP parent property](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/devpkey-device-parent) and [supported WPD protocols](https://learn.microsoft.com/en-us/windows-hardware/drivers/portable/wpd-drivers-overview).
