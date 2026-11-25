"""Export offline native handoff descriptors, not installed Steam shortcuts."""
import argparse
import json
import re
from pathlib import Path

MAX_BYTES = 2 * 1024 * 1024
MAX_ENTRIES = 4096


def descriptor(provider, ident):
    if not isinstance(ident, str):
        return None
    if provider == 'switch':
        if not re.fullmatch(r'[0-9a-fA-F]{16}', ident) or not int(ident, 16):
            return None
        target = ident.lower()
        route = 'switch'
    elif provider == 'switch_homebrew':
        prefix = 'sdmc:/switch/'
        relative = ident
        try:
            length = len((prefix + relative).encode('utf-8'))
        except UnicodeError:
            return None
        if (length >= 512 or len(relative) <= 4 or not relative.lower().endswith('.nro')
                or any(p in ('', '.', '..') for p in relative.split('/'))
                or any(ord(c) < 32 or ord(c) == 127 or c in '\\:"' for c in relative)):
            return None
        target = ident
        route = 'homebrew'
    else:
        return None
    return {'key': f'native:{route}:{target}', 'argv': ['--native-target', route, target],
            'requires_confirmation': True, 'launch_status': 'unverified'}


def export(path):
    def unique(pairs):
        result = {}
        for k, v in pairs:
            if k in result:
                raise ValueError('duplicate JSON field')
            result[k] = v
        return result

    path = Path(path)
    if not path.is_file():
        raise ValueError('expected a regular catalog file')
    with path.open('rb') as stream:
        raw = stream.read(MAX_BYTES + 1)
    if len(raw) > MAX_BYTES:
        raise ValueError('catalog exceeds byte limit')
    data = json.loads(raw, object_pairs_hook=unique)
    if (not isinstance(data, dict) or set(data) != {'schema_version', 'entries'}
            or type(data['schema_version']) is not int or data['schema_version'] != 1
            or not isinstance(data['entries'], list) or len(data['entries']) > MAX_ENTRIES):
        raise ValueError('invalid catalog')
    result = {}
    identities = set()
    required_text = {'provider', 'provider_id', 'name', 'installation_state', 'compatibility_state'}
    nullable_text = {'artwork', 'launch_mode'}
    for entry in data['entries']:
        if (not isinstance(entry, dict) or set(entry) != required_text | nullable_text | {'last_played'}
                or any(not isinstance(entry[k], str) or not entry[k] for k in required_text)
                or any(entry[k] is not None and (not isinstance(entry[k], str) or not entry[k]) for k in nullable_text)
                or (entry['last_played'] is not None and
                    (type(entry['last_played']) is not int or not 0 <= entry['last_played'] <= 2**64 - 1))
                or '\0' in entry['provider_id']):
            raise ValueError('invalid catalog entry')
        identity = (entry['provider'], entry['provider_id'])
        if identity in identities:
            raise ValueError('duplicate catalog identity')
        identities.add(identity)
        target = descriptor(entry.get('provider'), entry.get('provider_id'))
        if target is not None:
            result.setdefault(target['key'], target)
    return {'schema_version': 1, 'descriptors': list(result.values())}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalog', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(export(args.catalog), ensure_ascii=True, indent=2))
    except (OSError, ValueError, RecursionError) as exc:
        parser.exit(2, f'export failed: {exc}\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
