"""Owner-only local listener, with no default execution adapter."""
from contextlib import contextmanager
import os
from pathlib import Path
import socket
import stat

from .steam_bridge import BridgeError
from .steam_bridge_server import serve_connection


@contextmanager
def listener(path):
    if not hasattr(os, 'getuid') or not hasattr(socket, 'AF_UNIX'):
        raise BridgeError('POSIX listener unavailable')
    path = Path(path)
    if not path.is_absolute() or path.parent.resolve(strict=True) != path.parent:
        raise BridgeError('listener requires canonical absolute path')
    directory = path.parent.stat()
    if not stat.S_ISDIR(directory.st_mode) or directory.st_uid != os.getuid() or directory.st_mode & 0o077:
        raise BridgeError('listener requires owner-only directory')
    if path.exists() or path.is_symlink():
        raise BridgeError('endpoint exists; no stale socket removal')
    identity = None
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
        try:
            server.bind(str(path))
            info = path.lstat()
            identity = (info.st_dev, info.st_ino)
            path.chmod(0o600)
            server.listen(1)
            yield server
        finally:
            if identity is not None:
                try:
                    info = path.lstat()
                    if stat.S_ISSOCK(info.st_mode) and (info.st_dev, info.st_ino) == identity:
                        path.unlink()
                except FileNotFoundError:
                    pass


def serve_once(server, adapter=None, accept_timeout=10):
    """One session at a time; trusted native adapter is injected by the host."""
    server.settimeout(accept_timeout)
    conn, _ = server.accept()
    with conn:
        serve_connection(conn, adapter)
