import unittest

from exodus.corkdeck_catalog import parse_catalog


class CorkDeckCatalogTests(unittest.TestCase):
    def test_v3_metadata(self):
        entries = parse_catalog(
            "version=3\nnext-id=2\n[game 1]\n"
            "path=sdmc:/switch/wine/A%25B.exe\n"
            "title=Game%3DOne\nportrait-art=sdmc:/art.png\n"
            "launched-order=12\nfavorite=1\n"
        )
        self.assertEqual(entries, [{
            "provider": "corkdeck",
            "provider_id": "sdmc:/switch/wine/A%B.exe",
            "name": "Game=One",
            "artwork": "sdmc:/art.png",
            "installation_state": "unknown",
            "last_played": None,
            "compatibility_state": "unknown",
            "launch_mode": None,
        }])

    def test_v2_fallback(self):
        entries = parse_catalog(
            "version=2\n[game 9]\npath=ums0:/Wine/Example.exe\n"
            "landscape-art=sdmc:/art.jpg\n"
        )
        self.assertEqual(entries[0]["name"], "Example.exe")
        self.assertEqual(entries[0]["artwork"], "sdmc:/art.jpg")

    def test_reject_invalid(self):
        for text in (
            "version=4\n",
            "version=3\n[game 1]\ntitle=No path\n",
            "version=3\n[game 1]\npath=A%ZZ\n",
            "version=3\n[game 1]\npath=A%00\n",
            "version=3\n[game 1]\npath=A\npath=B\n",
            "version=3\n[game 1]\npath=A\n[game 1]\npath=B\n",
            "version=3\n[game 1]\npath=A\n[game 2]\npath=a\n",
        ):
            with self.subTest(text=text), self.assertRaises(ValueError):
                parse_catalog(text)

    def test_empty_and_limits(self):
        self.assertEqual(parse_catalog("version=3\n"), [])
        too_many = "version=3\n" + "".join(
            f"[game {i}]\npath={i}.exe\n" for i in range(1, 258)
        )
        with self.assertRaises(ValueError):
            parse_catalog(too_many)
        with self.assertRaises(ValueError):
            parse_catalog("version=3\n[game 1]\npath=" + "a" * 512)


if __name__ == "__main__":
    unittest.main()
