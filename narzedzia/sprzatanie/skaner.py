# -*- coding: utf-8 -*-
"""
Skaner dysku - faza 1. Tylko czyta, nic nie rusza.

Wyjscie: JSON z "grupami" znalezisk. Kazda grupa ma rozmiar, ryzyko i powod.
Ten JSON pozniej czyta agent.py (reguly + opcjonalnie lokalny model).

Uruchomienie:
    python skaner.py                      # profil uzytkownika, domyslne progi
    python skaner.py --korzen D:\\         # tylko wskazany katalog
    python skaner.py --wyjscie plan.json
"""

import argparse
import datetime as dt
import hashlib
import json
import os
import shutil
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reguly as R  # noqa: E402


# ------------------------------------------------------------------ pomocnicze
def mb(b: int) -> float:
    return round(b / (1024 * 1024), 1)


def gb(b: int) -> float:
    return round(b / (1024 ** 3), 2)


def dni(t: float) -> int:
    return int((time.time() - t) / 86400)


def chroniony(p: Path) -> bool:
    """Czy sciezka jest na liscie NIGDY (chmura, system, .git)."""
    s = str(p).lower().replace("\\", "/")
    for frag in R.NIGDY_FRAGMENTY:
        f = frag.lower().replace("\\", "/").lstrip("/")
        if f and f in s:
            return True
    return False


def info_katalogu(p: Path, limit_s: float = 30.0) -> tuple[int, float]:
    """(suma bajtow, najnowszy mtime w drzewie).

    Wiek katalogu liczymy po NAJNOWSZYM pliku w srodku, nie po dacie folderu:
    Windows odswieza mtime folderu przy kazdym zapisie, wiec sam folder
    wyglada na "swiezy" nawet gdy projekt lezy od roku.
    """
    start = time.time()
    suma = 0
    najnowszy = 0.0
    for korzen, katalogi, pliki in os.walk(p, onerror=lambda e: None):
        katalogi[:] = [k for k in katalogi if k.lower() not in R.POMIN_KATALOGI]
        for f in pliki:
            try:
                st = (Path(korzen) / f).stat()
            except OSError:
                continue
            suma += st.st_size
            najnowszy = max(najnowszy, st.st_mtime)
        if time.time() - start > limit_s:
            break
    return suma, najnowszy


def rozmiar_katalogu(p: Path, limit_s: float = 30.0) -> int:
    """Suma rozmiarow plikow; limit czasu chroni przed ogromnymi drzewami."""
    return info_katalogu(p, limit_s)[0]


def pozycja(p: Path, typ: str, rozmiar: int, powod: str,
            mtime_zamiast: float | None = None) -> dict:
    try:
        mtime = mtime_zamiast or p.stat().st_mtime
    except OSError:
        mtime = mtime_zamiast or 0
    return {
        "sciezka": str(p),
        "typ": typ,
        "rozmiar_MB": mb(rozmiar),
        "wiek_dni": dni(mtime) if mtime else None,
        "powod": powod,
    }


