"""Reference-only Tumor partition admission model; never accesses hardware."""
from dataclasses import dataclass
import unittest

PAGE = 4096
MAX_PHYS = 1 << 64
DOMAINS = frozenset({"horizon", "freebsd", "mediator"})
DEVICES = frozenset({"gpu", "display", "hid", "wifi", "sd"})
@dataclass(frozen=True)
class Region:
    start: int
    size: int
    owner: str
    shared: bool = False

def admit(ram, regions, cores, devices, available_cores):
    """Reject inconsistent abstract plans, not a hardware safety certificate."""
    def span(start, size):
        if type(start) is not int or type(size) is not int:
            raise ValueError("non-integer address")
        if start < 0 or size <= 0 or start % PAGE or size % PAGE or start + size > MAX_PHYS:
            raise ValueError("invalid physical span")
        return start, start + size
    banks = sorted(span(*b) for b in ram)
    if not banks or any(a[1] > b[0] for a, b in zip(banks, banks[1:])):
        raise ValueError("invalid RAM banks")
    seen = set()
    occupied = []
    for r in regions:
        lo, hi = span(r.start, r.size)
        if type(r.shared) is not bool or r.owner not in DOMAINS:
            raise ValueError("invalid owner")
        if r.shared and r.owner != "mediator":
            raise ValueError("shared memory must be mediated")
        if not any(a <= lo and hi <= b for a, b in banks):
            raise ValueError("outside available RAM")
        occupied.append((lo, hi))
        if not r.shared:
            seen.add(r.owner)
    occupied.sort()
    if any(a[1] > b[0] for a, b in zip(occupied, occupied[1:])):
        raise ValueError("overlap")
    if seen != DOMAINS:
        raise ValueError("missing private domain")
    if set(cores) != DOMAINS:
        raise ValueError("missing CPU domain")
    topology = tuple(available_cores)
    if (not topology or any(type(i) is not int or i < 0 for i in topology)
            or len(set(topology)) != len(topology)):
        raise ValueError("invalid CPU topology")
    assigned = set()
    for owner, ids in cores.items():
        # EL2 mediation can execute on guest pCPUs; no dedicated core required.
        if (owner != "mediator" and not ids) or any(type(i) is not int or i not in topology for i in ids):
            raise ValueError("invalid CPU assignment")
        for i in ids:
            if i in assigned:
                raise ValueError("duplicate CPU")
            assigned.add(i)
    if set(devices) != DEVICES:
        raise ValueError("incomplete device ownership")
    for owner in devices.values():
        if not isinstance(owner, str) or owner not in DOMAINS:
            raise ValueError("invalid device ownership")
    return True

# Symbolic identities, never hardware CPU numbers or physical addresses.
AMP_A57 = frozenset(f"a57:{i}" for i in range(4))
AMP_ADSP = "adsp:a9"
AMP_DEVICES = DEVICES | {"audio"}

def admit_amp(ram, regions, cores, devices):
    """Validate an abstract AMP allocation, not target availability or safety."""
    if set(cores) != DOMAINS:
        raise ValueError("missing AMP domain")
    requested = {}
    for owner, ids in cores.items():
        ids = tuple(ids)
        if any(type(i) is not str for i in ids) or len(set(ids)) != len(ids):
            raise ValueError("invalid AMP CPU identity")
        requested[owner] = frozenset(ids)
    if (requested["horizon"] != AMP_A57 or
            requested["freebsd"] != {AMP_ADSP} or requested["mediator"]):
        raise ValueError("AMP requires all A57 for Horizon, ADSP for FreeBSD")
    if set(devices) != AMP_DEVICES or devices["audio"] != "horizon":
        raise ValueError("AMP audio ownership must remain with Horizon")
    # Require explicit mediated shared RAM for the IPC transport. The existing
    # reference validator checks disjoint spans, reservations and device owners.
    regions = tuple(regions)
    if not any(r.shared is True and r.owner == "mediator" for r in regions):
        raise ValueError("missing AMP shared region")
    numbering = {name: i for i, name in enumerate(sorted(AMP_A57 | {AMP_ADSP}))}
    return admit(ram, regions,
                 {owner: [numbering[i] for i in ids]
                  for owner, ids in requested.items()},
                 {name: devices[name] for name in DEVICES},
                 tuple(numbering.values()))

