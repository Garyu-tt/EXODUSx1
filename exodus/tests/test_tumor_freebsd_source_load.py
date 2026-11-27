import hashlib
import struct
import unittest
from tumor_freebsd_source_load import plan, EH, PH, LINK, LOAD

def fixture():
    data=bytearray(0x2004)
    ident=b"\x7fELF\x01\x01\x01"+bytes(9)
    EH.pack_into(data,0,ident,2,40,1,LINK+0x1000,EH.size,0,0,EH.size,PH.size,3,0,0,0)
    PH.pack_into(data,EH.size,1,0,LINK,LINK,0x100,0x100,4,0x1000)
    PH.pack_into(data,EH.size+PH.size,1,0x1000,LINK+0x1000,LINK+0x1000,4,4,5,0x1000)
    PH.pack_into(data,EH.size+2*PH.size,1,0x2000,LINK+0x2000,LINK+0x2000,4,0x1000,6,0x1000)
    data[0x1000:0x1004]=b"CODE"
    data[0x2000:0x2004]=b"DATA"
    return data

def check(data, ram=32):
    return plan(data,hashlib.sha256(data).hexdigest(),ram)

class LoaderTests(unittest.TestCase):
    def test_dynamic_entry_bss(self):
        entry,image,segs=check(fixture())
        self.assertEqual(entry,LOAD+0x1000)
        self.assertEqual(image[0x1000:0x1004],b"CODE")
        self.assertEqual(image[0x2000:0x2004],b"DATA")
        self.assertEqual(image[0x2004:],bytes(0xffc))
        self.assertEqual(len(segs),3)
    def test_digest(self):
        with self.assertRaisesRegex(ValueError,"digest mismatch"):
            plan(fixture(),"0"*64,32)
    def test_nonexec_entry(self):
        d=fixture()
        struct.pack_into("<I",d,24,LINK+0x2000)
        with self.assertRaisesRegex(ValueError,"executable segment"):
            check(d)
    def test_wx(self):
        d=fixture()
        struct.pack_into("<I",d,EH.size+PH.size+24,7)
        with self.assertRaisesRegex(ValueError,"permissions"):
            check(d)
    def test_overlap(self):
        d=fixture()
        struct.pack_into("<I",d,EH.size+PH.size+20,0x1004)
        with self.assertRaisesRegex(ValueError,"overlapping"):
            check(d)
    def test_ram_bounds(self):
        d=fixture()
        struct.pack_into("<I",d,EH.size+2*PH.size+20,0x800000)
        with self.assertRaisesRegex(ValueError,"exceeds reference RAM"):
            check(d,24)
    def test_truncated(self):
        with self.assertRaisesRegex(ValueError,"truncated"):
            check(b"ELF")

if __name__=="__main__":
    unittest.main(verbosity=2)
