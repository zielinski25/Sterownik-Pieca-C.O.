# -*- coding: utf-8 -*-
"""
Wykonawca planu - faza 3. Przenosi do kwarantanny ALBO tylko raportuje.

Zasady bezpieczenstwa (kazda jest twarda, nie do obejscia flaga):
  1. Tylko pozycje z grup o akcji "kwarantanna" i statusie "zatwierdzone".
  2. Kazda sciezka sprawdzana jeszcze raz przez chroniony().
  3. Przenoszenie, nie kasowanie - plik da sie odzyskac.
  4. Manifest JSON z kazdym ruchem: skad -> dokad. To jest kopia zapasowa
     struktury, nawet po oproznieniu kwarantanny.
  5. Folder kwarantanny na tym samym dysku = przenoszenie natychmiastowe.
"""

import datetime as dt
import json
import os
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reguly as R  # noqa: E402
from skaner import chroniony  # noqa: E402


def _unikalna_nazwa(cel: Path) -> Path:
    if not cel.exists():
        return cel
    i = 1
    while True:
        kandydat = cel.with_name(f"{cel.stem}__{i}{cel.suffix}")
        if not kandydat.exists():
            return kandydat
        i += 1


def _rozmiar(p: Path) -> int:
    try:
        if p.is_file():
            return p.stat().st_size
        suma = 0
        for k, _, pliki in os.walk(p, onerror=lambda e: None):
            for f in pliki:
                try:
                    suma += (Path(k) / f).stat().st_size
                except OSError:
                    pass
        return suma
    except OSError:
        return 0


PREFIKS_DLUGIEJ_SCIEZKI = "\\\\?\\"   # Windows: \\?\  (limit ~32 tys. znakow)


def _dluga_sciezka(p: Path) -> Path:
    """Windows odmawia sciezkom >260 znakow (WinError 206).

    Prefiks \\?\\ podnosi limit do ~32 tys. znakow. Tylko Windows i tylko
    dlugie sciezki - krotkie zostaja bez zmian, wiec manifest jest czysty.
    """
    s = str(p)
    if os.name == "nt" and len(s) > 240 and not s.startswith(PREFIKS_DLUGIEJ_SCIEZKI):
        return Path(PREFIKS_DLUGIEJ_SCIEZKI + s)
    return p


def _przenies_plik(zrodlo: Path, cel: Path) -> tuple[int, str | None]:
    """Przenosi JEDEN plik. Zwraca (rozmiar, blad).

    Windows nie pozwoli przeniesc calego %TEMP% naraz: czesc plikow jest
    w uzyciu, a sciezki potrafia przekroczyc 260 znakow (WinError 206).
    Dlatego idziemy plik po pliku - jeden zablokowany plik nie psuje reszty.
    """
    zrodlo = _dluga_sciezka(zrodlo)
    cel = _dluga_sciezka(cel)
    try:
        rozmiar = zrodlo.stat().st_size
    except OSError:
        rozmiar = 0
    try:
        cel.parent.mkdir(parents=True, exist_ok=True)
        shutil.move(str(zrodlo), str(_unikalna_nazwa(cel)))
        return rozmiar, None
    except (OSError, shutil.Error) as e:
        powod = getattr(e, "strerror", None) or str(e)
        kod = getattr(e, "winerror", None)
        if kod == 206:
            powod = "sciezka za dluga dla Windows (WinError 206)"
        elif kod == 32:
            powod = "plik uzywany przez inny proces (WinError 32)"
        elif kod == 3:
            powod = "sciezka nie istnieje (WinError 3)"
        return 0, str(powod)[:120]


