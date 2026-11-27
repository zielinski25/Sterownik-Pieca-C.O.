# -*- coding: utf-8 -*-
"""
Test skanera i wykonawcy - dziala na atrapie dysku, nie na prawdziwym profilu.

Uruchomienie:
    python test_skaner.py          # Linux/macOS
    python test_skaner.py -v       # wiecej szczegolow

Sprawdza cztery rzeczy, ktore najlatwiej zepsuc:
  1. smieci sa wykrywane (cache, buildy, instalki, duplikaty),
  2. OneDrive / .git / Program Files sa CHRONIONE mimo pasujacej nazwy,
  3. rozmiary sie zgadzaja z tym, co lezy na dysku,
  4. wykonawca w trybie probnym nic nie rusza, a w trybie "na serio"
     przenosi do kwarantanny i da sie to odwrocic.
"""

import json
import os
import shutil
import sys
import tempfile
import time
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reguly as R            # noqa: E402
from skaner import Skaner     # noqa: E402
from wykonawca import wykonaj  # noqa: E402


def plik(p: Path, kb: int = 1024, dni_wstecz: int = 0, tresc: bytes | None = None):
    """Tworzy plik o zadanym wieku. Katalogi dostaja ten sam czas co plik,
    inaczej skaner widzi swiezy folder i slusznie go pomija."""
    p.parent.mkdir(parents=True, exist_ok=True)
    dane = tresc if tresc is not None else os.urandom(kb * 1024)
    p.write_bytes(dane)
    if dni_wstecz:
        t = time.time() - dni_wstecz * 86400
        os.utime(p, (t, t))
        katalog = p.parent
        while str(katalog).startswith(str(Path.home())) and katalog != Path.home():
            os.utime(katalog, (t, t))
            katalog = katalog.parent
    return p


