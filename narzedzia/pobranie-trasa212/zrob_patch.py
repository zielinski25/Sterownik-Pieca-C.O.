#!/usr/bin/env python3
"""212: wygeneruj patch v3.55.8 -> v3.56.0 (trasa pobrania historii w centrali).

Patch NIE jest pisany recznie. Skrypt:
  1. bierze przypiety plik v3.55.8 (SHA-256 sprawdzany),
  2. naklada na niego patch etapu 211 (v3.55.8 -> v3.55.9) - bez tego trasa
     pobrania czytalaby plik inaczej niz wysylacze Telegramu,
  3. dokłada TRZY nowe fragmenty (include, sekcja tokenu, bramka + rejestracja),
  4. zapisuje wynik jako skumulowany diff v3.55.8 -> v3.56.0 ORAZ jako maly
     diff v3.55.9 -> v3.56.0 (dowod, ze 211 zostalo nietkniete).

Uruchomienie (Linux, Python 3):
    python3 -B narzedzia/pobranie-trasa212/zrob_patch.py
"""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
FIRMWARE = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")
ZRODLO = os.path.join(FIRMWARE, "main_centrala.cpp")
PATCH211 = os.path.join(REPO, "narzedzia", "wysylka-zapinka211",
                        "main_centrala-v3.55.8-to-v3.55.9.patch")
WYJSCIE = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.0.patch")
WYJSCIE_INC = os.path.join(TU, "main_centrala-v3.55.9-to-v3.56.0.patch")
SHA_ZRODLA211 = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"
PRZYKLAD = os.path.join(FIRMWARE, "secrets.h.example")

# Wpis w secrets.h.example: sam token, bez wartosci-domyslnej. Pusto = trasa
# pobrania wylaczona, wiec brak wpisu u uzytkownika niczego nie otwiera.
WPIS_PRZYKLADU = """
// ----- 5. TOKEN POBRANIA HISTORII -------------------------------------
// Token do pobrania plikow historii przez HTTP (/history, /history/long).
// Bez niego te dwa adresy sa ZAMKNIETE (503). Token jedzie w naglowku
// "Authorization: Bearer <token>". Minimum 32 znaki, inne niz pozostale.
// Wpisz tu wlasny, losowy ciag; nie wklejaj go do gita ani na czat.
#define SECRET_HISTORIA_TOKEN   "ZMIEN-MNIE-NA-DLUGI-LOSOWY-TOKEN-32-ZNAKI"
"""

BLOK_BRAMKI = '''// [v3.56.0 HISTORIA-POBRANIE] Bramka trasy pobrania: jedyne miejsce, ktore
// siega po logMutex, historiaSys i cache rozmiarow historii. Trasa sama nie
// zna zadnej zmiennej globalnej - dzieki temu jest testowalna hostowo.
struct HistoriaBramka212 : historia212::Bramka {
  history193::Calls &io() override { return historiaSys; }
  bool magazynGotowy() override { return historiaDostepGotowy() && littlefsReady; }
  std::uint64_t potwierdzony(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? static_cast<std::uint64_t>(g_histShortSize)
                                                : static_cast<std::uint64_t>(g_histLongSize);
  }
  const char *sciezka(historia212::Strumien s) override {
    return (s == historia212::Strumien::Krotki) ? historia211::kShortVfsPath
                                                : historia211::kLongVfsPath;
  }
  bool zamknij() override {
    if (!logMutex) return false;
    return xSemaphoreTake(logMutex, pdMS_TO_TICKS(200)) == pdTRUE;
  }
  void otworz() override { xSemaphoreGive(logMutex); }
};
static HistoriaBramka212 historiaBramka;
static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);

// [v3.56.0 HISTORIA-POBRANIE] Dwa punkty pobrania: /history (krotka) i
// /history/long (dluga). HTTP_ANY, bo metode sprawdzamy sami - inaczej inna
// metoda dostalaby ciche 404 zamiast 405. Odpowiedz odmowy jest prawdziwa
// odpowiedzia HTTP, nie zerwaniem polaczenia.
'''

BLOK_REJESTRACJI = '''  wifiWebServer.on("/history", HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);
  });
  wifiWebServer.on("/history/long", HTTP_ANY, [](AsyncWebServerRequest *request) {
    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);
  });

'''

BLOK_TOKENU = '''// [v3.56.0 HISTORIA-POBRANIE] Token pobierania historii. Pusty = pobieranie
// wylaczone (trasa odpowiada 503), zeby starsza kopia secrets.h nie otworzyla
// plikow historii bez tokenu. Wartosc pochodzi z secrets.h.
#ifndef SECRET_HISTORIA_TOKEN
  #define SECRET_HISTORIA_TOKEN ""
#endif
static const char* HISTORIA_TOKEN = SECRET_HISTORIA_TOKEN;

'''


def sha256_sciezki(p):
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for blok in iter(lambda: f.read(1 << 20), b""):
            h.update(blok)
    return h.hexdigest()


def wczytaj(p):
    with open(p, "r", encoding="utf-8", newline="") as f:
        return f.read()


def zapisz(p, s):
    with open(p, "w", encoding="utf-8", newline="") as f:
        f.write(s)


def podmien(tekst, stary, nowy, opis):
    if tekst.count(stary) != 1:
        raise SystemExit("%s: oczekiwano jednego wystapienia, jest %d" % (opis, tekst.count(stary)))
    return tekst.replace(stary, nowy, 1)