class Skaner:
    def __init__(self, korzenie):
        self.korzenie = [Path(k).expanduser() for k in korzenie]
        self.home = Path.home()
        self.grupy = []
        self.widziane = set()      # sciezki juz zakwalifikowane - bez dublowania
        self.duplikaty_wylaczone = set()

    # --------------------------------------------------------------- dodawanie
    def dodaj_grupe(self, kategoria, ryzyko, akcja, pozycje, opis):
        if not pozycje:
            return
        suma = sum(p["rozmiar_MB"] for p in pozycje)
        self.grupy.append({
            "kategoria": kategoria,
            "ryzyko": ryzyko,
            "akcja": akcja,          # kwarantanna | raport
            "opis": opis,
            "rozmiar_MB": round(suma, 1),
            "pozycje": pozycje,
        })

    def _zajeta(self, p: Path) -> bool:
        return str(p).lower() in self.widziane

    def _zaznacz(self, p: Path):
        self.widziane.add(str(p).lower())

    # ------------------------------------------------------------------ cache
    def _dodaj_cache(self, cel: Path, pozycje: list, powod: str):
        """Dodaje katalog albo pojedynczy plik cache, jesli ma sens i jest nowy."""
        if not cel.exists() or chroniony(cel) or self._zajeta(cel):
            return
        if cel.is_file():
            r = cel.stat().st_size
        elif cel.is_dir():
            r = rozmiar_katalogu(cel)
        else:
            return
        if r > 1024 * 1024:
            pozycje.append(pozycja(cel, "plik" if cel.is_file() else "katalog",
                                   r, powod))
            self._zaznacz(cel)

    def cache(self):
        pozycje = []
        local = self.home / "AppData" / "Local"
        for wzor in R.CACHE_W_LOCALAPPDATA:
            czesci = wzor.split("/")
            idx = next((i for i, c in enumerate(czesci) if "*" in c), None)
            if idx is None:
                self._dodaj_cache(local / wzor, pozycje, "cache aplikacji")
                continue
            # glob: rozwijamy tylko pierwszy segment z gwiazdka, reszta to sciezka
            rodzic = local.joinpath(*czesci[:idx])
            if not rodzic.is_dir():
                continue
            ogon = czesci[idx + 1:]
            for kandydat in rodzic.glob(czesci[idx]):
                cel = kandydat.joinpath(*ogon) if ogon else kandydat
                self._dodaj_cache(cel, pozycje, "cache przegladarki/aplikacji")

        for wzor in R.CACHE_W_HOME:
            p = self.home / wzor
            if p.is_dir() and not chroniony(p) and not self._zajeta(p):
                r = rozmiar_katalogu(p)
                if r > 1024 * 1024:
                    pozycje.append(pozycja(p, "katalog", r, "cache narzedzi deweloperskich"))
                    self._zaznacz(p)

        self.dodaj_grupe(
            "cache_i_temp", 1, "kwarantanna", pozycje,
            "Cache i pliki tymczasowe. Programy odtworza je same przy nastepnym "
            "uruchomieniu. Bezpieczne, ale po skasowaniu pierwsze otwarcie "
            "przegladarki moze byc wolniejsze.")

    # -------------------------------------------------------- kosz systemowy
    def kosz(self):
        if os.name != "nt":
            return
        pozycje = []
        for litera in "CDEFGH":
            kosz = Path(f"{litera}:/$Recycle.Bin")
            if kosz.is_dir():
                r = rozmiar_katalogu(kosz, limit_s=10)
                if r > 1024 * 1024:
                    pozycje.append(pozycja(kosz, "katalog", r, "kosz systemowy"))
        self.dodaj_grupe(
            "kosz_systemowy", 1, "raport", pozycje,
            "Zawartosc kosza. Oproznij recznie (PPM na Kosz -> Oproznij), "
            "bo skrypt nie ma praw do cudzych koszy.")

    # ------------------------------------------------- v8: Windows.old
    def windows_old(self):
        """Katalog Windows.old (poprzednia instalacja Windowsa)."""
        pozycje = []
        if os.name == "nt":
            for litera in "CDEFGH":
                p = Path(f"{litera}:/Windows.old")
                if p.is_dir():
                    r, najnowszy = info_katalogu(p, limit_s=30)
                    if r > R.PROG_OLD_WINDOWS_GB * 1024 * 1024 * 1024:
                        pozycje.append(pozycja(p, "katalog", r,
                                               "poprzednia instalacja Windows",
                                               mtime_zamiast=najnowszy))
        self.dodaj_grupe(
            "windows_old", 2, "kwarantanna", pozycje,
            "Windows.old = poprzedni Windows po aktualizacji. Mozesz go "
            "usunac, jesli nie planujesz powrotu. Kwarantanna = cofniecie "
            "mozliwe. UWAGA: przeniesienie Windows.old moze wymagac praw "
            "administratora - agent zgłosi blad, nic nie zginie.")

    # --------------------------------------------- v8: cache deweloperskie
    def cache_dev(self):
        """Cache deweloperskie (PlatformIO, Cargo, pnpm...).

        Osobna grupa, bo to sa pobrane artefakty (toolchainy, crate'y),
        odtwarzalne komenda, ale czesto po kilka GB. Próg 10 MB.
        """
        pozycje = []
        for korzen in self.korzenie:
            for wzorzec in R.CACHE_DEV_W_HOME:
                p = korzen / wzorzec
                if not p.exists() or chroniony(p) or self._zajeta(p):
                    continue
                try:
                    r, najnowszy = info_katalogu(p, limit_s=20)
                except (OSError, PermissionError):
                    continue
                if r > R.PROG_CACHE_DEV_MB * 1024 * 1024:
                    pozycje.append(pozycja(p, "katalog", r,
                                           f"cache deweloperskie ({p.name})",
                                           mtime_zamiast=najnowszy))
                    self._zaznacz(p)
        self.dodaj_grupe(
            "cache_deweloperskie", 2, "kwarantanna", pozycje,
            "Pobrane artefakty deweloperskie (PlatformIO, Cargo, pnpm). "
            "Odtworzysz komenda (pio run, cargo build, pnpm install). "
            "NIE dotyka .android (debug.keystore) ani .ollama (modele AI).")

    # -------------------------------------------------------- katalogi builda
    def buildy(self):
        pozycje = []
        for korzen in self.korzenie:
            for p in korzen.rglob("*"):
                if len(pozycje) > 5000:
                    break
                try:
                    if not p.is_dir() or p.name.lower() not in R.KATALOGI_BUILDA:
                        continue
                    if chroniony(p) or self._zajeta(p):
                        continue
                    r, najnowszy = info_katalogu(p, limit_s=20)
                    if not najnowszy:
                        continue
                    wiek = dni(najnowszy)
                    if wiek < R.PROG_PROJEKT_DNI:
                        continue
                    if r > 5 * 1024 * 1024:
                        pozycje.append(pozycja(p, "katalog", r,
                                               f"artefakt builda, ostatnia zmiana {wiek} dni temu",
                                               mtime_zamiast=najnowszy))
                        self._zaznacz(p)
                except (OSError, PermissionError):
                    continue
        self.dodaj_grupe(
            "katalogi_builda", 2, "kwarantanna", pozycje,
            "node_modules, .venv, target, dist itd. Odtworzysz komenda "
            "(npm install, pip install -r, cargo build). Skasowanie = trzeba "
            "przebudowac projekt.")

    # ---------------------------------------------------- stare pliki z Pobranych
    def pobrane(self):
        instalki, archiwa, logi = [], [], []
        bazy = [self.home / "Downloads", self.home / "Pobrane"]
        for baza in bazy:
            if not baza.is_dir():
                continue
            for p in baza.iterdir():
                try:
                    if not p.is_file() or chroniony(p) or self._zajeta(p):
                        continue
                    st = p.stat()
                    wiek = dni(st.st_mtime)
                    ext = p.suffix.lower()
                    if ext in R.EXT_INSTALKI and wiek > R.PROG_INSTALKI_DNI:
                        instalki.append(pozycja(p, "plik", st.st_size,
                                                f"instalka, {wiek} dni"))
                        self._zaznacz(p)
                    elif ext in R.EXT_ARCHIWA and wiek > R.PROG_POBRANE_DNI:
                        archiwa.append(pozycja(p, "plik", st.st_size,
                                               f"archiwum, {wiek} dni"))
                        self._zaznacz(p)
                    elif ext in R.EXT_LOGOW and wiek > R.PROG_POBRANE_DNI:
                        logi.append(pozycja(p, "plik", st.st_size, f"log, {wiek} dni"))
                        self._zaznacz(p)
                except OSError:
                    continue
        self.dodaj_grupe("stare_instalki", 2, "kwarantanna", instalki,
                         "Instalki programow starsze niz "
                         f"{R.PROG_INSTALKI_DNI} dni. Program jest juz zainstalowany, "
                         "ale przy problemach przydaje sie ten sam plik.")
        self.dodaj_grupe("stare_archiwa", 3, "raport", archiwa,
                         "Stare ZIP/RAR/ISO w Pobranych. Moga zawierac jedyna kopie "
                         "czegos - dlatego domyslnie tylko raport.")
        self.dodaj_grupe("stare_logi", 1, "kwarantanna", logi,
                         "Pliki .log/.dmp/.bak po awariach. Nikomu nie sa potrzebne.")

    # ------------------------------------------------- v8: puste katalogi
    def puste_katalogi(self):
        """Puste katalogi (0 B) - sprzatanie porzadku, nie miejsca."""
        pozycje = []
        for korzen in self.korzenie:
            for p in korzen.rglob("*"):
                if len(pozycje) > 5000:
                    break
                try:
                    if not p.is_dir() or chroniony(p) or self._zajeta(p):
                        continue
                    # pusty = brak pozycji (pliki i podkatalogi)
                    if any(p.iterdir()):
                        continue
                    pozycje.append(pozycja(p, "katalog", 0,
                                           "pusty katalog",
                                           mtime_zamiast=p.stat().st_mtime))
                    self._zaznacz(p)
                except (OSError, PermissionError):
                    continue
        self.dodaj_grupe(
            "puste_katalogi", 1, "kwarantanna", pozycje,
            "Puste katalogi (0 B). Nie zwalniaja miejsca, ale porzadkuja "
            "drzewo. Kwarantanna = bezpieczne cofniecie.")

    # ------------------------------------------------- v8: stare katalogi
    def stare_katalogi(self):
        """Katalogi nieuzywane rok+ (tylko raport, ryzyko 3)."""
        # v9: pomijamy katalogi builda i VCS - te obsluguje buildy(),
        # a .pio/.git to wnetrze aktywnych projektow, nie "stare katalogi".
        pomin = set(R.KATALOGI_BUILDA) | {".pio", ".git", ".hg", ".svn"}
        pozycje = []
        for korzen in self.korzenie:
            for p in korzen.rglob("*"):
                if len(pozycje) > 5000:
                    break
                try:
                    if not p.is_dir() or chroniony(p) or self._zajeta(p):
                        continue
                    if p.name.lower() in pomin:
                        continue
                    r, najnowszy = info_katalogu(p, limit_s=20)
                    if not najnowszy:
                        continue
                    wiek = dni(najnowszy)
                    if wiek < R.PROG_STARE_KATALOGI_DNI:
                        continue
                    if r > 1024 * 1024:  # >1 MB
                        pozycje.append(pozycja(p, "katalog", r,
                                               f"nieuzywany {wiek} dni",
                                               mtime_zamiast=najnowszy))
                        self._zaznacz(p)
                except (OSError, PermissionError):
                    continue
        self.dodaj_grupe(
            "stare_katalogi", 3, "raport", pozycje,
            "Katalogi nieuzywane rok+. Tylko raport - agent NIE przenosi "
            "automatycznie (ryzyko 3). Przejrzyj recznie.")

    # -------------------------------------------------------------- duplikaty
    def duplikaty(self):
        kandydaci = {}
        bazy = [self.home / n for n in ("Downloads", "Pobrane", "Pictures", "Obrazy",
                                        "Documents", "Dokumenty", "Desktop", "Pulpit",
                                        "Videos", "Wideo")]
        for baza in bazy:
            if not baza.is_dir():
                continue
            for korzen, katalogi, pliki in os.walk(baza, onerror=lambda e: None):
                katalogi[:] = [k for k in katalogi if not chroniony(Path(korzen) / k)]
                for f in pliki:
                    p = Path(korzen) / f
                    try:
                        s = p.stat().st_size
                    except OSError:
                        continue
                    if s < R.PROG_DUPLIKAT_MB * 1024 * 1024 or chroniony(p):
                        continue
                    kandydaci.setdefault((s, p.suffix.lower()), []).append(p)

        nadmiarowe = []
        for (s, _ext), lista in kandydaci.items():
            if len(lista) < 2:
                continue
            hashe = {}
            for p in lista:
                h = self._hash(p)
                if h:
                    hashe.setdefault(h, []).append(p)
            for h, grupa in hashe.items():
                if len(grupa) < 2:
                    continue
                grupa.sort(key=lambda x: (x.stat().st_mtime, len(str(x))))
                wzor = grupa[0]                     # najstarszy zostaje
                for p in grupa[1:]:
                    if not self._zajeta(p):
                        nadmiarowe.append(pozycja(
                            p, "plik", s,
                            f"duplikat: {wzor.name}"))
                        self._zaznacz(p)
        self.dodaj_grupe("duplikaty", 2, "kwarantanna", nadmiarowe,
                         "Ten sam plik w kilku miejscach (ten sam SHA-256). "
                         "Zostaje najstarszy egzemplarz, reszta do kwarantanny.")

    @staticmethod
    def _hash(p: Path) -> str | None:
        """SHA-256 calego pliku - duplikaty musza byc identyczne co do bajtu."""
        try:
            h = hashlib.sha256()
            with open(p, "rb") as f:
                for blok in iter(lambda: f.read(1024 * 1024), b""):
                    h.update(blok)
            return h.hexdigest()
        except OSError:
            return None

    # ------------------------------------------------------------ duze pliki
    def duze_pliki(self):
        pozycje = []
        prog = R.PROG_DUZY_MB * 1024 * 1024
        for korzen in self.korzenie:
            for katalog, katalogi, pliki in os.walk(korzen, onerror=lambda e: None):
                katalogi[:] = [k for k in katalogi
                               if k.lower() not in R.POMIN_KATALOGI
                               and not chroniony(Path(katalog) / k)]
                for f in pliki:
                    p = Path(katalog) / f
                    try:
                        st = p.stat()
                    except OSError:
                        continue
                    if st.st_size >= prog and not chroniony(p) and not self._zajeta(p):
                        pozycje.append(pozycja(p, "plik", st.st_size,
                                               f"{mb(st.st_size)} MB"))
                        self._zaznacz(p)
        pozycje.sort(key=lambda x: -x["rozmiar_MB"])
        self.dodaj_grupe("duze_pliki", 3, "raport", pozycje[:200],
                         f"Pliki wieksze niz {R.PROG_DUZY_MB} MB. Domyslnie tylko "
                         "raport - to moga byc maszyny wirtualne, obrazy dyskow, "
                         "surowce wideo. Decyzja nalezy do Ciebie.")

    # ------------------------------------------------------------------ start
    def uruchom(self):
        t0 = time.time()
        print("Skanuje: cache...", flush=True);        self.cache()
        print("Skanuje: kosz...", flush=True);         self.kosz()
        print("Skanuje: duplikaty...", flush=True);    self.duplikaty()
        print("Skanuje: Pobrane...", flush=True);      self.pobrane()
        print("Skanuje: artefakty builda...", flush=True); self.buildy()
        print("Skanuje: duze pliki...", flush=True);   self.duze_pliki()
        # v8: nowe kategorie
        print("Skanuje: cache deweloperskie...", flush=True); self.cache_dev()
        print("Skanuje: Windows.old...", flush=True);  self.windows_old()
        print("Skanuje: puste katalogi...", flush=True); self.puste_katalogi()
        print("Skanuje: stare katalogi...", flush=True); self.stare_katalogi()

        total = shutil.disk_usage(self.home)
        plan = {
            "wygenerowano": dt.datetime.now().isoformat(timespec="seconds"),
            "host": os.environ.get("COMPUTERNAME", "?"),
            "korzenie": [str(k) for k in self.korzenie],
            "dysk": {
                "razem_GB": gb(total.total),
                "wolne_GB": gb(total.free),
                "uzyte_GB": gb(total.used),
                "zajete_proc": round(100 * total.used / total.total, 1),
            },
            "czas_skanu_s": round(time.time() - t0, 1),
            "grupy": sorted(self.grupy, key=lambda g: -g["rozmiar_MB"]),
        }
        plan["suma_do_odzyskania_MB"] = round(sum(
            g["rozmiar_MB"] for g in plan["grupy"] if g["ryzyko"] <= 2), 1)
        # v8: rekomendacje systemowe (tylko raport, skrypt ich nie wykona)
        plan["rekomendacje_systemowe"] = R.REKOMENDACJE_SYSTEMOWE
        return plan


