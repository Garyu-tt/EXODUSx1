"""Read-only inventory of a Switch SD distribution ZIP; never extracts files."""
import argparse
import hashlib
import json
import stat
import sys
import unicodedata
import zipfile

MAX_FILES = 20000
MAX_BYTES = 4 * 1024**3
MAX_FILE = 2 * 1024**3
CHUNK = 1024**2


def path_key(name):
    parts = name.rstrip('/').split('/')
    for p in parts:
        if (not p or p in ('.', '..') or p.endswith((' ', '.')) or
                any(ord(c) < 32 or c in '\\:<>"|?*' for c in p) or
                p.split('.')[0].upper() in {'CON', 'PRN', 'AUX', 'NUL',
                    *('COM'+str(i) for i in range(1, 10)),
                    *('LPT'+str(i) for i in range(1, 10))}):
            raise ValueError('unsafe archive path: ' + repr(name))
    return '/'.join(unicodedata.normalize('NFC', p).casefold() for p in parts)


def inventory(path):
    records = []
    seen = {}
    total = 0
    with zipfile.ZipFile(path) as archive:
        entries = archive.infolist()
        if len(entries) > MAX_FILES:
            raise ValueError('too many entries')
        for item in entries:
            name = item.orig_filename
            if '\x00' in name:
                raise ValueError('NUL in path')
            key = path_key(name)
            mode = stat.S_IFMT(item.external_attr >> 16)
            if mode not in (0, stat.S_IFREG, stat.S_IFDIR) or (mode == stat.S_IFDIR and not item.is_dir()):
                raise ValueError('unsupported archive entry: ' + repr(name))
            if key in seen:
                raise ValueError('duplicate or case-colliding path: ' + repr(name))
            seen[key] = item.is_dir()
            if item.flag_bits & 1 or item.file_size > MAX_FILE:
                raise ValueError('encrypted or oversized entry')
            total += item.file_size
            if total > MAX_BYTES:
                raise ValueError('archive exceeds inventory budget')
            if item.is_dir():
                if item.file_size:
                    raise ValueError('directory with payload')
                continue
            digest = hashlib.sha256()
            count = 0
            with archive.open(item) as stream:
                while True:
                    block = stream.read(CHUNK)
                    if not block:
                        break
                    count += len(block)
                    if count > item.file_size:
                        raise ValueError('entry exceeds declared size')
                    digest.update(block)
            if count != item.file_size:
                raise ValueError('entry size mismatch')
            records.append({'path': name, 'bytes': count, 'sha256': digest.hexdigest()})
        for key in seen:
            parts = key.split('/')
            for i in range(1, len(parts)):
                if seen.get('/'.join(parts[:i])) is False:
                    raise ValueError('file used as parent directory')
    return {'schema_version': 1, 'files': sorted(records, key=lambda x: x['path']), 'total_bytes': total}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive')
    parser.add_argument('--verify', help='compare against an existing JSON inventory')
    args = parser.parse_args()
    try:
        result = inventory(args.archive)
        if args.verify:
            with open(args.verify, encoding='utf-8') as stream:
                expected = json.load(stream)
            if result != expected:
                print('inventory mismatch', file=sys.stderr)
                return 1
        print(json.dumps(result, sort_keys=True, indent=2, ensure_ascii=True))
        return 0
    except (OSError, ValueError, RuntimeError, NotImplementedError, zipfile.BadZipFile) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
