"""Reference Task Center contract; no scheduler, device access or process control.

All observations must come from authenticated backend/mediator adapters.
This is a serialized in-memory model, not an authorization or hardware boundary.
"""
from dataclasses import dataclass
import time

BACKENDS = frozenset({"horizon", "freebsd"})
STATES = frozenset({"running", "suspended", "starting", "exited", "unknown"})
UNKNOWN = "unknown"
MAX_SESSIONS = 256
LIMIT = (1 << 64) - 1


def positive(value):
    return type(value) is int and 0 < value <= LIMIT


@dataclass(frozen=True)
class Session:
    backend: str
    epoch: int
    id: str
    instance: int
    state: str

    @property
    def key(self):
        return (self.backend, self.epoch, self.id, self.instance)


@dataclass(frozen=True)
class Handoff:
    token: int
    phase: str
    source: object
    target: tuple
    deadline: float


class TaskCenter:
    """One foreground ownership bundle: GPU, display and HID together."""
    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.snapshots = {}
        self.owner = UNKNOWN
        self.pending = None
        self.token = 0

    def _live(self, key, now):
        if not isinstance(key, tuple) or len(key) != 4:
            return False
        view = self.snapshots.get(key[0])
        return bool(view and now < view[2] and
                    any(s.key == key and s.state == "running" for s in view[3]))

    def _invalidate(self):
        self.owner = UNKNOWN
        self.pending = None

    def _tick(self, now):
        p = self.pending
        if p and (now >= p.deadline or not self._live(p.target, now)):
            self._invalidate()
        if self.owner not in (None, UNKNOWN) and not self._live(self.owner, now):
            self._invalidate()

    def publish(self, backend, epoch, sequence, sessions):
        now = self.clock()
        self._tick(now)
        if backend not in BACKENDS or not positive(epoch) or not positive(sequence):
            raise ValueError("invalid backend generation")
        old = self.snapshots.get(backend)
        if old and (epoch < old[0] or
                    (epoch == old[0] and (sequence <= old[1] or now >= old[2]))):
            raise ValueError("stale snapshot; expired backend requires a new epoch")
        if not isinstance(sessions, (list, tuple)) or len(sessions) > MAX_SESSIONS:
            raise ValueError("invalid snapshot size")
        ids = set()
        for s in sessions:
            if (not isinstance(s, Session) or s.backend != backend or s.epoch != epoch
                    or type(s.epoch) is not int or not positive(s.instance)
                    or not isinstance(s.id, str) or not 1 <= len(s.id) <= 128
                    or any(ord(ch) < 32 or ord(ch) == 127 for ch in s.id)
                    or s.state not in STATES or s.id in ids):
                raise ValueError("invalid or duplicate session")
            ids.add(s.id)
        self.snapshots[backend] = (epoch, sequence, now + 3, tuple(sessions))
        self._tick(now)

    def disconnect(self, backend):
        if backend not in BACKENDS:
            raise ValueError("invalid backend")
        now = self.clock()
        if backend in self.snapshots:
            epoch, seq, _, rows = self.snapshots[backend]
            self.snapshots[backend] = (epoch, seq, now, rows)
        self._tick(now)

    def view(self):
        now = self.clock()
        self._tick(now)
        return [{"key": s.key, "state": s.state if now < until else UNKNOWN}
                for _, _, until, rows in self.snapshots.values() for s in rows]

    def reconcile(self, observed_owner):
        """Trusted mediator's fresh, exclusive ownership observation.

        None means positively observed release of the entire bundle.
        Unknown is not equivalent to released.
        """
        now = self.clock()
        self._tick(now)
        if self.pending:
            raise ValueError("handoff pending")
        if observed_owner is not None and not self._live(observed_owner, now):
            raise ValueError("owner must be a live session or verified released")
        self.owner = observed_owner

    def begin(self, target):
        now = self.clock()
        self._tick(now)
        if self.pending or self.owner == UNKNOWN or not self._live(target, now):
            raise ValueError("handoff unavailable")
        if target == self.owner:
            raise ValueError("already foreground")
        if self.token == LIMIT:
            raise ValueError("token exhausted")
        self.token += 1
        self.pending = Handoff(self.token, "acquire" if self.owner is None else "release",
                               self.owner, target, now + 2)
        return self.pending

    def acknowledge(self, token, phase, observed_owner):
        """Only mediator-confirmed observations advance the handoff.

        Never infer release from a stopped process or a disconnected backend.
        No retry, implicit grant or rollback on an uncertain transition.
        """
        now = self.clock()
        self._tick(now)
        p = self.pending
        if not p or type(token) is not int or token != p.token or phase != p.phase:
            raise ValueError("stale or unexpected acknowledgment")
        expected = None if phase == "release" else p.target
        if observed_owner != expected:
            self._invalidate()
            raise ValueError("ownership observation mismatch")
        if phase == "release":
            self.owner = None
            self.pending = Handoff(p.token, "acquire", p.source, p.target, p.deadline)
            return self.pending
        self.owner = p.target
        self.pending = None
        return None
