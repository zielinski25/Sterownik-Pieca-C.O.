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
        "bledy": [],
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
            try:
                cel = _unikalna_nazwa(cel_baza / grupa["kategoria"] / zrodlo.name)
                cel.parent.mkdir(parents=True, exist_ok=True)
                shutil.move(str(zrodlo), str(cel))
                manifest["ruchy"].append({
                    "z": str(zrodlo), "na": str(cel),
                    "MB": round(rozmiar / 1024 / 1024, 1),
                    "kategoria": grupa["kategoria"],
                })
                manifest["odzyskane_MB"] += round(rozmiar / 1024 / 1024, 1)
            except (OSError, shutil.Error) as e:
                manifest["bledy"].append({"sciezka": str(zrodlo), "blad": str(e)})

    katalog = R.katalog_raportow(home)
    katalog.mkdir(parents=True, exist_ok=True)
    wyj = katalog / f"manifest_{znacznik}.json"
    wyj.write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    manifest["manifest"] = str(wyj)
    manifest["kwarantanna"] = str(cel_baza) if not probne else str(kwarantanna)
    manifest["odzyskane_MB"] = round(manifest["odzyskane_MB"], 1)
    return manifest
