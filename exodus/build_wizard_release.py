"""Build the archived 4IFIR Wizard package used by the legacy updater."""

import argparse
from datetime import datetime
from io import BytesIO
from pathlib import Path
import re
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


PAYLOAD_DIR = Path("Old files/4ifir Wizard source code")
WRAPPER_DIR = Path("Old files/4IFIR-Wizard-update/4IFIR Wizard")
VERSIONED_FILES = {"config.ini", "4Advanced/Full Assist/inis/Start.ini"}
VERSION_LINE = re.compile(rb"^;version='[0-9.]*'(?=\r?$)", re.MULTILINE)


def _write_entry(archive, name, timestamp, data=b""):
    entry = ZipInfo(name, timestamp)
    entry.create_system = 3
    if entry.is_dir():
        entry.external_attr = (0o40755 << 16) | 0x10
        archive.writestr(entry, data)
    else:
        entry.external_attr = 0o100644 << 16
        archive.writestr(entry, data, compress_type=ZIP_DEFLATED)


def _build_payload(source, version, timestamp):
    if not source.is_dir() or source.is_symlink():
        raise ValueError(f"missing Wizard payload directory {source}")

    version_line = f";version='{version}'".encode("ascii")
    remaining = VERSIONED_FILES.copy()
    payload = BytesIO()
    with ZipFile(payload, "w") as archive:
        paths = sorted(source.rglob("*"), key=lambda path: path.relative_to(source).as_posix())
        for path in paths:
            name = path.relative_to(source).as_posix()
            if path.is_symlink():
                raise ValueError(f"unexpected symlink in Wizard payload {name}")
            if path.is_dir():
                _write_entry(archive, name + "/", timestamp)
                continue
            if not path.is_file():
                raise ValueError(f"unexpected entry in Wizard payload {name}")

            data = path.read_bytes()
            if name in remaining:
                data, count = VERSION_LINE.subn(version_line, data)
                if count != 1:
                    raise ValueError(f"expected one version marker in {name}, found {count}")
                remaining.remove(name)
            _write_entry(archive, name, timestamp, data)

    if remaining:
        raise ValueError(f"missing Wizard payload files {', '.join(sorted(remaining))}")
    return payload.getvalue()


def build_wizard_release(repo_root: Path, output_dir: Path, version: str) -> Path:
    if not re.fullmatch(r"[0-9]{2}(?:\.[0-9]{2}){3}", version):
        raise ValueError("version must use UTC format YY.MM.DD.HH")
    year, month, day, hour = map(int, version.split("."))
    timestamp = datetime(2000 + year, month, day, hour).timetuple()[:6]

    payload = _build_payload(repo_root / PAYLOAD_DIR, version, timestamp)
    wrapper = repo_root / WRAPPER_DIR
    output_dir.mkdir(parents=True, exist_ok=True)
    destination = output_dir / f"4IFIR-Wizard-update-{version}.zip"

    with TemporaryDirectory(dir=output_dir) as staging:
        staged_archive = Path(staging) / destination.name
        with ZipFile(staged_archive, "w") as archive:
            prefix = "4IFIR Wizard/"
            _write_entry(archive, prefix, timestamp)
            for name in ("config.ini", "init.ini"):
                path = wrapper / name
                if not path.is_file() or path.is_symlink() or wrapper.is_symlink():
                    raise ValueError(f"missing Wizard wrapper file {name}")
                _write_entry(archive, prefix + name, timestamp, path.read_bytes())
            _write_entry(archive, prefix + "files.zip", timestamp, payload)
        staged_archive.replace(destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True, help="package version in UTC YY.MM.DD.HH format")
    parser.add_argument("--output-dir", required=True, type=Path, help="destination directory for the ZIP")
    args = parser.parse_args()

    try:
        print(build_wizard_release(Path(__file__).resolve().parents[1], args.output_dir, args.version))
    except (OSError, ValueError) as error:
        parser.exit(1, f"{parser.prog}: {error}\n")


if __name__ == "__main__":
    main()