def main():
    ap = argparse.ArgumentParser(description="Skaner dysku (tylko czyta)")
    ap.add_argument("--korzen", action="append", default=None,
                    help="katalog do skanowania (mozna podac wiele razy)")
    ap.add_argument("--wyjscie", default=None, help="plik JSON z planem")
    a = ap.parse_args()

    home = Path.home()
    korzenie = a.korzen or [str(home / n) for n in
                            ("Downloads", "Pobrane", "Documents", "Dokumenty",
                             "Desktop", "Pulpit", "Pictures", "Obrazy", "source",
                             "repos", "Projekty")]
    korzenie = [k for k in korzenie if Path(k).is_dir()] or [str(home)]

    plan = Skaner(korzenie).uruchom()

    if a.wyjscie:
        wyj = Path(a.wyjscie)
    else:
        katalog = R.katalog_raportow(home)
        katalog.mkdir(parents=True, exist_ok=True)
        wyj = katalog / f"plan_{dt.datetime.now():%Y-%m-%d_%H%M}.json"
    wyj.write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding="utf-8")

    d = plan["dysk"]
    print(f"\nDysk: {d['uzyte_GB']} / {d['razem_GB']} GB ({d['zajete_proc']}%), "
          f"wolne {d['wolne_GB']} GB")
    for g in plan["grupy"]:
        print(f"  [{g['ryzyko']}] {g['kategoria']:<20} {g['rozmiar_MB']:>9.1f} MB  "
              f"({len(g['pozycje'])} poz., akcja: {g['akcja']})")
    print(f"\nDo odzyskania bez ryzyka danych: "
          f"{plan['suma_do_odzyskania_MB']/1024:.2f} GB")
    print(f"Plan zapisany: {wyj}")


if __name__ == "__main__":
    main()