def dodaj_212(tekst):
    """Trzy nowe fragmenty na pliku v3.55.9 (po patchu 211)."""
    tekst = podmien(
        tekst,
        "// FIRMWARE_VERSION v3.55.8 -> v3.55.9 (KANDYDAT: zapinka historii w wysylaczach "
        "dokumentow Telegram; NIE WGRYWAC).\n",
        "// FIRMWARE_VERSION v3.55.9 -> v3.56.0 (KANDYDAT: trasa pobrania historii "
        "/history i /history/long, za tokenem; NIE WGRYWAC).\n"
        "// v3.56.0: pobranie historii przez HTTP dziala na tej samej zapince co wysylacze\n"
        "// Telegramu (etap 211): dlugosc pod logMutexem, plik otwarty raz, tylko potwierdzony\n"
        "// prefiks. Trasa jest wylaczona, dopoki w secrets.h nie ma SECRET_HISTORIA_TOKEN.\n"
        "// Zapis historii, rotacja, CSV, NVS, OTA, sterowanie i rytm 8s/60s bez zmian.\n"
        "// FIRMWARE_VERSION v3.55.8 -> v3.55.9 (KANDYDAT: zapinka historii w wysylaczach "
        "dokumentow Telegram; NIE WGRYWAC).\n",
        "naglowek wersji")

    tekst = podmien(
        tekst,
        '#include "HistoriaPin.h"   // [v3.55.9 HISTORIA-PIN] zapinka prefiksu historii\n',
        '#include "HistoriaPin.h"   // [v3.55.9 HISTORIA-PIN] zapinka prefiksu historii\n'
        '#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n',
        "include")

    tekst = podmien(
        tekst,
        '#ifndef SECRET_CMD_TOKEN\n'
        '  #error "secrets.h jest nieaktualny: brak SECRET_CMD_TOKEN. Skopiuj nowe wpisy z '
        'secrets.h.example."\n'
        '#endif\n',
        '#ifndef SECRET_CMD_TOKEN\n'
        '  #error "secrets.h jest nieaktualny: brak SECRET_CMD_TOKEN. Skopiuj nowe wpisy z '
        'secrets.h.example."\n'
        '#endif\n\n' + BLOK_TOKENU,
        "sekcja tokenu")

    tekst = podmien(
        tekst,
        '  wifiWebServer.on("/api/log-download", HTTP_GET, [](AsyncWebServerRequest *request) {\n',
        BLOK_BRAMKI + BLOK_REJESTRACJI +
        '  wifiWebServer.on("/api/log-download", HTTP_GET, [](AsyncWebServerRequest *request) {\n',
        "rejestracja trasy")

    tekst = podmien(tekst, 'static const char* FIRMWARE_VERSION = "v3.55.9";',
                    'static const char* FIRMWARE_VERSION = "v3.56.0";', "FIRMWARE_VERSION")
    return tekst


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA211:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    tmp = tempfile.mkdtemp(prefix="p212-")
    try:
        a = os.path.join(tmp, "a")
        b = os.path.join(tmp, "b")
        os.makedirs(a)
        os.makedirs(b)
        zapisz(os.path.join(a, "main_centrala.cpp"), wczytaj(ZRODLO))
        shutil.copyfile(ZRODLO, os.path.join(b, "main_centrala.cpp"))
        shutil.copyfile(PRZYKLAD, os.path.join(a, "secrets.h.example"))
        shutil.copyfile(PRZYKLAD, os.path.join(b, "secrets.h.example"))
        naloz = subprocess.run(["patch", "-p1", "-i", PATCH211], cwd=b, capture_output=True, text=True)
        if naloz.returncode != 0:
            raise SystemExit("patch 211 nie nalozyl sie: %s%s" % (naloz.stdout, naloz.stderr))
        v3559 = wczytaj(os.path.join(b, "main_centrala.cpp"))
        v3560 = dodaj_212(v3559)
        zapisz(os.path.join(b, "main_centrala.cpp"), v3560)
        zapisz(os.path.join(tmp, "v3559.cpp"), v3559)

        # secrets.h.example: dopisz sekcje 5 (tylko przyklad, bez sekretu)
        przyklad = wczytaj(os.path.join(b, "secrets.h.example"))
        if "SECRET_HISTORIA_TOKEN" in przyklad:
            raise SystemExit("secrets.h.example juz ma SECRET_HISTORIA_TOKEN")
        zapisz(os.path.join(b, "secrets.h.example"), przyklad.rstrip("\n") + "\n" + WPIS_PRZYKLADU)

        # skumulowany diff v3.55.8 -> v3.56.0
        diff = subprocess.run(["diff", "-u", "-r", "a", "b"], cwd=tmp, capture_output=True, text=True)
        if diff.returncode != 1:
            raise SystemExit("diff zwrocil %d" % diff.returncode)
        zapisz(WYJSCIE, diff.stdout)

        # maly diff v3.55.9 -> v3.56.0
        c = os.path.join(tmp, "c")
        os.makedirs(c)
        zapisz(os.path.join(c, "main_centrala.cpp"), vczytaj_bezpiecznie(os.path.join(tmp, "v3559.cpp")))
        shutil.copyfile(PRZYKLAD, os.path.join(c, "secrets.h.example"))
        diff2 = subprocess.run(["diff", "-u", "-r", "c", "b"], cwd=tmp, capture_output=True, text=True)
        if diff2.returncode != 1:
            raise SystemExit("diff 212 zwrocil %d" % diff2.returncode)
        zapisz(WYJSCIE_INC, diff2.stdout)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    for plik in (WYJSCIE, WYJSCIE_INC):
        tresc = wczytaj(plik)
        plus = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
        minus = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
        hunki = sum(1 for l in tresc.split("\n") if l.startswith("@@"))
        print("PASS patch212 %s +%d -%d hunki=%d" %
              (os.path.basename(plik), plus, minus, hunki))
    return 0


def vczytaj_bezpiecznie(p):
    return wczytaj(p)


if __name__ == "__main__":
    sys.exit(main())
