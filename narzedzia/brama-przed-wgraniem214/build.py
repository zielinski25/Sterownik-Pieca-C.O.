#!/usr/bin/env python3
"""214: naloz patche i przepusc plik przez brame.

Robi cztery rzeczy, kazda sprawdzalna:
  1. bierze przypiety plik v3.55.8 (SHA-256),
  2. naklada patch skumulowany (3.55.8 -> 3.56.2) ORAZ lancuch
     211 + 212 + 213 + 214 i porownuje pliki bajt w bajt,
  3. sprawdza, ze patch 214 jest ODWRACALNY na wyniku (czyli plik jest
     dokladnie tym, co opisuje patch), a nie "mniej wiecej",
  4. przepuszcza wynik przez brame sprawdz.py (S01..S18) i zapisuje
     RAPORT.md + KONTROLA.json.

Uruchomienie:
    python3 -B narzedzia/brama-przed-wgraniem214/build.py
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

import sprawdz
import zrob_patch as Z

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
NARZ = os.path.join(REPO, "narzedzia")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PRZYKLAD = os.path.join(FIRMWARE, "secrets.h.example")

PATCH_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.2.patch")
PATCH_INC = os.path.join(TU, "main_centrala-v3.56.1-to-v3.56.2.patch")

NAGLOWKI_ZRODLA = {
    "HistoriaPin.h": os.path.join(NARZ, "wysylka-zapinka211", "HistoriaPin.h"),
    "HistoriaPobranie.h": os.path.join(NARZ, "panel-pobrania213", "HistoriaPobranie.h"),
    "HistoriaBilety.h": os.path.join(NARZ, "panel-pobrania213", "HistoriaBilety.h"),
}


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def naloz(katalog, patch, odwrotnie=False):
    cmd = ["patch", "-p1", "--no-backup-if-mismatch", "-i", patch]
    if odwrotnie:
        cmd.insert(1, "-R")
        cmd.insert(2, "--dry-run")
    return subprocess.run(cmd, cwd=katalog, capture_output=True, text=True)


def main():
    if sha256_sciezki(ZRODLO) != Z.SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS brama214 zrodlo v3.55.8 sha256 zgodny z 210")

    # --- 1. patch skumulowany vs lancuch --------------------------------
    tmp = tempfile.mkdtemp(prefix="b214-")
    przyklad_po = ""
    try:
        kum = os.path.join(tmp, "kum")
        lan = os.path.join(tmp, "lan")
        for d in (kum, lan):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        if naloz(kum, PATCH_KUM).returncode != 0:
            raise SystemExit("patch skumulowany sie nie nalozyl")
        for p in (Z.P211, Z.P212, Z.P213, PATCH_INC):
            t = naloz(lan, p)
            if t.returncode != 0:
                raise SystemExit("patch %s sie nie nalozyl: %s" % (os.path.basename(p), t.stdout))
        r_kum = wczytaj(os.path.join(kum, "main_centrala.cpp"))
        r_lan = wczytaj(os.path.join(lan, "main_centrala.cpp"))
        przyklad_po = wczytaj(os.path.join(kum, "secrets.h.example"))
        if r_kum != r_lan:
            raise SystemExit("patch skumulowany != lancuch 211+212+213+214")
        print("PASS brama214 patch skumulowany == 211+212+213+214 (bajt w bajt)")
        print("PASS brama214 wynik sha256 %s" % hashlib.sha256(r_kum.encode()).hexdigest())

        # --- 2. odwracalnosc -------------------------------------------------
        t = naloz(kum, PATCH_KUM, odwrotnie=True)
        odwracalny = t.returncode == 0
        print("%s brama214 patch odwracalny: %s" % ("PASS" if odwracalny else "FAIL",
                                                    "tak" if odwracalny else t.stdout.strip()[:200]))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    # --- 3. brama --------------------------------------------------------
    naglowki = {n: wczytaj(p) for n, p in NAGLOWKI_ZRODLA.items()}
    wyniki = sprawdz.uruchom(
        r_kum, naglowki, przyklad_po,
        z_kompilacja=True, katalog_zrodla=None, odwracalny=odwracalny)

    bledy = [x for x in wyniki if not x["ok"]]
    for x in wyniki:
        print("%s %s  %s  (%s)" % ("PASS" if x["ok"] else "FAIL", x["id"], x["nazwa"], x["opis"]))

    raport = ["# Raport bramy przed wgraniem (etap 214)", "",
              "Plik po patchu: `main_centrala-v3.55.8-to-v3.56.2.patch`",
              "SHA-256 wyniku: `%s`" % hashlib.sha256(r_kum.encode()).hexdigest(), "",
              "| # | kontrola | wynik | szczegoly |", "|---|---|---|---|"]
    for x in wyniki:
        raport.append("| %s | %s | %s | %s |" % (x["id"], x["nazwa"],
                                                 "OK" if x["ok"] else "**BLAD**", x["opis"]))
    raport.append("")
    raport.append("Bledy: %d" % len(bledy))
    with open(os.path.join(TU, "RAPORT.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(raport) + "\n")

    with open(os.path.join(TU, "KONTROLA.json"), "w", encoding="utf-8") as f:
        json.dump({"status": "PASS" if not bledy else "FAIL",
                   "stage": 214,
                   "release_ready": False,
                   "main_changed": False,
                   "device_touched": False,
                   "checks": wyniki,
                   "errors": len(bledy),
                   "patched_sha256": hashlib.sha256(r_kum.encode()).hexdigest()}, f,
                  indent=2, ensure_ascii=False)
        f.write("\n")

    if bledy:
        print("\nFAIL brama214 sprawdzen=%d bledow=%d" % (len(wyniki), len(bledy)))
        return 1
    print("\nPASS brama214 sprawdzen=%d bledow=0" % len(wyniki))
    return 0


if __name__ == "__main__":
    sys.exit(main())
