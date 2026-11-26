"""Reference Task Center contract; no scheduler, device access or process control.

All observations must come from authenticated backend/mediator adapters.
This is a serialized in-memory model, not an authorization or hardware boundary.
"""
from dataclasses import dataclass
import time

BACKENDS = frozenset({"horizon", "freebsd"})
STATES = frozenset({"running", "suspended", "starting", "exited", "unknown"})
UNKNOWN = "unknown"
OPERATIONS = frozenset({"suspend", "resume", "terminate"})
TRANSITIONS = {"suspend": ("running", "suspended"),
               "resume": ("suspended", "running"),
               "terminate": ("running", "exited")}
MAX_SESSIONS = 256
LIMIT = (1 << 64) - 1


def positive(value):
    return type(value) is int and 0 < value <= LIMIT


def valid_key(key):
    return (type(key) is tuple and len(key) == 4 and type(key[0]) is str
            and key[0] in BACKENDS and positive(key[1]) and type(key[2]) is str
            and 1 <= len(key[2]) <= 128
            and not any(ord(ch) < 32 or ord(ch) == 127 for ch in key[2])
            and positive(key[3]))


@dataclass(frozen=True)
class Session:
    backend: str
    epoch: int
    id: str
    instance: int
    state: str
    capabilities: frozenset = frozenset()

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


@dataclass(frozen=True)
class Lifecycle:
    token: int
    operation: str
    target: tuple
    sequence: int
    deadline: float
    accepted: bool = False


