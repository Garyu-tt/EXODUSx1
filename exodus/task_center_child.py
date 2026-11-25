"""Local POSIX child adapter for Task Center. No arbitrary PID or remote shell.

Single serialized owner and sole waitpid reaper are required. The executable
and argv come from trusted local configuration, never a UI request. This
reference manages one direct child, not a process tree or hardware resources.
"""
import math
import os
import signal
import time

from .task_center import Lifecycle, Session, UNKNOWN, LIMIT, OPERATIONS, positive, valid_key


class ManagedChild:
    def __init__(self, epoch, session_id, instance, argv):
        if (not valid_key(("freebsd", epoch, session_id, instance))
                or not isinstance(argv, (tuple, list)) or not argv
                or not all(isinstance(s, str) and "\0" not in s for s in argv)
                or not os.path.isabs(argv[0])):
            raise ValueError("invalid trusted child configuration")
        if not hasattr(os, "posix_spawn") or not hasattr(os, "WCONTINUED"):
            raise NotImplementedError("POSIX child observation unavailable")
        if signal.getsignal(signal.SIGCHLD) != signal.SIG_DFL:
            raise ValueError("exclusive child reaper required")
        self.key = ("freebsd", epoch, session_id, instance)
        self.sequence = 0
        self.last_token = 0
        self.state = "running"
        self.pid = os.posix_spawn(argv[0], argv, os.environ.copy(),
                                  setsigmask=(), setsigdef=(signal.SIGTERM, signal.SIGINT))
        self.terminal = False

    def poll(self, deadline=None):
        if self.terminal:
            return self.state
        while True:
            # Dispatch must stop observing at its deadline even if waitpid
            # repeatedly reports interruptions or state changes.
            if deadline is not None and time.monotonic() >= deadline:
                break
            try:
                pid, status = os.waitpid(self.pid, os.WNOHANG | os.WUNTRACED | os.WCONTINUED)
            except InterruptedError:
                continue
            except ChildProcessError:
                # A lost reaper contract means we must never signal this PID.
                self.state, self.terminal = UNKNOWN, True
                break
            if pid == 0:
                break
            if os.WIFEXITED(status) or os.WIFSIGNALED(status):
                self.state, self.terminal = "exited", True
                break
            if os.WIFSTOPPED(status):
                self.state = "suspended"
            elif os.WIFCONTINUED(status):
                self.state = "running"
        return self.state

    def capabilities(self):
        if self.state == "running":
            return frozenset({"suspend", "terminate"})
        if self.state == "suspended":
            return frozenset({"resume"})
        return frozenset()

    def snapshot(self):
        self.poll()
        if self.sequence == LIMIT:
            raise ValueError("snapshot sequence exhausted")
        self.sequence += 1
        return self.key[0], self.key[1], self.sequence, [
            Session(*self.key, self.state, self.capabilities())]

    def dispatch(self, request):
        """Synchronous delivery only. True means signal accepted, not completed."""
        if (not isinstance(request, Lifecycle) or not valid_key(request.target)
                or request.target != self.key
                or not positive(request.token) or request.token <= self.last_token
                or request.accepted is not False or not positive(request.sequence)
                or request.sequence != self.sequence
                or type(request.operation) is not str or request.operation not in OPERATIONS
                or type(request.deadline) not in (int, float)
                or not 0 < request.deadline <= LIMIT
                or not math.isfinite(request.deadline)
                or request.deadline <= time.monotonic()):
            return False
        self.last_token = request.token
        self.poll(deadline=request.deadline)
        # poll() can be delayed by waitpid events or interruptions. Recheck
        # expiry after observation, before issuing any process signal.
        if (self.terminal or request.operation not in self.capabilities()
                or request.deadline <= time.monotonic()):
            return False
        sig = {"suspend": signal.SIGSTOP, "resume": signal.SIGCONT,
               "terminate": signal.SIGTERM}[request.operation]
        try:
            # Until waitpid reaps a direct child, its PID cannot be reused.
            # No other thread, handler or library may reap it.
            os.kill(self.pid, sig)
        except ProcessLookupError:
            self.poll(deadline=request.deadline)
            return False
        except PermissionError:
            return False
        return True
