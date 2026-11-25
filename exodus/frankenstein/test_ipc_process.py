"""Real process/Unix socket reference test; not independent kernel evidence."""
import os
import socket
import subprocess
import sys
import time
import unittest
from contextlib import ExitStack
from ipc_contract import FRAME, Peer, heartbeat


def now_ms():
    return time.monotonic_ns() // 1_000_000


def worker(fd, role, generation):
    with socket.socket(fileno=fd) as channel:
        for seq in range(1, 1025):
            channel.sendall(heartbeat(role, generation, seq))
            time.sleep(0.05)


def spawn(stack, role, generation):
    parent, child = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
    stack.enter_context(parent)
    with child:
        proc = subprocess.Popen([sys.executable, __file__, '--worker',
                                 str(child.fileno()), str(role), str(generation)],
                                pass_fds=(child.fileno(),))
    def cleanup():
        if proc.poll() is None:
            proc.kill()
        proc.wait(timeout=5)
    stack.callback(cleanup)
    parent.settimeout(2)
    return proc, parent, Peer(role, generation, now_ms())


def collect(channel, peer):
    # Extra byte makes oversized packets fail the fixed-length check.
    return peer.receive(channel.recv(FRAME.size + 1), now_ms())


class ProcessContractTests(unittest.TestCase):
    def test_independent_process_liveness_and_restart(self):
        with ExitStack() as stack:
            a, ca, pa = spawn(stack, 1, 100)
            b, cb, pb = spawn(stack, 2, 200)
            self.assertNotEqual(a.pid, b.pid)
            self.assertNotIn(os.getpid(), (a.pid, b.pid))
            for _ in range(5):
                self.assertTrue(collect(ca, pa))
                self.assertTrue(collect(cb, pb))
            self.assertTrue(pa.live(now_ms()) and pb.live(now_ms()))
            self.assertIsNone(a.poll())
            self.assertIsNone(b.poll())
            a.kill()
            a.wait(timeout=5)
            # Drain any messages queued before exit; EOF closes the peer.
            for _ in range(1025):
                if not collect(ca, pa):
                    break
            self.assertTrue(pa.closed)
            self.assertTrue(collect(cb, pb))
            self.assertTrue(pb.live(now_ms()))
            self.assertIsNone(b.poll())
            a2, ca2, pa2 = spawn(stack, 1, 101)
            self.assertTrue(collect(ca2, pa2))
            self.assertTrue(collect(cb, pb))
            self.assertTrue(pa2.live(now_ms()) and pb.live(now_ms()))
            self.assertIsNone(a2.poll())
            self.assertFalse(pa.receive(heartbeat(1, 100, pa.sequence + 1), now_ms()))

    def test_oversized_packet_rejected(self):
        a, b = socket.socketpair(socket.AF_UNIX, socket.SOCK_SEQPACKET)
        with a, b:
            p = Peer(1, 42, now_ms())
            b.sendall(heartbeat(1, 42, 1) + b'extra')
            self.assertFalse(collect(a, p))
            self.assertTrue(p.closed)


if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[1] == '--worker':
        worker(*map(int, sys.argv[2:]))
    else:
        unittest.main(verbosity=2)