class TaskCenter:
    """One foreground ownership bundle: GPU, display and HID together."""
    def __init__(self, clock=time.monotonic):
        self.clock = clock
        self.snapshots = {}
        self.owner = UNKNOWN
        self.pending = None
        self.token = 0
        self.lifecycle = None
        self.lifecycle_result = None
        self.uncertain_lifecycle = None
        self.uncertain_handoff = None

    def _session(self, key, now):
        if not valid_key(key):
            return None
        view = self.snapshots.get(key[0])
        if view and now < view[2]:
            return next((s for s in view[3] if s.key == key), None)
        return None

    def _live(self, key, now):
        session = self._session(key, now)
        return session is not None and session.state == "running"

    def _finish_lifecycle(self, result):
        self.lifecycle_result = (self.lifecycle.token, result)
        if result == UNKNOWN:
            self.uncertain_lifecycle = self.lifecycle
        self.lifecycle = None

    def _check_lifecycle(self, now):
        p = self.lifecycle
        if not p:
            return
        s = self._session(p.target, now)
        if now >= p.deadline or s is None or s.state == UNKNOWN or self.owner == UNKNOWN:
            self._finish_lifecycle(UNKNOWN)
            return
        if not p.accepted:
            return
        view = self.snapshots[s.backend]
        if view[1] > p.sequence and s.state == TRANSITIONS[p.operation][1]:
            self._finish_lifecycle("confirmed")
        elif s.state == "exited":
            self._finish_lifecycle(UNKNOWN)

    def _invalidate(self):
        if self.pending is not None:
            self.uncertain_handoff = self.pending
        self.owner = UNKNOWN
        self.pending = None

    def _tick(self, now):
        p = self.pending
        if p and (now >= p.deadline or not self._live(p.target, now)):
            self._invalidate()
        if self.owner not in (None, UNKNOWN) and not self._live(self.owner, now):
            self._invalidate()
        self._check_lifecycle(now)

    def publish(self, backend, epoch, sequence, sessions):
        now = self.clock()
        self._tick(now)
        if type(backend) is not str or backend not in BACKENDS or not positive(epoch) or not positive(sequence):
            raise ValueError("invalid backend generation")
        old = self.snapshots.get(backend)
        if old and (epoch < old[0] or
                    (epoch == old[0] and (sequence <= old[1] or now >= old[2]))):
            raise ValueError("stale snapshot; expired backend requires a new epoch")
        if not isinstance(sessions, (list, tuple)) or len(sessions) > MAX_SESSIONS:
            raise ValueError("invalid snapshot size")
        ids = set()
        for s in sessions:
            if (not isinstance(s, Session) or not valid_key(s.key) or s.backend != backend or s.epoch != epoch
                    or type(s.epoch) is not int or not positive(s.instance)
                    or not isinstance(s.id, str) or not 1 <= len(s.id) <= 128
                    or any(ord(ch) < 32 or ord(ch) == 127 for ch in s.id)
                    or type(s.state) is not str or s.state not in STATES or s.id in ids
                    or not isinstance(s.capabilities, frozenset)
                    or not s.capabilities <= OPERATIONS):
                raise ValueError("invalid or duplicate session")
            ids.add(s.id)
        self.snapshots[backend] = (epoch, sequence, now + 3, tuple(sessions))
        self._tick(now)

    def disconnect(self, backend):
        if type(backend) is not str or backend not in BACKENDS:
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
        if self.pending or self.lifecycle or self.uncertain_handoff:
            raise ValueError("operation pending or handoff not quiescent")
        if observed_owner is not None and not self._live(observed_owner, now):
            raise ValueError("owner must be a live session or verified released")
        self.owner = observed_owner

    def begin(self, target):
        now = self.clock()
        self._tick(now)
        if (self.pending or self.lifecycle or self.uncertain_lifecycle or self.uncertain_handoff
                or self.owner == UNKNOWN or not self._live(target, now)):
            raise ValueError("handoff unavailable")
        if target == self.owner:
            raise ValueError("already foreground")
        if self.token == LIMIT:
            raise ValueError("token exhausted")
        self.token += 1
        self.pending = Handoff(self.token, "acquire" if self.owner is None else "release",
                               self.owner, target, now + 2)
        return self.pending

    def request_lifecycle(self, target, operation):
        """Return a typed request; never execute or optimistically change state."""
        now = self.clock()
        self._tick(now)
        s = self._session(target, now)
        if (self.pending or self.lifecycle or self.uncertain_lifecycle or self.uncertain_handoff
                or self.owner == UNKNOWN or self.owner == target
                or s is None or type(operation) is not str or operation not in OPERATIONS or operation not in s.capabilities):
            raise ValueError("lifecycle unavailable")
        initial = TRANSITIONS[operation][0]
        if s.state != initial and not (operation == "terminate" and s.state == "suspended"):
            raise ValueError("invalid lifecycle transition")
        if self.token == LIMIT:
            raise ValueError("token exhausted")
        self.token += 1
        self.lifecycle_result = None
        self.lifecycle = Lifecycle(self.token, operation, s.key, self.snapshots[s.backend][1], now + 5)
        return self.lifecycle

    def resolve_uncertain_lifecycle(self, token, target, *, quiesced):
        """Trusted adapter attests that the exact request cannot execute later.

        A timeout, snapshot, epoch change or owner reconciliation is not that
        attestation. This does not claim that the requested operation succeeded.
        """
        self._tick(self.clock())
        p = self.uncertain_lifecycle
        if (not p or type(token) is not int or token != p.token
                or not valid_key(target) or target != p.target or quiesced is not True):
            raise ValueError("unresolved lifecycle operation")
        self.uncertain_lifecycle = None

    def acknowledge_lifecycle(self, token, target, accepted):
        """Backend acceptance is not execution evidence. Require later snapshot."""
        now = self.clock()
        self._tick(now)
        p = self.lifecycle
        if (not p or type(token) is not int or token != p.token or not valid_key(target) or target != p.target
                or type(accepted) is not bool or p.accepted):
            raise ValueError("unexpected lifecycle acknowledgment")
        if not accepted:
            self._finish_lifecycle("rejected")
            return
        # A snapshot predating acceptance cannot confirm the command.
        self.lifecycle = Lifecycle(p.token, p.operation, p.target,
                                   self.snapshots[p.target[0]][1], p.deadline, True)

    def resolve_uncertain_handoff(self, token, phase, target, *, quiesced):
        """Mediator attests the exact timed-out phase cannot execute later.

        Ownership stays unknown until a subsequent fresh reconciliation.
        """
        self._tick(self.clock())
        p = self.uncertain_handoff
        if (not p or type(token) is not int or token != p.token
                or type(phase) is not str or phase != p.phase
                or not valid_key(target) or target != p.target or quiesced is not True):
            raise ValueError("unresolved handoff")
        self.uncertain_handoff = None

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
        if (expected is None and observed_owner is not None) or (expected is not None and
                (not valid_key(observed_owner) or observed_owner != expected)):
            self._invalidate()
            raise ValueError("ownership observation mismatch")
        if phase == "release":
            self.owner = None
            self.pending = Handoff(p.token, "acquire", p.source, p.target, p.deadline)
            return self.pending
        self.owner = p.target
        self.pending = None
        return None