def _przenies_katalog(zrodlo: Path, cel: Path) -> tuple[int, list[dict], int]:
    """Przenosi zawartosc katalogu plik po pliku, zachowujac strukture.

    Zwraca (przeniesione_bajty, lista_bledow, liczba_plikow_ok).
    """
    bajty = 0
    bledy: list[dict] = []
    ok = 0
    for korzen, _katalogi, pliki in os.walk(zrodlo, topdown=False,
                                            onerror=lambda e: None):
        for f in pliki:
            z = Path(korzen) / f
            wzgledna = z.relative_to(zrodlo)
            r, blad = _przenies_plik(z, cel / wzgledna)
            if blad:
                # grupujemy po powodzie - 200 takich samych bledow to szum
                bledy.append({"sciezka": str(z), "blad": blad})
            else:
                bajty += r
                ok += 1
        # pusty katalog po przeniesieniu - usun, jesli sie udalo
        try:
            Path(korzen).rmdir()
        except OSError:
            pass
    return bajty, bledy, ok


def wykonaj(plan: dict, probne: bool = True, potwierdzone_kategorie=None) -> dict:
    """Przenosi zatwierdzone pozycje. probne=True -> nic nie rusza."""
    home = Path.home()
    kwarantanna = R.katalog_kwarantanny(home)
    znacznik = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    cel_baza = kwarantanna / znacznik

    manifest = {
        "data": dt.datetime.now().isoformat(timespec="seconds"),
        "tryb": "probny" if probne else "wykonany",
        "ruchy": [],
        "bledy": [],        # cale pozycje, ktorych nie udalo sie ruszyc
        "pominiete": [],    # pojedyncze pliki w srodku katalogu
        "odzyskane_MB": 0.0,
    }

    for grupa in plan.get("grupy", []):
        if grupa.get("akcja") != "kwarantanna":
            continue
        if potwierdzone_kategorie and grupa["kategoria"] not in potwierdzone_kategorie:
            continue
        for poz in grupa.get("pozycje", []):
            zrodlo = Path(poz["sciezka"])
            if not zrodlo.exists():
                continue
            if chroniony(zrodlo):
                manifest["bledy"].append(
                    {"sciezka": str(zrodlo), "blad": "sciezka chroniona - pominieto"})
                continue
            rozmiar = _rozmiar(zrodlo)
            if probne:
                manifest["ruchy"].append({
                    "z": str(zrodlo), "na": "(probne - bez zmian)",
                    "MB": round(rozmiar / 1024 / 1024, 1),
                    "kategoria": grupa["kategoria"],
                })
                continue
            cel = cel_baza / grupa["kategoria"] / zrodlo.name
            if zrodlo.is_file():
                r, blad = _przenies_plik(zrodlo, cel)
                if blad:
                    manifest["bledy"].append({"sciezka": str(zrodlo), "blad": blad})
                else:
                    manifest["ruchy"].append({
                        "z": str(zrodlo), "na": str(cel),
                        "MB": round(r / 1024 / 1024, 1),
                        "kategoria": grupa["kategoria"],
                    })
                    manifest["odzyskane_MB"] += round(r / 1024 / 1024, 1)
            elif zrodlo.is_dir():
                r, bledy, ok = _przenies_katalog(zrodlo, cel)
                manifest["ruchy"].append({
                    "z": str(zrodlo), "na": str(cel),
                    "MB": round(r / 1024 / 1024, 1),
                    "plikow": ok,
                    "kategoria": grupa["kategoria"],
                })
                manifest["odzyskane_MB"] += round(r / 1024 / 1024, 1)
                # bledy per plik: trzymamy w osobnej sekcji, zeby nie mieszac ich
                # z bledami calych pozycji (inaczej raport jest nieczytelny)
                for b in bledy:
                    manifest["pominiete"].append(b)

    katalog = R.katalog_raportow(home)
    katalog.mkdir(parents=True, exist_ok=True)
    wyj = katalog / f"manifest_{znacznik}.json"
    wyj.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    manifest["manifest"] = str(wyj)
    manifest["kwarantanna"] = str(cel_baza) if not probne else str(kwarantanna)
    manifest["odzyskane_MB"] = round(manifest["odzyskane_MB"], 1)
    return manifest
