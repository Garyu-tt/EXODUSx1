"""One-connection server boundary for a trusted local EXODUS backend adapter."""
import json
import re

from .steam_bridge import MAX_FRAME, BridgeError
from .steam_compat import invocation, unique


def serve_connection(conn, adapter=None, request_timeout=5):
    """Adapter yields observed (state, exit_code) events; absent adapter rejects.

    Listener ownership, concurrency and actual backend lifecycle are caller duties.
    No default adapter, shell execution, remote endpoint or automatic retries.
    """
    conn.settimeout(request_timeout)
    with conn.makefile('rb') as stream:
        raw = stream.readline(MAX_FRAME + 1)
        if not raw.endswith(b'\n') or len(raw) > MAX_FRAME:
            raise BridgeError('invalid request frame')
        try:
            req = json.loads(raw, object_pairs_hook=unique)
            if (not isinstance(req, dict) or set(req) != {'version', 'request_id', 'backend', 'kind', 'id', 'verb', 'requires_confirmation'}
                    or type(req['version']) is not int or req['version'] != 1
                    or not isinstance(req['request_id'], str) or not re.fullmatch('[0-9a-f]{32}', req['request_id'])
                    or req['backend'] != 'horizon' or req['requires_confirmation'] is not True):
                raise ValueError('invalid request schema')
            target = {'version': 1, 'backend': req['backend'], 'kind': req['kind'], 'id': req['id']}
            plan = invocation('exodus-horizon', req['verb'], target)
            if plan['argv'][-1] != req['id']:
                raise ValueError('noncanonical target')
        except (ValueError, TypeError, RecursionError) as exc:
            raise BridgeError('request rejected before adapter') from exc

    def send(state, code=None):
        event = {'version': 1, 'request_id': req['request_id'], 'state': state}
        if state == 'exited':
            event['exit_code'] = code
        conn.sendall(json.dumps(event).encode('utf-8') + b'\n')

    if adapter is None:
        send('rejected')
        return
    transitions = {None: {'accepted', 'rejected', 'canceled', 'failed'},
                   'accepted': {'running', 'canceled', 'failed'},
                   'running': {'exited', 'failed'}}
    state = None
    # Do not synthesize accepted/running/exited based on an adapter return value.
    # Only events explicitly observed by the trusted adapter can be forwarded.
    try:
        for next_state, code in adapter(plan):
            if (not isinstance(next_state, str) or next_state not in transitions.get(state, set())
                    or (next_state == 'exited' and (type(code) is not int or not 0 <= code <= 255))
                    or (next_state != 'exited' and code is not None)):
                raise BridgeError('invalid adapter lifecycle event')
            send(next_state, code)
            state = next_state
            if state in {'exited', 'rejected', 'canceled', 'failed'}:
                return
    except OSError:
        raise
    except Exception as exc:
        if 'failed' in transitions.get(state, set()):
            send('failed')
        raise BridgeError('adapter failed') from exc
    if 'failed' in transitions.get(state, set()):
        send('failed')
    raise BridgeError('adapter ended without terminal event')
