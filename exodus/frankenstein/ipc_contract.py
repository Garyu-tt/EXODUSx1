"""Reference heartbeat contract, not a kernel or shared-memory driver."""
import struct
import unittest

FRAME = struct.Struct('>4sBBHQQ')
LIMIT = (1 << 64) - 1
TIMEOUT_MS = 3000


def heartbeat(role, generation, sequence):
    return FRAME.pack(b'TMR1', 1, role, 0, generation, sequence)


class Peer:
    def __init__(self, role, generation, now):
        if role not in (1, 2) or not 0 < generation <= LIMIT or not 0 <= now <= LIMIT:
            raise ValueError('invalid local session')
        self.role, self.generation = role, generation
        self.last = self.last_clock = now
        self.sequence = 0
        self.closed = False

    def close(self):
        self.closed = True
        return False

    def tick(self, now):
        if self.closed or not self.last_clock <= now <= LIMIT or now - self.last >= TIMEOUT_MS:
            return self.close()
        self.last_clock = now
        return True

    def receive(self, data, now):
        if not self.tick(now) or type(data) is not bytes or len(data) != FRAME.size:
            return self.close()
        magic, version, role, reserved, generation, seq = FRAME.unpack(data)
        if (magic, version, role, reserved, generation) != (b'TMR1', 1, self.role, 0, self.generation):
            return self.close()
        if self.sequence == LIMIT or seq != self.sequence + 1:
            return self.close()
        self.sequence, self.last = seq, now
        return True

    def live(self, now):
        return self.tick(now) and self.sequence != 0


class IPCContractTests(unittest.TestCase):
    def setUp(self):
        self.peer = Peer(1, 9, 100)

    def test_fresh_pair(self):
        other = Peer(2, 10, 100)
        self.assertFalse(self.peer.live(100))
        self.assertTrue(self.peer.receive(heartbeat(1, 9, 1), 101))
        self.assertTrue(other.receive(heartbeat(2, 10, 1), 102))
        self.assertTrue(self.peer.live(103) and other.live(103))
        self.assertTrue(self.peer.receive(heartbeat(1, 9, 2), 2000))
        self.assertFalse(other.live(3102))
        self.assertTrue(self.peer.live(3102))

    def test_invalid_frames(self):
        good = heartbeat(1, 9, 1)
        bad = [good[:-1], good + b'x', b'BAD!' + good[4:],
               good[:4] + b'\x02' + good[5:], good[:7] + b'\x01' + good[8:],
               heartbeat(2, 9, 1), heartbeat(1, 8, 1), heartbeat(1, 9, 0),
               heartbeat(1, 9, 2)]
        for data in bad:
            with self.subTest(data=data):
                p = Peer(1, 9, 100)
                self.assertFalse(p.receive(data, 101))
                self.assertFalse(p.receive(good, 102))

    def test_replay(self):
        data = heartbeat(1, 9, 1)
        self.assertTrue(self.peer.receive(data, 101))
        self.assertFalse(self.peer.receive(data, 102))

    def test_expiry(self):
        self.assertFalse(self.peer.receive(heartbeat(1, 9, 1), 3100))
        self.assertFalse(self.peer.live(3101))

    def test_clock_rollback(self):
        self.assertFalse(self.peer.receive(heartbeat(1, 9, 1), 99))

    def test_clock_rollback_between_polls(self):
        self.assertTrue(self.peer.tick(200))
        self.assertFalse(self.peer.receive(heartbeat(1, 9, 1), 199))

    def test_restart_requires_local_generation(self):
        self.peer.close()
        new = Peer(1, 10, 200)
        self.assertFalse(new.receive(heartbeat(1, 9, 1), 201))
        new = Peer(1, 10, 200)
        self.assertTrue(new.receive(heartbeat(1, 10, 1), 201))

    def test_sequence_exhaustion(self):
        self.peer.sequence = LIMIT
        self.assertFalse(self.peer.receive(heartbeat(1, 9, 0), 101))


if __name__ == '__main__':
    unittest.main(verbosity=2)
