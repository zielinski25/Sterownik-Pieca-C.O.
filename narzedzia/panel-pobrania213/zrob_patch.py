#!/usr/bin/env python3
"""213: wygeneruj patch v3.56.0 -> v3.56.1 (przycisk pobrania historii w panelu).

Wszystko, co ten etap dokłada do firmware, jest WYGENEROWANE z plikow
zamrozonych w etapie 212, zeby nie było "ręcznie przepisanego" dokumentu:

  * main_centrala.cpp: bierzemy wynik patchy 211 + 212, dokładamy pięć miejsc
    (komentarz wersji, include, obiekt biletów, rejestrację /history/link,
    przycisk w panelu) i zapisujemy diff 3.56.0 -> 3.56.1 oraz skumulowany
    3.55.8 -> 3.56.1,
  * HistoriaPobranie.h: bierzemy plik z etapu 212 i dokładamy obsługę biletów,
  * host/Serwer213.h: bierzemy model serwera z etapu 212 i dokładamy parametry
    zapytania (hasParam/getParam), ktorych wymaga /history?k=...

Dzieki temu build.py moze udowodnic, ze 213 = 212 + dokladnie te wstawki.

Uruchomienie (Linux, Python 3):
    python3 -B narzedzia/panel-pobrania213/zrob_patch.py
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
D212 = os.path.join(NARZ, "pobranie-trasa212")
PATCH211 = os.path.join(NARZ, "wysylka-zapinka211", "main_centrala-v3.55.8-to-v3.55.9.patch")
PATCH212 = os.path.join(D212, "main_centrala-v3.55.9-to-v3.56.0.patch")
POBRANIA212 = os.path.join(D212, "HistoriaPobranie.h")
SERWER212 = os.path.join(D212, "host", "Serwer212.h")

WYJSCIE_INC = os.path.join(TU, "main_centrala-v3.56.0-to-v3.56.1.patch")
WYJSCIE_KUM = os.path.join(TU, "main_centrala-v3.55.8-to-v3.56.1.patch")
WYJSCIE_POBRANIA = os.path.join(TU, "HistoriaPobranie.h")
WYJSCIE_SERWER = os.path.join(TU, "host", "Serwer213.h")

SHA_ZRODLA = "3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31"


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


KOMENTARZ = """// FIRMWARE_VERSION v3.56.0 -> v3.56.1 (KANDYDAT: przycisk pobrania historii w panelu; NIE WGRYWAC).
// v3.56.1: panel WWW dostaje przycisk "Pobierz historie". Token z secrets.h NIE trafia
// do strony - przycisk prosi centrale o JEDNORAZOWY bilet (GET /history/link), ktory
// wygasa po 5 minutach i jest zwiazany ze strumieniem. Trasa /history przyjmuje wiec
// albo naglowek Bearer, albo ?k=<bilet>. Staly token, pliki CSV, zapis historii,
// NVS, OTA i rytm 8s/60s bez zmian.
"""

INCLUDE = ('#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n'
           '#include "HistoriaBilety.h"   // [v3.56.1 HISTORIA-BILETY] jednorazowe linki do pobrania\n')

OBIEKTY = """
// [v3.56.1 HISTORIA-BILETY] Zrodlo losowosci i zegar dla biletow. esp_random()
// to sprzetowy generator ESP32; zegar to millis() (32 bity, przewija sie co
// ~49 dni - porownania w Biletach sa na roznicy, wiec przewiniecie nie szkodzi).
static void historiaLosuj213(std::uint8_t *out, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(esp_random() & 0xFFu);
}
static std::uint32_t historiaZegar213() { return static_cast<std::uint32_t>(millis()); }
static historia213::Bilety historiaBilety(historiaLosuj213, historiaZegar213);

// Podpiecie biletow do trasy robi konstruktor ponizszego obiektu - w tej samej
// jednostce kompilacji kolejnosc inicjalizacji jest z gory na dol, wiec
// historiaPobranie (wyzej) jest juz zbudowane.
struct HistoriaBiletyPodpiecie213 {
  HistoriaBiletyPodpiecie213() { historiaPobranie.ustawBilety(&historiaBilety); }
};
static HistoriaBiletyPodpiecie213 historiaBiletyPodpiecie;
"""

REJESTRACJA = """
  // [v3.56.1 HISTORIA-BILETY] Panel prosi o bilet, dostaje 32 znaki hex i sam
  // otwiera /history?k=<bilet>. Tu NIE ma tokenu z secrets.h - tylko bilet.
  wifiWebServer.on("/history/link", HTTP_GET, [](AsyncWebServerRequest *request) {
    historiaPobranie.link(request);
  });
