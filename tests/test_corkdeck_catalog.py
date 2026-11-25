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

    def test_last_artwork_alias_wins_in_both_versions(self):
        for version in (2, 3):
            for first, last in (("portrait-art", "landscape-art"),
                                ("landscape-art", "portrait-art")):
                with self.subTest(version=version, last=last):
                    entries = parse_catalog(
                        f"version={version}\n[game 1]\npath=sdmc:/Wine/Game.exe\n"
                        "square-art=sdmc:/square.png\nhero-art=sdmc:/hero.png\n"
                        f"{first}=sdmc:/old.png\n{last}=sdmc:/new%20cover.png\n"
                    )
                    self.assertEqual(entries[0]["artwork"], "sdmc:/new cover.png")

    def test_empty_last_alias_clears_portrait_before_independent_fallbacks(self):
        fallbacks = (
            ("hero-art=sdmc:/hero.png\nsquare-art=sdmc:/square.png\n", "sdmc:/square.png"),
            ("square-art=\nhero-art=sdmc:/hero.png\n", "sdmc:/hero.png"),
            ("", None),
        )
        for version in (2, 3):
            for first, last in (("portrait-art", "landscape-art"),
                                ("landscape-art", "portrait-art")):
                for fields, expected in fallbacks:
                    with self.subTest(version=version, last=last, fallback=expected):
                        entries = parse_catalog(
                            f"version={version}\n[game 1]\npath=sdmc:/Wine/Game.exe\n"
                            f"{fields}{first}=sdmc:/old.png\n{last}=\n"
                        )
                        self.assertEqual(entries[0]["artwork"], expected)

    def test_invalid_superseded_or_unselected_artwork_is_still_rejected(self):
        invalid = ("bad%GG", "bad%FF", "bad%00", "bad%09", "a" * 512)
        for key in ("portrait-art", "landscape-art", "square-art", "hero-art"):
            later_alias = "landscape-art" if key == "portrait-art" else "portrait-art"
            for value in invalid:
                with self.subTest(key=key, value=value), self.assertRaises(ValueError):
                    parse_catalog(
                        "version=3\n[game 1]\npath=sdmc:/Wine/Game.exe\n"
                        f"{key}={value}\n{later_alias}=sdmc:/valid.png\n"
                    )

    def test_alias_resolution_preserves_duplicate_and_unknown_key_handling(self):
        header = "version=3\n[game 1]\npath=sdmc:/Wine/Game.exe\n"
        for key in ("portrait-art", "landscape-art", "square-art", "hero-art", "custom"):
            with self.subTest(key=key), self.assertRaises(ValueError):
                parse_catalog(header + f"{key}=first\n{key}=last\n")
        entries = parse_catalog(header + "custom=ignored%GG\nportrait-art=sdmc:/cover.png\n")
        self.assertEqual(entries[0]["artwork"], "sdmc:/cover.png")
        self.assertNotIn("custom", entries[0])

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
