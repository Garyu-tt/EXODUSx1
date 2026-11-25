"""Verify a local upstream archive against the reviewed release lock, read-only."""
import argparse
import hashlib
import hmac
import json
from pathlib import Path
import re
import sys

LOCK = Path(__file__).with_name('upstream') / '4ifir.lock.json'


def verify(path, asset):
    size = asset['bytes']
    digest = asset['sha256']
    if type(size) is not int or not 0 < size <= 4 * 1024**3:
        raise ValueError('invalid locked size')
    if not isinstance(digest, str) or not re.fullmatch('[0-9a-f]{64}', digest):
        raise ValueError('invalid locked SHA-256')
    if not path.is_file():
        raise ValueError('expected a regular archive file')
    hashed = hashlib.sha256()
    count = 0
    with path.open('rb') as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            count += len(block)
            if count > size:
                raise ValueError('archive exceeds locked size')
            hashed.update(block)
    if count != size or not hmac.compare_digest(hashed.hexdigest(), digest):
        raise ValueError('archive does not match locked size and digest')
    return {'bytes': count, 'sha256': hashed.hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    args = parser.parse_args()
    try:
        lock = json.loads(LOCK.read_text(encoding='utf-8'))
        if lock['schema_version'] != 1:
            raise ValueError('unsupported lock schema')
        result = verify(args.archive, lock['asset'])
        print(json.dumps({'tag': lock['tag'], **result}, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
