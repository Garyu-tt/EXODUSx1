"""Local Steam-to-EXODUS launch transport; requires a trusted native bridge."""
import argparse
import json
import math
import os
from pathlib import Path
import secrets
import socket
import stat
import time

from .steam_compat import invocation, read_target, unique

MAX_FRAME = 4096


class BridgeError(RuntimeError):
    pass


def check_socket(path):
    if not hasattr(os, 'getuid') or not hasattr(socket, 'AF_UNIX'):
        raise BridgeError('local POSIX bridge unsupported')
    path = Path(path)
    if not path.is_absolute() or path.resolve(strict=True) != path:
        raise BridgeError('bridge socket must have a canonical absolute path')
    parent = path.parent.stat()
    endpoint = path.lstat()
    if (parent.st_uid != os.getuid() or parent.st_mode & 0o077
            or not stat.S_ISSOCK(endpoint.st_mode) or endpoint.st_uid != os.getuid()
            or endpoint.st_mode & 0o077):
        raise BridgeError('bridge endpoint must be owner-only')
    return str(path)


def exchange(path, tool, verb, target, timeout=3600):
    plan = invocation(tool, verb, target)
    if not math.isfinite(timeout) or not 0 < timeout <= 86400:
        raise ValueError('invalid bridge timeout')
    endpoint = check_socket(path)
    request_id = secrets.token_hex(16)
    request = {'version': 1, 'request_id': request_id, 'backend': plan['backend'],
               'kind': plan['target_kind'], 'id': plan['argv'][-1], 'verb': verb,
               'requires_confirmation': True}
    deadline = time.monotonic() + timeout
    state = None
    transitions = {None: {'accepted', 'rejected', 'canceled', 'failed'},
                   'accepted': {'running', 'canceled', 'failed'},
                   'running': {'exited', 'failed'}}
    pending = bytearray()

    def set_deadline(conn):
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise BridgeError('bridge deadline exceeded; target state unknown')
        conn.settimeout(remaining)

    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as conn:
            set_deadline(conn)
            conn.connect(endpoint)
            set_deadline(conn)
            conn.sendall(json.dumps(request, ensure_ascii=False).encode('utf-8') + b'\n')
            while True:
                while b'\n' not in pending:
                    if len(pending) >= MAX_FRAME:
                        raise BridgeError('response exceeds frame limit')
                    set_deadline(conn)
                    chunk = conn.recv(MAX_FRAME - len(pending))
                    if not chunk:
                        raise BridgeError('bridge disconnected; target state unknown')
                    pending.extend(chunk)
                line, _, rest = pending.partition(b'\n')
                pending = bytearray(rest)
                event = json.loads(line, object_pairs_hook=unique)
                if (not isinstance(event, dict) or type(event.get('version')) is not int
                        or event['version'] != 1 or event.get('request_id') != request_id
                        or not isinstance(event.get('state'), str)
                        or event['state'] not in transitions.get(state, set())):
                    raise BridgeError('invalid bridge event or lifecycle transition')
                state = event['state']
                fields = {'version', 'request_id', 'state'}
                if state == 'exited':
                    fields.add('exit_code')
                    code = event.get('exit_code')
                    if type(code) is not int or not 0 <= code <= 255:
                        raise BridgeError('invalid exit code')
                if set(event) != fields:
                    raise BridgeError('unexpected bridge event fields')
                if state == 'exited':
                    return {'status': 'exited', 'exit_code': code}
                if state in {'rejected', 'canceled', 'failed'}:
                    return {'status': state, 'exit_code': 3 if state == 'canceled' else 1}
    except (OSError, ValueError, RecursionError) as exc:
        raise BridgeError('bridge transport failed; target state unknown') from exc


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket', required=True, type=Path)
    parser.add_argument('--timeout', type=float, default=3600)
    parser.add_argument('tool')
    parser.add_argument('verb')
    parser.add_argument('target', type=Path)
    args = parser.parse_args()
    try:
        result = exchange(args.socket, args.tool, args.verb, read_target(args.target), args.timeout)
    except (BridgeError, OSError, ValueError, RecursionError) as exc:
        parser.exit(2, f'bridge stopped: {exc}\n')
    print(json.dumps(result))
    return result['exit_code']


if __name__ == '__main__':
    raise SystemExit(main())
