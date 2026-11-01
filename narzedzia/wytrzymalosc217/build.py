#!/usr/bin/env python3
"""217: zbuduj plik po patchu v3.56.3 i wyciagnij z niego PRAWDZIWE kawalki
do testu wytrzymalosci (soak) - trasy HTTP ORAZ oba wysylacze Telegram.

Ten etap nie zmienia firmware. Bierze przypiety plik v3.55.8, naklada patch
skumulowany v3.55.8 -> v3.56.3 (SHA-256 sprawdzany wobec etapu 216), a potem
wyciaga z niego - przez dopasowanie nawiasow, nie przez przepisywanie:

  * adaptery losowosci/zegara (213),
  * bramke i obiekty trasy (212/213),
  * trzy lambdy tras (212/213),
  * oba wysylacze dokumentow Telegram (211).

Wynik: Extracted217.inc (do Test.cpp) i raport pokrycia: ktore linie dodane
przez patche 211/212/213/214/216 sa skompilowane w ktorym etapie.

Uruchomienie:
    python3 -B narzedzia/wytrzymalosc217/build.py
"""
import hashlib
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

PATCH_KUM = os.path.join(NARZ, "od-przycisku216", "main_centrala-v3.55.8-to-v3.56.3.patch")
SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
SHA_WYNIKU = "f51b0b0749e2c1e3a8b5c02848a9c0e4902e0b641d6a7313a42304bbbcef91b3"

WYJSCIE = os.path.join(TU, "Extracted217.inc")


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def zapisz(p, t):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(t)


def funkcja(tekst, sygnatura):
    """Wytnij cala definicje funkcji (z bilansowaniem nawiasow i pomijaniem
    napisow, znakow i komentarzy) - zeby wyciagnac DOKLADNIE ten kod."""
    i = tekst.index(sygnatura)
    k = tekst.index("{", i)
    glebokosc = 0
    while True:
        c = tekst[k]
        if c == "{":
            glebokosc += 1
        elif c == "}":
            glebokosc -= 1
            if glebokosc == 0:
                return tekst[i:k + 1]
        elif c == '"' or c == "'":
            cytat = c
            k += 1
            while tekst[k] != cytat:
                if tekst[k] == "\\":
                    k += 1
                k += 1
        elif c == "/" and tekst[k + 1] == "/":
            while tekst[k] != "\n":
                k += 1
        elif c == "/" and tekst[k + 1] == "*":
            k = tekst.index("*/", k) + 1
        k += 1


def blok(tekst, od, do):
    i = tekst.find(od)
    if i < 0:
        raise SystemExit("217: brak fragmentu: %r" % od[:60])
    j = tekst.find(do, i)
    if j < 0:
        raise SystemExit("217: brak konca fragmentu: %r" % do[:60])
    return tekst[i:j + len(do)]


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("217: plik zrodlowy nie jest tym z etapu 210")
    print("PASS build217 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="b217-")
    try:
        katalog = os.path.join(tmp, "v3563")
        os.makedirs(katalog)
        shutil.copyfile(ZRODLO, os.path.join(katalog, "main_centrala.cpp"))
        shutil.copyfile(PRZYKLAD, os.path.join(katalog, "secrets.h.example"))
        t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", PATCH_KUM],
                           cwd=katalog, capture_output=True, text=True)
        if t.returncode != 0:
            raise SystemExit("217: patch v3.56.3 sie nie nalozyl: %s%s" % (t.stdout, t.stderr))
        tekst = wczytaj(os.path.join(katalog, "main_centrala.cpp"))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    if hashlib.sha256(tekst.encode()).hexdigest() != SHA_WYNIKU:
        raise SystemExit("217: plik po patchu nie ma SHA z etapu 216")
    print("PASS build217 plik po patchu v3.56.3 sha256 zgodny z etapem 216")

    # --- kawalki do testu -----------------------------------------------
    i = tekst.index("static void historiaLosuj213")
    j = tekst.index("static historia213::Bilety historiaBilety", i)
    adaptery = tekst[i:j].rstrip() + "\n"

    i = tekst.index("struct HistoriaBramka212 : historia212::Bramka {")
    j = tekst.index("static historia212::Pobranie historiaPobranie", i)
    k = tekst.index("\n", j) + 1
    bramka = tekst[i:k].rstrip() + "\n"

    sendery = []
    for sygnatura in ("bool sendTelegramDocumentHistKrotka() {",
                      "bool sendTelegramDocumentHistDluga() {"):
        sendery.append(funkcja(tekst, sygnatura))
        if len(sendery[-1].split("\n")) < 100:
            raise SystemExit("217: %s wyciagnieta za krotka (%d linii)"
                             % (sygnatura, len(sendery[-1].split("\n"))))
    if "pin.read(chBuf, DOC_CHUNK)" not in sendery[0]:
        raise SystemExit("217: wyciaganie nie trafilo w nowe cialo wysylki")

    import re
    wzor = re.compile(r'wifiWebServer\.on\("([^"]+)",\s*HTTP_(\w+),\s*'
                      r'\[\]\(AsyncWebServerRequest \*request\) \{\n(.*?)\n  \}\);', re.S)
    nazwy = {"/history": "obslugaHistory", "/history/long": "obslugaHistoryLong",
             "/history/link": "obslugaHistoryLink"}
    rej = {s: (m, c, nazwy[s]) for s, m, c in wzor.findall(tekst) if s in nazwy}
    for s in nazwy:
        if s not in rej:
            raise SystemExit("217: brak rejestracji %s" % s)

    czesci = [
        "// WYGENEROWANE przez narzedzia/wytrzymalosc217/build.py - nie edytowac.\n",
        "// Zrodlo: firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp\n",
        "//         + narzedzia/od-przycisku216/main_centrala-v3.55.8-to-v3.56.3.patch\n",
        "//         (sha256 %s)\n" % sha256_sciezki(PATCH_KUM),
        "// Ponizej PRAWDZIWY kod z pliku po patchu v3.56.3 - bez zadnych zmian.\n\n",
        "// ---- oba wysylacze dokumentow Telegram (dodane w etapie 211) --------\n",
        sendery[0], "\n\n", sendery[1], "\n\n",
        "// ---- adaptery losowosci i zegara (213) -----------------------------\n",
        adaptery, "\n",
        "// ---- bramka i obiekt trasy (212/213) -------------------------------\n",
        bramka, "\n",
    ]
    for s in ("/history", "/history/long", "/history/link"):
        metoda, cialo, nazwa = rej[s]
        czesci.append("// %s (%s) - tresc lambdy z main_centrala.cpp\n" % (s, metoda))
        czesci.append("static void %s(AsyncWebServerRequest *request) {\n%s\n}\n\n" % (nazwa, cialo))
    czesci.append("// Sciezka -> obsluga, w kolejnosci rejestracji w main_centrala.cpp.\n")
    czesci.append("struct Trasa217 {\n  const char *sciezka;\n"
                  "  void (*obsluz)(AsyncWebServerRequest *);\n};\n")
    czesci.append("static const Trasa217 TRASY_OBSLUGI217[] = {\n")
    for s in ("/history", "/history/long", "/history/link"):
        czesci.append('  {"%s", %s},\n' % (s, rej[s][2]))
    czesci.append("};\nstatic const int TRASY_OBSLUGI217_N = 3;\n")

    tresc = "".join(czesci)
    zapisz(WYJSCIE, tresc)
    print("PASS build217 Extracted217.inc %d linii (w tym oba wysylacze: %d i %d)"
          % (len(tresc.split("\n")), len(sendery[0].split("\n")), len(sendery[1].split("\n"))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
