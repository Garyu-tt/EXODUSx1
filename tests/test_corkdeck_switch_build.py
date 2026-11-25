import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


REPO = Path(__file__).resolve().parents[1]
SWITCH_BUILD = REPO / "exodus/home/corkdeck/build-switch.sh"
TOOLS = ("sh", "cc", "c++", "ar", "pkg-config", "python3")


def write(path, content, executable=False):
    path.write_text(content, encoding="utf-8")
    if executable:
        path.chmod(0o755)


@unittest.skipUnless(sys.platform.startswith("linux") and all(shutil.which(tool) for tool in TOOLS),
                     "requires the Linux host C/C++ build toolchain and pkg-config")
class CorkdeckSwitchBuildTests(unittest.TestCase):
    def test_switch_pkg_config_preserves_paths_and_static_dependencies(self):
        with tempfile.TemporaryDirectory(prefix="corkdeck switch build ") as directory:
            root = Path(directory)
            shell = root / "exodus/home/corkdeck"
            catalog = root / "exodus/home/ulaunch"
            runtime = root / "exodus/runtime"
            donor = root / "donor"
            donor_source = donor / "horizon-wine/source"
            sdk = root / "fixture sdk"
            compilers = sdk / "devkitA64/bin"
            tools = sdk / "tools/bin"
            nx = sdk / "libnx"
            pc = sdk / "portlibs/switch/lib/pkgconfig"
            dependencies = root / "switch dependencies"
            include = dependencies / "include"
            libraries = dependencies / "lib"
            headers = root / "json include"
            for path in (shell, catalog, runtime, donor_source, compilers, tools,
                         nx / "lib", pc, include, libraries, headers):
                path.mkdir(parents=True)
            (nx / "switch.specs").touch()
            (headers / "json.hpp").touch()
            script = shell / SWITCH_BUILD.name
            shutil.copyfile(SWITCH_BUILD, script)
            write(shell / "prepare-donor.py",
                  "import shutil, sys\nshutil.copytree(sys.argv[1], sys.argv[2])\n")
            # The fixture exercises the build command rather than the production C/C++ sources.
            for directory in (shell, catalog, runtime):
                source_dir = REPO / directory.relative_to(root)
                for source in (*source_dir.glob("*.c"), *source_dir.glob("*.cpp")):
                    (directory / source.name).touch()
            for name in ("ui", "audio", "svg", "image"):
                (donor_source / f"launcher_{name}.c").touch()
            write(shell / "settings_preview.c",
                  "#include <fixture.h>\nint measured(void) { return dependency() - 7; }\n")
            write(shell / "library_catalog.cpp",
                  'extern "C" int measured(void);\nint main() { return measured(); }\n')
            write(include / "fixture.h", "int dependency(void);\n")
            source = root / "dependency.c"
            write(source, "int dependency(void) { return 7; }\n")
            obj = root / "dependency.o"
            subprocess.run(["cc", "-c", source, "-o", obj], check=True, timeout=30)
            subprocess.run(["ar", "rcs", libraries / "libfixture.a", obj], check=True, timeout=30)
            subprocess.run(["ar", "rcs", nx / "lib/libnx.a"], check=True, timeout=30)
            for package in ("sdl2", "SDL2_ttf", "libpng", "libturbojpeg"):
                flags = (
                    'Cflags: -I"${prefix}/include"\nLibs.private: -L"${prefix}/lib" -lfixture\n'
                    if package == "sdl2" else ""
                )
                write(pc / f"{package}.pc", f"prefix={dependencies}\nName: {package}\n"
                      f"Description: local build fixture\nVersion: 1\n{flags}")
            # Use real compiler drivers and remove only ARM/Switch target flags in this host fixture.
            for tool in ("gcc", "g++"):
                target = "c++" if tool == "g++" else "cc"
                write(compilers / f"aarch64-none-elf-{tool}",
                      "#!/usr/bin/env python3\n"
                      "import os, sys\n"
                      "prefixes = ('-march=', '-mtune=', '-mtp=', '-specs=')\n"
                      f"os.execvp({target!r}, [{target!r}, *[arg for arg in sys.argv[1:]"
                      " if not arg.startswith(prefixes)]])\n", executable=True)
            write(tools / "nacptool", '#!/bin/sh\n: > "$5"\n', executable=True)
            write(tools / "elf2nro", '#!/bin/sh\ncp "$1" "$2"\n', executable=True)

            env = {**os.environ, "DEVKITPRO": str(sdk), "DEVKITA64": str(sdk / "devkitA64"),
                   "JSON_INCLUDE": str(headers), "PYTHON": sys.executable,
                   "TMPDIR": directory, "SWITCH_EXTRA_LIBS": ""}
            output = root / "output files"
            built = subprocess.run(["sh", script, donor, output], env=env, capture_output=True,
                                   text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            self.assertTrue((output / "exodus-shell.nro").is_file())
            subprocess.run([output / "exodus-shell.elf"], check=True, timeout=15)


if __name__ == "__main__":
    unittest.main()
