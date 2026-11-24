"""Explicit, recoverable copy into Games; never installs or removes sources."""
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import stat
import sys

from .evcs_import_plan import plan, inventory, regular

CHUNK = 1024 * 1024


def digest(path):
    before = regular(path)
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(CHUNK), b''):
            h.update(block)
    after = regular(path)
    if fingerprint(before) != fingerprint(after):
        raise ValueError('file changed while hashing')
    return h.hexdigest()


def fingerprint(st):
    return [st.st_dev, st.st_ino, st.st_size, st.st_mtime_ns, st.st_ctime_ns]


def sync_dir(path):
    if sys.platform.startswith('linux'):
        fd = os.open(path, os.O_RDONLY | os.O_DIRECTORY)
        try:
            os.fsync(fd)
        finally:
            os.close(fd)


def publish(source, dest):
    # Never silently substitute POSIX rename(), which can overwrite a file.
    if os.name == 'nt':
        os.rename(source, dest)
    elif sys.platform.startswith('linux'):
        fn = getattr(ctypes.CDLL(None, use_errno=True), 'renameat2', None)
        if fn is None:
            raise OSError('no atomic no-replace rename available')
        fn.argtypes = [ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p, ctypes.c_uint]
        fn.restype = ctypes.c_int
        if fn(-100, os.fsencode(source), -100, os.fsencode(dest), 1):
            err = ctypes.get_errno()
            raise OSError(err, os.strerror(err), str(dest))
    else:
        raise OSError('no supported no-replace publication on this platform')


def write_copy(src, dst, expected):
    h = hashlib.sha256()
    with src.open('rb') as f, dst.open('xb') as out:
        for block in iter(lambda: f.read(CHUNK), b''):
            h.update(block)
            out.write(block)
        out.flush()
        os.fsync(out.fileno())
    if h.hexdigest() != expected or digest(dst) != expected:
        raise ValueError('copy verification failed')


def transfer(source, games, fat32=False, resume=False):
    # Stable, owner-controlled paths and a single writer are prerequisites.
    report = plan(source, games, fat32)
    src, dest = Path(report['source']), Path(report['destination'])
    root = Path(games).resolve(strict=True)
    if report['status'] == 'already_discoverable':
        return {'status': 'already_discoverable', 'destination': str(dest)}
    if report['status'] not in {'copy_preflight_ok', 'insufficient_copy_space', 'destination_exists'}:
        raise ValueError(report['status'])
    if Path(str(src) + '.evcs').exists() or Path(str(src) + '.evcs').is_symlink():
        raise ValueError('sidecar transfer not supported; keep source and metadata together')
    split = src.is_dir()
    infos = inventory(src)
    names = [f'{i:02d}' for i in range(len(infos))] if split else ['payload']
    sources = [src / n for n in names] if split else [src]
    journal = {'version': 1, 'source': str(src), 'destination': str(dest), 'split': split,
               'parts': [[n, fingerprint(st), digest(p)] for n, st, p in zip(names, infos, sources)]}
    token = hashlib.sha256((str(src) + '\0' + str(dest)).encode('utf-8')).hexdigest()[:32]
    stage = root / ('.evcs-stage-' + token)
    meta = stage / 'journal.json'
    payload = stage / 'payload'
    if stage.exists() or stage.is_symlink():
        if not resume or not stat.S_ISDIR(stage.lstat().st_mode):
            raise ValueError('staging exists; explicit --resume required')
        if regular(meta).st_size > 65536 or json.loads(meta.read_text(encoding='utf-8')) != journal:
            raise ValueError('staging journal mismatch; no changes made')
    else:
        if report['status'] != 'copy_preflight_ok':
            raise ValueError(report['status'])
        if resume:
            raise ValueError('no transaction to resume')
        stage.mkdir(mode=0o700)
        with meta.open('x', encoding='utf-8') as f:
            json.dump(journal, f)
            f.flush()
            os.fsync(f.fileno())
        sync_dir(stage)
        sync_dir(root)
    def verify(target):
        if split:
            if not stat.S_ISDIR(target.lstat().st_mode) or sorted(p.name for p in target.iterdir()) != names:
                raise ValueError('split destination differs')
        for n, _, expected in journal['parts']:
            if digest(target / n if split else target) != expected:
                raise ValueError('destination verification failed')
    if dest.exists() or dest.is_symlink():
        if not resume or payload.exists() or payload.is_symlink():
            raise FileExistsError(str(dest))
        verify(dest)
        sync_dir(root)
        return {'status': 'published_verified', 'destination': str(dest), 'source_retained': True}
    if any(p.name.casefold() == dest.name.casefold() for p in root.iterdir()):
        raise FileExistsError('case-insensitive destination collision')
    if split:
        if not payload.exists():
            payload.mkdir()
        if not stat.S_ISDIR(payload.lstat().st_mode) or any(p.name not in names for p in payload.iterdir()):
            raise ValueError('unexpected staging payload')
    for src_part, (n, original, expected) in zip(sources, journal['parts']):
        dst = payload / n if split else payload
        if dst.exists() or dst.is_symlink():
            # Only a journal-bound regular staging file can be discarded on explicit resume.
            if not resume or not stat.S_ISREG(dst.lstat().st_mode):
                raise ValueError('unexpected staging file')
            if dst.stat().st_size and digest(dst) == expected:
                continue
            dst.unlink()
        if fingerprint(regular(src_part)) != original:
            raise ValueError('source changed')
        write_copy(src_part, dst, expected)
    verify(payload)
    if [fingerprint(regular(p)) for p in sources] != [p[1] for p in journal['parts']]:
        raise ValueError('source changed before publication')
    if any(p.name.casefold() == dest.name.casefold() for p in root.iterdir()):
        raise FileExistsError('case-insensitive destination collision')
    if split:
        sync_dir(payload)
    sync_dir(stage)
    publish(payload, dest)
    sync_dir(root)
    sync_dir(stage)
    return {'status': 'published_verified', 'destination': str(dest), 'source_retained': True}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('games', type=Path)
    p.add_argument('--fat32', action='store_true')
    p.add_argument('--execute', action='store_true', help='explicitly approve copy; source retained')
    p.add_argument('--resume', action='store_true', help='recover matching staged transaction')
    args = p.parse_args()
    if args.resume and not args.execute:
        p.error('--resume requires --execute')
    try:
        result = (transfer(args.source, args.games, args.fat32, args.resume) if args.execute
                  else plan(args.source, args.games, args.fat32))
    except (OSError, ValueError) as exc:
        p.exit(2, f'transfer stopped: {exc}\n')
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
