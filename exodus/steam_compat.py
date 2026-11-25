"""Experimental Steam compatibility-tool protocol; no installation or native launch."""
import argparse
import json
import os
import re
import stat
from pathlib import Path

from .native_descriptors import descriptor

MAX_MARKER_BYTES = 4096
SUPPORTED_VERBS = ('run', 'waitforexitandrun')


def unique(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError('duplicate field')
        result[key] = value
    return result


def validate_target(data):
    if (not isinstance(data, dict) or set(data) != {'version', 'backend', 'kind', 'id'}
            or type(data['version']) is not int or data['version'] != 1
            or any(not isinstance(data[key], str) or not data[key]
                   for key in ('backend', 'kind', 'id'))):
        raise ValueError('invalid target schema')
    return data


def regular_marker(metadata):
    return (stat.S_ISREG(metadata.st_mode)
            and not getattr(metadata, 'st_reparse_tag', 0))


def read_target(path):
    path = Path(path)
    if path.suffix != '.exodus-target':
        raise ValueError('expected a regular .exodus-target descriptor')
    before = path.lstat()
    if not regular_marker(before):
        raise ValueError('expected a regular .exodus-target descriptor')
    # Nonblocking open prevents a substituted FIFO from hanging the caller.
    # O_NOFOLLOW rejects final symlinks where supported; fstat checks the
    # actual opened object before any read. The parent must be trusted.
    flags = os.O_RDONLY | getattr(os, 'O_BINARY', 0) | getattr(os, 'O_NONBLOCK', 0)
    flags |= getattr(os, 'O_NOFOLLOW', 0) | getattr(os, 'O_CLOEXEC', 0)
    fd = os.open(path, flags)
    try:
        actual = os.fstat(fd)
        if not regular_marker(actual) or not os.path.samestat(before, actual):
            raise ValueError('target changed or is not a regular file')
        if actual.st_size > MAX_MARKER_BYTES:
            raise ValueError('target exceeds byte limit')
        with os.fdopen(fd, 'rb', closefd=False) as stream:
            raw = stream.read(MAX_MARKER_BYTES + 1)
    finally:
        os.close(fd)
    if len(raw) > MAX_MARKER_BYTES:
        raise ValueError('target exceeds byte limit')
    return validate_target(json.loads(raw, object_pairs_hook=unique))


def invocation(tool, verb, target):
    """Translate a Steam invocation into a typed local bridge request, never execute."""
    validate_target(target)
    if verb not in SUPPORTED_VERBS:
        raise ValueError('unsupported Steam verb')
    if tool not in ('exodus-horizon', 'exodus-bsd'):
        raise ValueError('unknown compatibility tool')
    expected = 'horizon' if tool == 'exodus-horizon' else 'bsd'
    if target.get('backend') != expected:
        raise ValueError('backend mismatch')
    if expected == 'bsd':
        raise ValueError('BSD execution adapter unavailable')
    kind = target.get('kind')
    provider = 'switch' if kind == 'title_id' else 'switch_homebrew' if kind == 'nro' else None
    desc = descriptor(provider, target.get('id'))
    if desc is None:
        raise ValueError('invalid Horizon target')
    return {'version': 1, 'tool': tool, 'backend': expected, 'verb': verb,
            'target_kind': kind, 'argv': desc['argv'], 'requires_confirmation': True,
            'requires_exit_tracking': verb == 'waitforexitandrun',
            'status': 'bridge_unavailable', 'executed': False}


def candidate_manifests(tool, source_os, target_os):
    """Render candidate VDF, never install it or infer supported OS identifiers."""
    if tool != 'exodus-horizon':
        raise ValueError('tool has no candidate adapter')
    for token in (source_os, target_os):
        if not isinstance(token, str) or not re.fullmatch(r'[a-z][a-z0-9_]{0,31}', token):
            raise ValueError('invalid OS identifier')
    if source_os == 'windows':
        raise ValueError('Horizon is not a Windows compatibility tool')
    compat = ('"compatibilitytools"\n{\n  "compat_tools"\n  {\n'
              '    "exodus-horizon"\n    {\n      "install_path" "."\n'
              '      "display_name" "EXODUS Horizon (experimental)"\n'
              f'      "from_oslist" "{source_os}"\n      "to_oslist" "{target_os}"\n'
              '    }\n  }\n}\n')
    manifest = ('"manifest"\n{\n  "version" "2"\n'
                '  "commandline" "/exodus-compat %verb%"\n}\n')
    return {'status': 'unverified_candidate', 'installable': False,
            'compatibilitytool.vdf': compat, 'toolmanifest.vdf': manifest}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    command = sub.add_parser('plan')
    command.add_argument('tool')
    command.add_argument('verb')
    command.add_argument('target', type=Path)
    manifest = sub.add_parser('manifest-preview')
    manifest.add_argument('tool')
    manifest.add_argument('--source-os', required=True)
    manifest.add_argument('--target-os', required=True)
    args = parser.parse_args()
    try:
        report = (invocation(args.tool, args.verb, read_target(args.target))
                  if args.action == 'plan' else candidate_manifests(args.tool, args.source_os, args.target_os))
    except (OSError, ValueError, RecursionError) as exc:
        parser.exit(2, f'compatibility protocol rejected: {exc}\n')
    print(json.dumps(report, indent=2))
    return 1 if args.action == 'plan' else 0


if __name__ == '__main__':
    raise SystemExit(main())