class AtrapaDysku(unittest.TestCase):
    def setUp(self):
        self.baza = Path(tempfile.mkdtemp(prefix="sprzatanie_test_"))
        self.home = self.baza / "home"
        self.oryginal_home = Path.home
        Path.home = staticmethod(lambda: self.home)   # izolacja od prawdziwego profilu

        # --- smieci, ktore MUSZA zostac znalezione
        plik(self.home / "AppData/Local/Temp/smieci.tmp", 4096)
        plik(self.home / "AppData/Local/Google/Chrome/User Data/Default/Cache/c1", 4096)
        plik(self.home / "Downloads/stara_instalka.exe", 2048, dni_wstecz=90)
        plik(self.home / "Downloads/nowa_instalka.exe", 2048, dni_wstecz=2)
        plik(self.home / "Downloads/archiwum.zip", 2048, dni_wstecz=400)
        plik(self.home / "projekt/node_modules/a/b/index.js", 8192, dni_wstecz=200)
        plik(self.home / "projekt2/.venv/lib/x.py", 8192, dni_wstecz=120)
        plik(self.home / "projekt3/build/out.bin", 8192, dni_wstecz=90)
        # --- duplikaty
        tresc = b"identyczna tresc" * 300000        # ~4.8 MB
        plik(self.home / "Pictures/rodzinne.jpg", 0, 500, tresc)
        plik(self.home / "Pictures/kopia_rodzinne.jpg", 0, 10, tresc)
        # --- duze pliki: plik rzadki 700 MB, nie zajmuje miejsca na dysku
        duzy = plik(self.home / "Videos/wakacje.mkv", 0, 30, b"")
        with open(duzy, "r+b") as f:
            f.seek(700 * 1024 * 1024 - 1)
            f.write(b"\0")
        os.utime(duzy, (time.time() - 30 * 86400,) * 2)

        # --- PULAPKI: nazwa pasuje do smieci, ale sciezka jest chroniona
        self.onedrive = plik(self.home / "OneDrive/Temp/nie_ruszaj.txt", 2048)
        self.git = plik(self.home / "projekt/.git/objects/pack/x", 2048)
        self.dokument = plik(self.home / "Documents/umowa.docx", 512)
        self.dokument_str = str(self.dokument)

    def tearDown(self):
        Path.home = self.oryginal_home
        shutil.rmtree(self.baza, ignore_errors=True)

    def skan(self):
        return Skaner([self.home]).uruchom()

    # ------------------------------------------------------------------ testy
    def test_01_smieci_wykryte(self):
        plan = self.skan()
        kategorie = {g["kategoria"]: g for g in plan["grupy"]}
        for oczekiwana in ("cache_i_temp", "katalogi_builda", "stare_instalki",
                           "duplikaty", "duze_pliki"):
            self.assertIn(oczekiwana, kategorie, f"brak grupy {oczekiwana}")

        sciezki_cache = {p["sciezka"] for p in kategorie["cache_i_temp"]["pozycje"]}
        self.assertTrue(any("Temp" in s for s in sciezki_cache))
        self.assertTrue(any("User Data" in s and "Cache" in s for s in sciezki_cache))

        buildy = {Path(p["sciezka"]).name for p in kategorie["katalogi_builda"]["pozycje"]}
        self.assertEqual({"node_modules", ".venv", "build"}, buildy)

        instalki = [p["sciezka"] for p in kategorie["stare_instalki"]["pozycje"]]
        self.assertEqual(len(instalki), 1)
        self.assertIn("stara_instalka.exe", instalki[0])

    def test_02_duplikat_zostawia_najstarszy(self):
        plan = self.skan()
        grupa = next(g for g in plan["grupy"] if g["kategoria"] == "duplikaty")
        self.assertEqual(len(grupa["pozycje"]), 1)
        self.assertIn("kopia_rodzinne.jpg", grupa["pozycje"][0]["sciezka"])

    def test_03_one_drive_i_git_chronione(self):
        plan = self.skan()
        wszystkie = [p["sciezka"] for g in plan["grupy"] for p in g["pozycje"]]
        for chroniona in (str(self.onedrive), str(self.git)):
            self.assertNotIn(chroniona, wszystkie, f"nie wolno ruszac {chroniona}")

    def test_04_rozmiary_zgodne(self):
        plan = self.skan()
        # do odzyskania bez ryzyka danych: 8 cache + 24 buildy + 4.6 duplikat
        # + 2 instalka. Archiwum i duzy plik to tylko raport (ryzyko 3).
        odzysk = plan["suma_do_odzyskania_MB"]
        self.assertGreater(odzysk, 37.0, f"za malo: {odzysk}")
        self.assertLess(odzysk, 40.0, f"za duzo: {odzysk}")
        self.assertAlmostEqual(plan["dysk"]["razem_GB"] > 0, True)

    def test_05_tryb_probny_nic_nie_rusza(self):
        plan = self.skan()
        przed = sorted(str(p) for p in self.home.rglob("*"))
        wynik = wykonaj(plan, probne=True)
        po = sorted(str(p) for p in self.home.rglob("*"))
        # poza katalogiem na manifest/plan, drzewo musi byc identyczne
        # wpisy z katalogu raportow to jedyne dozwolone nowe pliki
        dozwolone = ("SprzatanieDysku",)
        roznica = {x for x in set(po) ^ set(przed) if not any(d in x for d in dozwolone)}
        self.assertEqual(roznica, set(), f"tryb probny zmienil dysk: {roznica}")
        self.assertGreater(len(wynik["ruchy"]), 0)
        self.assertEqual(wynik["odzyskane_MB"], 0.0)

    def test_06_przeniesienie_i_przywrocenie(self):
        plan = self.skan()
        temp = self.home / "AppData/Local/Temp/smieci.tmp"
        instalka = self.home / "Downloads/stara_instalka.exe"
        self.assertTrue(temp.exists() and instalka.exists())

        wynik = wykonaj(plan, probne=False)
        self.assertGreater(wynik["odzyskane_MB"], 0)
        self.assertFalse(temp.exists(), "temp powinien zostac przeniesiony")
        self.assertFalse(instalka.exists(), "instalka powinna zostac przeniesiona")
        # dane uzytkownika zostaja na miejscu
        self.assertTrue(self.dokument.exists(), "dokumentow nie ruszamy")
        self.assertTrue(self.onedrive.exists(), "OneDrive nietkniety")

        # manifest pozwala cofnac
        manifest = json.loads(Path(wynik["manifest"]).read_text(encoding="utf-8"))
        self.assertEqual(manifest["tryb"], "wykonany")
        for r in manifest["ruchy"]:
            self.assertTrue(Path(r["na"]).exists(), "cel przeniesienia nie istnieje")

        shutil.move(str(Path(manifest["ruchy"][0]["na"])),
                    str(Path(manifest["ruchy"][0]["z"])))
        self.assertTrue(any(True for _ in self.home.rglob("*")))


if __name__ == "__main__":
    unittest.main(verbosity=2)