class ContractTests(unittest.TestCase):
    def setUp(self):
        self.ram = [(0, 16 * PAGE)]
        self.regions = [Region(0, 4 * PAGE, "horizon"),
                        Region(4 * PAGE, 4 * PAGE, "freebsd"),
                        Region(8 * PAGE, PAGE, "mediator"),
                        Region(9 * PAGE, PAGE, "mediator", True)]
        self.cores = {"horizon": [0, 1], "freebsd": [2], "mediator": [3]}
        self.devices = dict.fromkeys(DEVICES, "horizon")
        self.available_cores = (0, 1, 2, 3)
    def check(self):
        return admit(self.ram, self.regions, self.cores, self.devices, self.available_cores)
    def test_valid(self):
        self.assertTrue(self.check())
    def test_overlap(self):
        self.regions.append(Region(PAGE, PAGE, "freebsd"))
        with self.assertRaises(ValueError): self.check()
    def test_outside(self):
        self.regions.append(Region(16 * PAGE, PAGE, "mediator"))
        with self.assertRaises(ValueError): self.check()
    def test_alignment(self):
        self.regions.append(Region(10 * PAGE + 1, PAGE, "mediator"))
        with self.assertRaises(ValueError): self.check()
    def test_overflow(self):
        self.regions.append(Region(MAX_PHYS - PAGE, 2 * PAGE, "mediator"))
        with self.assertRaises(ValueError): self.check()
    def test_shared_owner(self):
        self.regions.append(Region(10 * PAGE, PAGE, "horizon", True))
        with self.assertRaises(ValueError): self.check()
    def test_no_private_mediator(self):
        del self.regions[2]
        with self.assertRaises(ValueError): self.check()
    def test_duplicate_cpu(self):
        self.cores["freebsd"] = [0]
        with self.assertRaises(ValueError): self.check()
    def test_missing_cpu(self):
        self.cores["freebsd"] = []
        with self.assertRaises(ValueError): self.check()
    def test_no_dedicated_mediator_core(self):
        self.cores = {"horizon": [0, 1, 2], "freebsd": [3], "mediator": []}
        self.assertTrue(self.check())
    def test_nonexistent_cpu(self):
        self.cores["freebsd"] = [4]
        with self.assertRaises(ValueError): self.check()
    def test_duplicate_topology(self):
        self.available_cores = (0, 1, 2, 3, 3)
        with self.assertRaises(ValueError): self.check()
    def test_boolean_cpu(self):
        self.cores["freebsd"] = [True]
        with self.assertRaises(ValueError): self.check()
    def test_device_double_owner(self):
        self.devices["gpu"] = ["horizon", "freebsd"]
        with self.assertRaises(ValueError): self.check()
    def test_missing_device(self):
        del self.devices["sd"]
        with self.assertRaises(ValueError): self.check()
    def test_ram_hole(self):
        self.ram = [(0, 2 * PAGE), (4 * PAGE, 12 * PAGE)]
        with self.assertRaises(ValueError): self.check()


class AMPContractTests(unittest.TestCase):
    def setUp(self):
        ContractTests.setUp(self)
        self.cores = {"horizon": sorted(AMP_A57), "freebsd": [AMP_ADSP],
                      "mediator": []}
        self.devices = dict.fromkeys(AMP_DEVICES, "horizon")
    def check_amp(self):
        return admit_amp(self.ram, self.regions, self.cores, self.devices)
    def test_amp_valid(self):
        self.assertTrue(self.check_amp())
    def test_amp_a57_stolen(self):
        self.cores["freebsd"] = [self.cores["horizon"].pop()]
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_missing_a57(self):
        self.cores["horizon"].pop()
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_a53_deferred(self):
        self.cores["freebsd"] = ["a53:0"]
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_duplicate_identity(self):
        self.cores["horizon"].append("a57:0")
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_mediator_dedicated_core(self):
        self.cores["mediator"] = ["a57:0"]
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_audio_owner(self):
        self.devices["audio"] = "freebsd"
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_audio_missing(self):
        del self.devices["audio"]
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_shared_missing(self):
        self.regions.pop()
        with self.assertRaises(ValueError): self.check_amp()
    def test_amp_overlap(self):
        self.regions.append(Region(PAGE, PAGE, "freebsd"))
        with self.assertRaises(ValueError): self.check_amp()

if __name__ == "__main__":
    unittest.main(verbosity=2)
