import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


HOST_BUILD = Path(__file__).resolve().parents[1] / "exodus/home/corkdeck/build-host.sh"
TOOLS = ("sh", "cc", "c++", "ar", "pkg-config")


@unittest.skipUnless(os.name == "posix" and all(shutil.which(tool) for tool in TOOLS),
                     "requires a POSIX host C/C++ build toolchain and pkg-config")
class CorkdeckHostBuildTests(unittest.TestCase):
    def test_pkg_config_paths_with_spaces(self):
        with tempfile.TemporaryDirectory(prefix="corkdeck host build ") as directory:
            root = Path(directory)
            shell = root / "exodus/home/corkdeck"
            catalog = root / "exodus/home/ulaunch"
            runtime = root / "exodus/runtime"
            donor = root / "donor"
            donor_source = donor / "horizon-wine/source"
            libraries = root / "local dependencies"
            include = libraries / "include"
            lib = libraries / "lib"
            pc = lib / "pkgconfig"
            for path in (shell, catalog, runtime, donor_source, include, pc):
                path.mkdir(parents=True)

            script = shell / HOST_BUILD.name
            shutil.copyfile(HOST_BUILD, script)
            # Exercise the actual compiler and linker without the SDL UI or a network checkout.
            (shell / "prepare-donor.py").write_text(
                "import shutil, sys\nshutil.copytree(sys.argv[1], sys.argv[2])\n", encoding="utf-8",
            )
            for path in (
                shell / "focus_adapter.c", shell / "focus_indicator.c",
                runtime / "autorun_probe.c", catalog / "catalog.cpp",
                *(donor_source / f"launcher_{name}.c" for name in ("ui", "audio", "svg", "image")),
            ):
                path.touch()
            (shell / "settings_preview.c").write_text(
                "#include <fixture.h>\nint measured(void) { return dependency() - 7; }\n", encoding="utf-8",
            )
            (shell / "library_catalog.cpp").write_text(
                'extern "C" int measured(void);\nint main() { return measured(); }\n', encoding="utf-8",
            )
            (include / "fixture.h").write_text("int dependency(void);\n", encoding="utf-8")
            dependency = root / "dependency.c"
            dependency.write_text("int dependency(void) { return 7; }\n", encoding="utf-8")
            obj = root / "dependency.o"
            subprocess.run(["cc", "-c", dependency, "-o", obj], check=True, timeout=30)
            subprocess.run(["ar", "rcs", lib / "libfixture.a", obj], check=True, timeout=30)
            for package in ("sdl2", "SDL2_ttf", "libpng", "libturbojpeg"):
                flags = (
                    'Cflags: -I"${prefix}/include"\nLibs: -L"${prefix}/lib" -lfixture\n'
                    if package == "sdl2" else ""
                )
                (pc / f"{package}.pc").write_text(
                    f"prefix={libraries}\nName: {package}\n"
                    f"Description: local build fixture\nVersion: 1\n{flags}",
                    encoding="utf-8",
                )
            env = {
                **os.environ, "PKG_CONFIG_PATH": "", "PKG_CONFIG_LIBDIR": str(pc),
                "PKG_CONFIG_SYSROOT_DIR": "", "TMPDIR": directory,
                "CC": "cc", "CXX": "c++", "PYTHON": sys.executable,
            }
            target = root / "host executable"
            built = subprocess.run(
                ["sh", script, donor, target], env=env, capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            subprocess.run([target], check=True, timeout=15)


if __name__ == "__main__":
    unittest.main()
