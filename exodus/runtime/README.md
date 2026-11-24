# EXODUS Windows Runtime

The Switch app finds Autorun on the SD card and opens its launcher through the Homebrew Menu loader. It checks the NRO header, image length and segment ranges before scheduling the launch. Autorun handles its own setup and Windows application compatibility.

## Build and use

Build with devkitPro's Switch toolchain, libnx and the Switch tools installed.

```sh
make -C exodus/runtime/switch
```

1. Install an official Autorun release at the SD card root. Its launcher is `switch/wine/wine-nx-runtime.nro`. [Autorun releases](https://github.com/autorunhq/autorun/releases)
2. Copy `exodus/runtime/switch/exodus-windows-runtime.nro` to `switch/exodus-windows-runtime/exodus-windows-runtime.nro` on the SD card.
3. Open EXODUS Windows Runtime from a Homebrew Menu running in application mode. Press **A** to open Autorun, **X** to rescan, or **B** or **+** to exit.

If the installed NRO is missing, unreadable or has an invalid header or segment layout, the app reports the problem alongside the path. The launch action requires application mode and a loader that supports scheduling another homebrew app.

## Validation

Run the host tests with a C compiler, GNU Make and Python 3.

```sh
make -C exodus/runtime check
```

On Switch, verify that **A** opens an installed Autorun launcher in application mode. Also start EXODUS Runtime in applet mode and with Autorun absent to check the corresponding diagnostics. These device checks are pending.