"""

PANEL = '''
<!-- [v3.56.1 HISTORIA-POBRANIE] Pobranie plikow historii (CSV) na komputer/telefon.
     Staly token z secrets.h NIE jest tu wklejony: przycisk prosi centrale o
     jednorazowy bilet (GET /history/link), a potem otwiera /history?k=<bilet>.
     Bilet wygasa po 5 minutach i dziala tylko raz. -->
<button id="histBtn" type="button" style="position:fixed;left:12px;bottom:12px;z-index:60;padding:10px 16px;border:0;border-radius:999px;background:#1f6feb;color:#fff;font-weight:700;font-size:14px;box-shadow:0 6px 18px rgba(0,0,0,.35);cursor:pointer">&#11015; HISTORIA CSV</button>
<div id="histPanel" style="display:none;position:fixed;left:12px;bottom:66px;z-index:60;width:290px;background:#111827;color:#e5e7eb;border:1px solid #374151;border-radius:14px;padding:14px;box-shadow:0 12px 32px rgba(0,0,0,.5)">
  <div style="font-weight:700;margin-bottom:10px">Pobierz historię</div>
  <button id="histShort" type="button" style="display:block;width:100%;margin-bottom:8px;padding:10px;border:0;border-radius:10px;background:#2563eb;color:#fff;font-weight:600;cursor:pointer">Krótka (24 h)</button>
  <button id="histLong" type="button" style="display:block;width:100%;padding:10px;border:0;border-radius:10px;background:#374151;color:#fff;font-weight:600;cursor:pointer">Długa (archiwum)</button>
  <div id="histInfo" role="status" style="margin-top:10px;font-size:12px;line-height:1.35;opacity:.85">Link jest jednorazowy i wygasa po 5 minutach. Jeśli pobieranie się nie zacznie, kliknij jeszcze raz.</div>
</div>
<script>
/* [v3.56.1 HISTORIA-POBRANIE] Obsluga przycisku pobrania. Celowo bez zaleznosci
   od reszty panelu: jesli ktorys element nie istnieje, nic sie nie dzieje. */
(function () {
  var przycisk = document.getElementById('histBtn');
  var panel = document.getElementById('histPanel');
  var info = document.getElementById('histInfo');
  if (!przycisk || !panel) return;
  function komunikat(t) { if (info) info.textContent = t; }
  function pobierz(strumien) {
    komunikat('Przygotowuję link…');
    fetch('/history/link?stream=' + strumien, { cache: 'no-store' })
      .then(function (r) { if (!r.ok) throw new Error('HTTP ' + r.status); return r.text(); })
      .then(function (bilet) {
        bilet = (bilet || '').trim();
        if (bilet.length !== 32) throw new Error('centrala nie dała biletu');
        window.location.href = '/history?k=' + encodeURIComponent(bilet) + '&s=' + strumien;
      })
      .catch(function (e) { komunikat('Nie udało się: ' + e.message); });
  }
  przycisk.addEventListener('click', function () {
    panel.style.display = (panel.style.display === 'none' ? 'block' : 'none');
  });
  var krotka = document.getElementById('histShort');
  var dluga = document.getElementById('histLong');
  if (krotka) krotka.addEventListener('click', function () { pobierz('short'); });
  if (dluga) dluga.addEventListener('click', function () { pobierz('long'); });
})();
</script>
'''


def dodaj_213(tekst):
    if "HISTORIA-BILETY" in tekst:
        raise SystemExit("plik juz ma wstawki 213")
    if tekst.count('#include "HistoriaPobranie.h"') != 1:
        raise SystemExit("brak jednoznacznego include'a trasy 212")
    tekst = KOMENTARZ + tekst
    tekst = tekst.replace('#include "HistoriaPobranie.h"   // [v3.56.0 HISTORIA-POBRANIE] trasa pobrania historii\n',
                          INCLUDE, 1)
    if tekst.count('static const char* FIRMWARE_VERSION = "v3.56.0";') != 1:
        raise SystemExit("brak FIRMWARE_VERSION v3.56.0")
    tekst = tekst.replace('static const char* FIRMWARE_VERSION = "v3.56.0";',
                          'static const char* FIRMWARE_VERSION = "v3.56.1";', 1)

    obiekty_po = ("static historia212::Pobranie historiaPobranie(historiaBramka, HISTORIA_TOKEN);\n")
    if tekst.count(obiekty_po) != 1:
        raise SystemExit("brak obiektu trasy 212")
    tekst = tekst.replace(obiekty_po, obiekty_po + OBIEKTY, 1)

    anchor = ('  wifiWebServer.on("/history/long", HTTP_ANY, [](AsyncWebServerRequest *request) {\n'
              '    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);\n'
              '  });\n')
    if tekst.count(anchor) != 1:
        raise SystemExit("brak rejestracji /history/long")
    tekst = tekst.replace(anchor, anchor + REJESTRACJA, 1)

    panel_anchor = "</aside>\n\n<script>"
    if tekst.count(panel_anchor) != 1:
        raise SystemExit("brak jednoznacznego miejsca na przycisk w panelu")
    tekst = tekst.replace(panel_anchor, "</aside>\n" + PANEL + "\n<script>", 1)
    return tekst


# --- HistoriaPobranie.h: 212 + obsluga biletow ---------------------------

POBRANIA_INCLUDE = '#include "HistoriaPin.h"\n#include "HistoriaBilety.h"\n'

POBRANIA_CTOR = '  Pobranie(Bramka &bramka, const char *token) : bramka_(bramka), token_(token) {}\n'
POBRANIA_SETTER = (POBRANIA_CTOR +
                   '\n'
                   '  // [v3.56.1 HISTORIA-BILETY] Trasa moze przyjmowac bilety z panelu.\n'
                   '  // nullptr = tylko naglowek Bearer (tak dzialal etap 212).\n'
                   '  void ustawBilety(historia213::Bilety *bilety) { bilety_ = bilety; }\n')

POBRANIA_AUTH_STARE = '''    const AsyncWebHeader *naglowek = request->getHeader("Authorization");
    const char *podany = nullptr;
    std::size_t podanaDlugosc = 0;
    if (naglowek == nullptr ||
        !wyciagnijToken(naglowek->value().c_str(), naglowek->value().length(), podany, podanaDlugosc) ||
        !tokenZgodny(podany, podanaDlugosc, token_)) {
      odpowiedzBledu(request, 401, "Brak albo bledny token", "WWW-Authenticate", "Bearer");
      return;
    }
'''

POBRANIA_AUTH_NOWE = '''    // [v3.56.1 HISTORIA-BILETY] Dwie drogi: naglowek Bearer (jak w 212) albo
    // bilet z panelu (?k=...). Bilet sprawdzamy DOPIERO po zajetosci i magazynie,
    // zeby nie spalic linku, gdy centrala jest chwilowo zajeta.
    const AsyncWebHeader *naglowek = request->getHeader("Authorization");
    const char *podany = nullptr;
    std::size_t podanaDlugosc = 0;
    bool przezBilet = false;
    if (naglowek != nullptr) {
      if (!wyciagnijToken(naglowek->value().c_str(), naglowek->value().length(), podany, podanaDlugosc) ||
          !tokenZgodny(podany, podanaDlugosc, token_)) {
        odpowiedzBledu(request, 401, "Brak albo bledny token", "WWW-Authenticate", "Bearer");
        return;
      }
    } else if (bilety_ != nullptr && request->hasParam("k")) {
      przezBilet = true;
    } else {
      odpowiedzBledu(request, 401, "Brak poswiadczen", "WWW-Authenticate", "Bearer");
      return;
    }
'''

POBRANIA_MAGAZYN = '''    if (!bramka_.magazynGotowy()) {
      odpowiedzBledu(request, 503, "Magazyn historii niegotowy", nullptr, nullptr);
      return;
    }

'''

POBRANIA_BILET = '''    if (przezBilet) {
      const String bilet = request->getParam("k")->value();
      if (!bilety_->zuzyj(bilet.c_str(), bilet.length(),
                          strumien == Strumien::Dluga ? historia213::kDluga : historia213::kKrotki)) {
        odpowiedzBledu(request, 401, "Link wygasl albo zostal juz uzyty - kliknij pobieranie jeszcze raz",
                       "WWW-Authenticate", "Bearer");
        return;
      }
    }
'''

POBRANIA_LINK = '''  // [v3.56.1 HISTORIA-BILETY] GET /history/link?stream=short|long -> 32 znaki
  // hex. Bilet jest jednorazowy i wygasa po 5 minutach. Staly token z secrets.h
  // NIGDY nie trafia do przegladarki.
  void link(AsyncWebServerRequest *request) {
    if (request == nullptr) return;
    if (request->method() != HTTP_GET) {
      odpowiedzBledu(request, 405, "Metoda niedozwolona", "Allow", "GET");
      return;
    }
    if (token_ == nullptr || *token_ == '\\0') {
      odpowiedzBledu(request, 503, "Pobieranie historii wylaczone (brak tokenu w secrets.h)", nullptr, nullptr);
      return;
    }
    if (bilety_ == nullptr) {
      odpowiedzBledu(request, 503, "Bilety pobrania nie sa podpiete", nullptr, nullptr);
      return;
    }
    const bool dluga = request->hasParam("stream") && request->getParam("stream")->value() == "long";
    char bilet[historia213::kZnakow + 1];
    if (!bilety_->wydaj(dluga ? historia213::kDluga : historia213::kKrotki, bilet)) {
      odpowiedzBledu(request, 503, "Brak zrodla losowosci dla biletu", nullptr, nullptr);
      return;
    }
    AsyncWebServerResponse *r = request->beginResponse(200, "text/plain", String(bilet));
    if (r != nullptr) r->addHeader("Cache-Control", "no-store");
    request->send(r);
  }

'''


def dodaj_pobrania(tekst):
    if "HISTORIA-BILETY" in tekst:
        raise SystemExit("HistoriaPobranie.h juz ma wstawki 213")
    tekst = tekst.replace('#include "HistoriaPin.h"\n', POBRANIA_INCLUDE, 1)
    if tekst.count(POBRANIA_CTOR) != 1:
        raise SystemExit("brak konstruktora Pobranie")
    tekst = tekst.replace(POBRANIA_CTOR, POBRANIA_SETTER, 1)
    if tekst.count(POBRANIA_AUTH_STARE) != 1:
        raise SystemExit("brak starego bloku poswiadczen")
    tekst = tekst.replace(POBRANIA_AUTH_STARE, POBRANIA_AUTH_NOWE, 1)
    if tekst.count(POBRANIA_MAGAZYN) != 1:
        raise SystemExit("brak bloku magazynu")
    tekst = tekst.replace(POBRANIA_MAGAZYN, POBRANIA_MAGAZYN + POBRANIA_BILET, 1)
    if '    if (!bramka_.zamknij()) {' not in tekst:
        raise SystemExit("brak bramki zamknij")
    if tekst.count('  bool zajeta() const { return zajeta_; }') != 1:
        raise SystemExit("brak zajeta()")
    # link() to OSOBNA metoda - musi wyladowac po zamknieciu obsluz(), nie w niej.
    tekst = tekst.replace('  bool zajeta() const { return zajeta_; }',
                          POBRANIA_LINK + '  bool zajeta() const { return zajeta_; }', 1)
    if '  Bramka &bramka_;\n  const char *token_;\n' not in tekst:
        raise SystemExit("brak pol Pobranie")
    tekst = tekst.replace('  Bramka &bramka_;\n  const char *token_;\n',
                          '  Bramka &bramka_;\n  const char *token_;\n  historia213::Bilety *bilety_ = nullptr;\n', 1)
    return tekst


# --- host/Serwer213.h: model 212 + parametry zapytania -------------------

SERWER_PARAM = '''// [v3.56.1 HISTORIA-BILETY] Parametr zapytania (?k=..., &stream=...).
// W prawdziwej bibliotece to AsyncWebParameter z WebRequest.cpp.
class AsyncWebParameter {
 public:
  AsyncWebParameter(const char *n, const char *v) : nazwa_(n), wartosc_(v) {}
  const String &name() const { return nazwa_; }
  const String &value() const { return wartosc_; }

 private:
  String nazwa_;
  String wartosc_;
};

'''

SERWER_POLA = '''  // [v3.56.1 HISTORIA-BILETY] Parametry zapytania. Biblioteka trzyma je
  // w _params (GET) i _postParams; trasa uzywa tylko GET-owych.
  std::vector<AsyncWebParameter> parametry;

  bool hasParam(const char *n) const { return getParam(n) != nullptr; }

  const AsyncWebParameter *getParam(const char *n) const {
    for (const auto &p : parametry) {
      const std::string &a = p.name().std_str();
      if (a.size() != std::strlen(n)) continue;
      bool takie = true;
      for (std::size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = n[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) {
          takie = false;
          break;
        }
      }
      if (takie) return &p;
    }
    return nullptr;
  }

  void dodajParametr(const char *n, const char *v) { parametry.emplace_back(n, v); }

'''


def dodaj_serwer(tekst):
    if "HISTORIA-BILETY" in tekst:
        raise SystemExit("Serwer213 juz ma wstawki 213")
    if tekst.count('class AsyncWebServerRequest {\n') != 1:
        raise SystemExit("brak klasy AsyncWebServerRequest")
    tekst = tekst.replace('class AsyncWebServerRequest {\n', SERWER_PARAM + 'class AsyncWebServerRequest {\n', 1)
    if tekst.count('  int method() const { return metoda; }\n') != 1:
        raise SystemExit("brak method()")
    tekst = tekst.replace('  int method() const { return metoda; }\n',
                          '  int method() const { return metoda; }\n\n' + SERWER_POLA, 1)
    return tekst


def naloz(katalog, patch):
    t = subprocess.run(["patch", "-p1", "-i", patch], cwd=katalog, capture_output=True, text=True)
    if t.returncode != 0:
        raise SystemExit("patch %s nie nalozyl sie: %s%s" % (os.path.basename(patch), t.stdout, t.stderr))


def main():
    if sha256_sciezki(ZRODLO) != SHA_ZRODLA:
        raise SystemExit("plik zrodlowy nie jest tym z etapu 210")
    print("PASS patch213 zrodlo v3.55.8 sha256 zgodny z 210")

    tmp = tempfile.mkdtemp(prefix="p213-")
    try:
        baza = os.path.join(tmp, "a")
        v213 = os.path.join(tmp, "b")
        v212 = os.path.join(tmp, "c")
        for d in (baza, v213, v212):
            os.makedirs(d)
            shutil.copyfile(ZRODLO, os.path.join(d, "main_centrala.cpp"))
            shutil.copyfile(os.path.join(FIRMWARE, "secrets.h.example"),
                            os.path.join(d, "secrets.h.example"))
        naloz(v212, PATCH211)
        naloz(v212, PATCH212)
        shutil.copyfile(os.path.join(v212, "main_centrala.cpp"), os.path.join(v213, "main_centrala.cpp"))
        shutil.copyfile(os.path.join(v212, "secrets.h.example"), os.path.join(v213, "secrets.h.example"))
        tekst = wczytaj(os.path.join(v213, "main_centrala.cpp"))
        zapisz(os.path.join(v213, "main_centrala.cpp"), dodaj_213(tekst))

        d = subprocess.run(["diff", "-u", "-r", "c", "b"], cwd=tmp, capture_output=True, text=True)
        if d.returncode != 1:
            raise SystemExit("diff 213 zwrocil %d" % d.returncode)
        zapisz(WYJSCIE_INC, d.stdout)

        d2 = subprocess.run(["diff", "-u", "-r", "a", "b"], cwd=tmp, capture_output=True, text=True)
        if d2.returncode != 1:
            raise SystemExit("diff skumulowany zwrocil %d" % d2.returncode)
        zapisz(WYJSCIE_KUM, d2.stdout)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    zapisz(WYJSCIE_POBRANIA, dodaj_pobrania(wczytaj(POBRANIA212)))
    zapisz(WYJSCIE_SERWER, dodaj_serwer(wczytaj(SERWER212)))

    for plik in (WYJSCIE_INC, WYJSCIE_KUM):
        tresc = wczytaj(plik)
        plus = sum(1 for l in tresc.split("\n") if l.startswith("+") and not l.startswith("+++"))
        minus = sum(1 for l in tresc.split("\n") if l.startswith("-") and not l.startswith("---"))
        hunki = sum(1 for l in tresc.split("\n") if l.startswith("@@"))
        print("PASS patch213 %s +%d -%d hunki=%d" %
              (os.path.basename(plik), plus, minus, hunki))
    print("PASS patch213 HistoriaPobranie.h %d linii" % len(wczytaj(WYJSCIE_POBRANIA).split("\n")))
    print("PASS patch213 host/Serwer213.h %d linii" % len(wczytaj(WYJSCIE_SERWER).split("\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
