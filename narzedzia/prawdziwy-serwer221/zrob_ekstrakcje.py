#!/usr/bin/env python3
# [ETAP 221] Wyciaga NASZ kod z pliku, ktory użytkownik wgrywa (v3.56.4).
#
# Zasada: nic nie przepisujemy recznie. Kazdy fragment, ktory test uruchamia,
# jest DOSLOWNIE wyciagniety z main_centrala.cpp v3.56.4 (odbudowanego z
# v3.55.8 + patch 218, z kontrola SHA). Jesli ktos zmieni plik firmware, ten
# skrypt to wykryje i test przestanie byc aktualny.
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
BAZA = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie", "main_centrala.cpp")
PATCH = os.path.join(REPO, "narzedzia", "panel-trasa218", "main_centrala-v3.55.8-to-v3.56.4.patch")

OCZEKIWANE = {
    "v3564_sha256": "f89874f3390009acd79632db1031d04cd90e67435eda68e2007ea7c4b3168e38",
    "baza_sha256": "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31",
    "historiapobranie_sha256": "b58d1a4c299e339798d3f229ad81177b0ee34f44dd37dae1776ba48279a4ede8",
}

# Granice wyciaganych fragmentow (dokladne napisy z v3.56.4).
BRAMKA_START = "struct HistoriaBramka212 : historia212::Bramka {"
BRAMKA_KONIEC = "static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;"
TRASY_START = '  wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY, [](AsyncWebServerRequest *request) {'
TRASY_KONIEC = '    historiaPobranie.link(request);\n  });'


def sha(bajty):
    return hashlib.sha256(bajty).hexdigest()


def odbuduj_v3564(katalog):
    """v3.55.8 + patch 218 -> v3.56.4, z kontrola SHA obu stron."""
    with open(BAZA, "rb") as f:
        baza = f.read()
    if sha(baza) != OCZEKIWANE["baza_sha256"]:
        raise SystemExit("BLAD: plik bazowy v3.55.8 ma inny SHA niz zapisany")
    cel = os.path.join(katalog, "main_centrala.cpp")
    with open(cel, "wb") as f:
        f.write(baza)
    # Patch 218 zmienia dwa pliki: main_centrala.cpp i secrets.h.example.
    # Podkladamy oba w wersji bazowej, zeby patch nalozyl sie w calosci.
    shutil.copyfile(os.path.join(os.path.dirname(BAZA), "secrets.h.example"),
                    os.path.join(katalog, "secrets.h.example"))
    rc = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", PATCH],
                        cwd=katalog, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if rc.returncode != 0:
        raise SystemExit("BLAD: patch 218 nie nalozyl sie:\n" + rc.stdout.decode("utf-8", "replace"))
    with open(cel, "rb") as f:
        wynik = f.read()
    if sha(wynik) != OCZEKIWANE["v3564_sha256"]:
        raise SystemExit("BLAD: po patchu plik nie ma SHA v3.56.4")
    return wynik.decode("utf-8")


def wytnij(tekst, poczatek, koniec, nazwa):
    i = tekst.find(poczatek)
    if i < 0:
        raise SystemExit(f"BLAD: nie znalazlem poczatku {nazwa}")
    if tekst.find(poczatek, i + 1) >= 0:
        raise SystemExit(f"BLAD: poczatek {nazwa} wystepuje wiecej niz raz")
    j = tekst.find(koniec, i)
    if j < 0:
        raise SystemExit(f"BLAD: nie znalazlem konca {nazwa}")
    j += len(koniec)
    linia = tekst.count("\n", 0, i) + 1
    return tekst[i:j] + "\n", linia


def zrob(katalog):
    tekst = odbuduj_v3564(katalog)
    bramka, linia_b = wytnij(tekst, BRAMKA_START, BRAMKA_KONIEC, "bramka")
    trasy, linia_t = wytnij(tekst, TRASY_START, TRASY_KONIEC, "trasy")

    # Kontrole samego wyciecia - zeby nie wyciagnac zlego kawalka po cichu.
    if bramka.count("struct HistoriaBramka212") != 1:
        raise SystemExit("BLAD: bramka wycieta niejednoznacznie")
    if trasy.count("wifiWebServer.on(") != 3:
        raise SystemExit("BLAD: oczekuje dokladnie 3 rejestracji tras, jest %d" % trasy.count("wifiWebServer.on("))
    for trasa in ('"/history"', '"/history/long"', '"/history/link"'):
        if trasa not in trasy:
            raise SystemExit(f"BLAD: brak trasy {trasa} w wycietym fragmencie")
    if "AsyncURIMatcher::exact" not in trasy:
        raise SystemExit("BLAD: trasy nie uzywaja exact()")

    manifest = {
        "v3564_sha256": OCZEKIWANE["v3564_sha256"],
        "baza_sha256": OCZEKIWANE["baza_sha256"],
        "patch_sha256": sha(open(PATCH, "rb").read()),
        "bramka": {"linia": linia_b, "sha256": sha(bramka.encode("utf-8")), "bajtow": len(bramka.encode("utf-8"))},
        "trasy": {"linia": linia_t, "sha256": sha(trasy.encode("utf-8")), "bajtow": len(trasy.encode("utf-8"))},
    }
    return bramka, trasy, manifest


def main():
    katalog = tempfile.mkdtemp(prefix="v3564-")
    try:
        bramka, trasy, manifest = zrob(katalog)
    finally:
        shutil.rmtree(katalog, ignore_errors=True)

    nasz = os.path.join(TU, "nasz")
    os.makedirs(nasz, exist_ok=True)
    if "--zapisz" in sys.argv:
        with open(os.path.join(nasz, "bramka221.inc"), "w", encoding="utf-8") as f:
            f.write(bramka)
        with open(os.path.join(nasz, "trasy221.inc"), "w", encoding="utf-8") as f:
            f.write(trasy)
        with open(os.path.join(TU, "MANIFEST.json"), "w", encoding="utf-8") as f:
            json.dump(manifest, f, indent=2, ensure_ascii=False)
            f.write("\n")
        print("zapisano: nasz/bramka221.inc (%d B), nasz/trasy221.inc (%d B)" % (len(bramka.encode()), len(trasy.encode())))
        print("bramka z linii %d, trasy z linii %d" % (manifest["bramka"]["linia"], manifest["trasy"]["linia"]))
        return 0

    # Tryb kontroli: porownaj z tym, co jest w repo (uzywa run.py przed testem).
    try:
        with open(os.path.join(TU, "MANIFEST.json"), encoding="utf-8") as f:
            zapisany = json.load(f)
    except FileNotFoundError:
        print("BLAD: brak MANIFEST.json - uruchom z --zapisz")
        return 2
    for nazwa, tresc in (("bramka", bramka), ("trasy", trasy)):
        if zapisany[nazwa]["sha256"] != sha(tresc.encode("utf-8")):
            print(f"BLAD: {nazwa} w repo rozni sie od tekstu w v3.56.4")
            return 1
    print("OK ekstrakcja221: bramka i trasy zgodne z v3.56.4 (sha pliku %s...)" % manifest["v3564_sha256"][:16])
    return 0


if __name__ == "__main__":
    sys.exit(main())
