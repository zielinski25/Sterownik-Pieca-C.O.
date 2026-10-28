#!/usr/bin/env python3
"""219: zbuduj trzy stany pliku od zera i sprawdz ich SHA-256.

Stan A - v3.55.8 (baza, bez zadnej poprawki),
stan B - v3.56.3 (po 211, 212, 213, 214, 216; z bledem przechwytywania tras),
stan C - v3.56.4 (po 218; to jest plik do wgrania).

Wszystko odtwarzane z przypietego pliku v3.55.8 i patchy z poprzednich
etapow - zaden plik nie jest kopiowany "z paczki".

Uruchomienie:
    python3 -B narzedzia/ostateczna-brama219/build.py [katalog_docelowy]
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
NARZ = os.path.join(REPO, "narzedzia")
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PRZYKLAD = os.path.join(FIRMWARE, "secrets.h.example")

P211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
P212 = os.path.join(NARZ, "pobranie-trasa212", "main_centrala-v3.55.9-to-v3.56.0.patch")
P213 = os.path.join(NARZ, "panel-pobrania213", "main_centrala-v3.56.0-to-v3.56.1.patch")
P214 = os.path.join(NARZ, "brama-przed-wgraniem214", "main_centrala-v3.56.1-to-v3.56.2.patch")
P216 = os.path.join(NARZ, "od-przycisku216", "main_centrala-v3.56.2-to-v3.56.3.patch")
P218_KUM = os.path.join(NARZ, "panel-trasa218", "main_centrala-v3.55.8-to-v3.56.4.patch")

SHA = {
    "v3.55.8": "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31",
    "v3.56.3": "f51b0b0749e2c1e3a8b5c02848a9c0e4902e0b641d6a7313a42304bbbcef91b3",
    "v3.56.4": "f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38",
}
WERSJE = {"v3.56.3": 46974, "v3.56.4": 46995}


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", patch],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s"
                         % (os.path.basename(patch), t.stdout, t.stderr))


def zbuduj(docel):
    os.makedirs(docel, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="b219-")
    opis = {}
    try:
        for nazwa, patche in (("v3.55.8", ()),
                              ("v3.56.3", (P211, P212, P213, P214, P216)),
                              ("v3.56.4", (P218_KUM,))):
            d = os.path.join(tmp, nazwa)
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
            for p in patche:
                naloz(d, p)
            plik = os.path.join(d, "main_centrala.cpp")
            sha = sha256_sciezki(plik)
            if sha != SHA[nazwa]:
                raise SystemExit("%s ma SHA %s, oczekiwano %s" % (nazwa, sha, SHA[nazwa]))
            cel = os.path.join(docel, "main_centrala-%s.cpp" % nazwa)
            shutil.copyfile(plik, cel)
            with open(cel, "r", encoding="utf-8", newline="") as f:
                tresc = f.read()
            linie = len(tresc.split("\n"))   # tak licza poprzednie etapy
            if nazwa in WERSJE and linie != WERSJE[nazwa]:
                raise SystemExit("%s ma %d linii, oczekiwano %d" % (nazwa, linie, WERSJE[nazwa]))
            opis[nazwa] = {"sha256": sha, "linii": linie, "plik": os.path.basename(cel)}
            print("PASS build219 %s sha256 %s (%d linii)" % (nazwa, sha[:16], linie))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    with open(os.path.join(docel, "stany.json"), "w", encoding="utf-8") as f:
        f.write(json.dumps(opis, indent=2) + "\n")
    return opis


if __name__ == "__main__":
    cel = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TU, "STANY")
    zbuduj(cel)
