#!/usr/bin/env python3
"""216: patch v3.56.2 -> v3.56.3 - DWA BLEDY NA STYKU PANELU Z TRASA.

CO BYLO ZLE (znalezione testem "od przycisku do pliku", nie testami stron):

  D2 - przycisk "Dluga (archiwum)" nie dzialal NIGDY.
       Panel robil:  GET /history/link?stream=long  -> bilet na strumien DLUGI
                     otwarcie /history?k=<bilet>&s=long
       Trasa /history podaje ZAWSZE strumien KROTKI (tak jest zarejestrowana:
       obs luz(request, Strumien::Krotki)), a bilet jest zwiazany ze strumieniem
       (Bilety::zuzyj porownuje strumien). Bilet na "dluga" na trasie "krotkiej"
       nie pasuje -> 401 "Link wygasl albo zostal juz uzyty". Uzytkownik widzi
       komunikat o wygasnieciu linku, ktory nigdy nie dzialal.
       Kazda strona z osobna byla przetestowana: link(stream=long) dawal bilet,
       /history/long z biletem dlugim dawal 200. Nie bylo testu, ktory bierze
       adres DOKLADNIE z panelu i podaje go trasie.

  D3 - pobrany plik nie mial nazwy.
       Odpowiedz CSV nie miala naglowka Content-Disposition, wiec przegladarka
       (szczegolnie w telefonie) pokazywala CSV jako tekst albo zapisywala plik
       bez rozszerzenia. "Pobieranie" konczylo sie widokiem na ekranie.
       Poprawka jest w HistoriaPobranie.h v3.56.3 (ten sam katalog).

CO ROBI TEN PATCH:
  1. panel wybiera trase wedlug strumienia: /history albo /history/long,
     i nie dokleja juz parametru s= (trasa go nie czytala),
  2. podnosi wersje na v3.56.3.
  Uklad include'ow i cala reszta pliku zostaje bajt w bajt bez zmian.

Skrypt nie pisze patcha recznie: bierze przypiety plik v3.55.8, naklada
211 + 212 + 213 + 214, poprawia panel, podnosi wersje i zapisuje diff.

Uruchomienie:
    python3 -B narzedzia/od-przycisku216/zrob_patch.py
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

P211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
P212 = os.path.join(NARZ, "pobranie-trasa212", "main_centrala-v3.55.9-to-v3.56.0.patch")
P213 = os.path.join(NARZ, "panel-pobrania213", "main_centrala-v3.56.0-to-v3.56.1.patch")
P214 = os.path.join(NARZ, "brama-przed-wgraniem214", "main_centrala-v3.56.1-to-v3.56.2.patch")

WYJSCIE_INC = os.path.join(TU, "main_centrala-v3.56.2-to-v3.56.3.patch")
WYJSCIE_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.3.patch")

# HistoriaPobranie.h: wersja z etapu 213 (v3.56.2 jej nie zmienial).
NAGLOWEK_WE = os.path.join(NARZ, "panel-pobrania213", "HistoriaPobranie.h")
NAGLOWEK_WY = os.path.join(TU, "HistoriaPobranie.h")

NAGLOWEK_STARY = ("    zajeta_ = true;\n"
                  "    request->send(odp);\n")
NAGLOWEK_NOWY = ("    // [v3.56.3 HISTORIA-PLIK] Nazwa pliku dla przegladarki i telefonu.\n"
                 "    // Bez \"attachment\" Chrome i Safari pokazuja CSV jako tekst na\n"
                 "    // stronie (albo zapisuja plik bez rozszerzenia) - pobieranie konczy\n"
                 "    // sie widokiem na ekranie zamiast plikiem w \"Pobranych\".\n"
                 "    odp->addHeader(\"Content-Disposition\",\n"
                 "                   strumien == Strumien::Dluga\n"
                 "                       ? \"attachment; filename=\\\"historia-dluga.csv\\\"\"\n"
                 "                       : \"attachment; filename=\\\"historia-krotka.csv\\\"\");\n"
                 "    zajeta_ = true;\n"
                 "    request->send(odp);\n")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"

# --- co dokladnie zmieniamy w panelu --------------------------------------
OPIS_STARY = ("     jednorazowy bilet (GET /history/link), a potem otwiera "
              "/history?k=<bilet>.\n")
OPIS_NOWY = ("     jednorazowy bilet (GET /history/link), a potem otwiera trase\n"
             "     swojego strumienia: /history?k=<bilet> (krotka) albo\n"
             "     /history/long?k=<bilet> (dluga).\n")

JS_STARY = ("        window.location.href = '/history?k=' + encodeURIComponent(bilet)"
            " + '&s=' + strumien;\n")
JS_NOWY = ("        // [v3.56.3] Trasa musi byc ta sama, dla ktorej wydano bilet:\n"
           "        // /history podaje ZAWSZE krotka, /history/long - dluga. Bilet\n"
           "        // na dluga na trasie krotkiej nie pasuje i dawal 401.\n"
           "        var trasa = (strumien === 'long') ? '/history/long' : '/history';\n"
           "        window.location.href = trasa + '?k=' + encodeURIComponent(bilet);\n")

KOMENTARZ = """// FIRMWARE_VERSION v3.56.2 -> v3.56.3 (KANDYDAT: przycisk "Dluga" i nazwa pliku CSV; NIE WGRYWAC).
// v3.56.3: dwa bledy na styku panelu z trasa, niewidoczne w testach kazdej
// strony osobno - znalazl je test "od przycisku do pliku":
//  1) przycisk "Dluga (archiwum)" otwieral /history?k=<bilet>, choc bilet byl
//     wydany dla strumienia dlugiego. Trasa /history podaje ZAWSZE krotka,
//     wiec bilet nie pasowal i panel dostawal 401 "link wygasl". Teraz panel
//     wybiera trase wedlug strumienia: /history albo /history/long.
//  2) odpowiedz CSV nie miala Content-Disposition, wiec telefon pokazywal tekst
//     zamiast zapisac plik (poprawka w HistoriaPobranie.h v3.56.3).
// Uklad include'ow z v3.56.2 i cala reszta pliku zostaja bez zmian.
"""


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


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "--no-backup-if-mismatch", "-i", patch],
                       cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s"
                         % (os.path.basename(patch), t.stdout, t.stderr))


def bez_znacznikow_czasu(diff):
    """diff wypisuje czasy plikow - usuwamy je, zeby patch byl powtarzalny."""
    wynik = []
    for linia in diff.split("\n"):
        if linia.startswith("--- ") or linia.startswith("+++ "):
            linia = linia.split("\t")[0]
        wynik.append(linia)
    return "\n".join(wynik)


def popraw_panel(tekst):
    for fragment, nowy, opis in ((OPIS_STARY, OPIS_NOWY, "opis panelu"),
                                 (JS_STARY, JS_NOWY, "adres pobrania w JS")):
        if tekst.count(fragment) != 1:
            raise SystemExit("oczekiwano dokladnie jednego wystapienia: %s" % opis)
        tekst = tekst.replace(fragment, nowy, 1)
    return tekst


def podnies_wersje(tekst):
    stara = 'static const char* FIRMWARE_VERSION = "v3.56.2";'
    if tekst.count(stara) != 1:
        raise SystemExit("brak FIRMWARE_VERSION v3.56.2")
    tekst = tekst.replace(stara, 'static const char* FIRMWARE_VERSION = "v3.56.3";', 1)
    return KOMENTARZ + tekst


def popraw_naglowek():
    """HistoriaPobranie.h v3.56.3: naglowek Content-Disposition na odpowiedzi."""
    tekst = wczytaj(NAGLOWEK_WE)
    if tekst.count("// [v3.56.3 HISTORIA-PLIK]") != 0:
        raise SystemExit("naglowek wejsciowy juz ma wstawe v3.56.3")
    if tekst.count(NAGLOWEK_STARY) != 1:
        raise SystemExit("oczekiwano dokladnie jednego 'zajeta_ = true; request->send(odp);'")
    tekst = tekst.replace(NAGLOWEK_STARY, NAGLOWEK_NOWY, 1)
    # Jeden podpis dla obu strumieni: sprawdzamy, ze nazwy plikow sa rozne.
    for nazwa in ("historia-krotka.csv", "historia-dluga.csv"):
        if nazwa not in tekst:
            raise SystemExit("brak nazwy pliku %s w naglowku" % nazwa)
    if tekst.count("Content-Disposition") != 1:
        raise SystemExit("Content-Disposition musi byc dodany dokladnie raz")
    zapisz(NAGLOWEK_WY, tekst)
    return tekst


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS patch216 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="p216-")
    try:
        baza = os.path.join(tmp, "a")
        stare = os.path.join(tmp, "b")   # v3.56.2
        nowe = os.path.join(tmp, "c")    # v3.56.3
        for d in (baza, stare, nowe):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(PRZYKLAD, os.path.join(d, "secrets.h.example"))
        for d in (stare, nowe):
            for p in (P211, P212, P213, P214):
                naloz(d, p)

        v3562 = wczytaj(os.path.join(stare, "main_centrala.cpp"))
        v3563 = podnies_wersje(popraw_panel(v3562))
        zapisz(os.path.join(nowe, "main_centrala.cpp"), v3563)

        d1 = subprocess.run(["diff", "-u", "-r", "b", "c"], cwd=tmp,
                            capture_output=True, text=True)
        if d1.returncode != 1:
            raise SystemExit("diff 216 zwrocil %d" % d1.returncode)
        zapisz(WYJSCIE_INC, bez_znacznikow_czasu(d1.stdout))

        d2 = subprocess.run(["diff", "-u", "-r", "a", "c"], cwd=tmp,
                            capture_output=True, text=True)
        if d2.returncode != 1:
            raise SystemExit("diff skumulowany zwrocil %d" % d2.returncode)
        zapisz(WYJSCIE_KUM, bez_znacznikow_czasu(d2.stdout))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    for plik in (WYJSCIE_INC, WYJSCIE_KUM):
        tresc = wczytaj(plik)
        plus = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
        minus = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
        hunki = sum(1 for l in tresc.split("\n") if l.startswith("@@"))
        print("PASS patch216 %s +%d -%d hunki=%d"
              % (os.path.basename(plik), plus, minus, hunki))

    naglowek = popraw_naglowek()
    print("PASS patch216 HistoriaPobranie.h v3.56.3 %d linii (Content-Disposition + 2 nazwy)"
          % len(naglowek.split("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
