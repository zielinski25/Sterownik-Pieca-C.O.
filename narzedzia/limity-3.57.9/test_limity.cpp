// Symulacja LIMITOW historii na flashu - firmware 3.57.8.
//
// Nie przepisujemy tu logiki zapisu "z pamieci": uruchamiamy PRAWDZIWA klase
// history193::Engine z src/CheckedHistoryIO.h (kopia bit w bit z paczki
// 3.57.8) na dysku o skonczonej pojemnosci (flash_sym.h). To, co dokladamy,
// to warstwa wyzej - te same progi i ta sama kolejnosc, co w
// historiaFlushDoFS()/historiaDlugaFlushDoFS()/historiaPrzytnijPlik():
//
//   1. append()  - dopisanie linii (Engine weryfikuje zapis i sync),
//   2. trim()    - TYLKO gdy linii > limit + margines (tak jak w firmware),
//
// a przy bledzie Engine sam sie blokuje (fault_ != None) - dokladnie ten
// mechanizm, ktory w firmware daje komunikat "zapis niepotwierdzony -
// kolejka zachowana, obie historie zatrzymane".
//
// Rozmiary linii wzietych z prawdziwych plikow z centrali (Dysk:
// hist_short (9).csv, hist_long (7).csv): 71 B i 180 B na linie.
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "flash_sym.h"

namespace {

constexpr const char* PLIK_KROTKI = "/littlefs/hist_short.csv";
constexpr const char* PLIK_DLUGI = "/littlefs/hist_long.csv";

// Progi z 3.57.8 (main_centrala.cpp):
//   HISTORIA_MAX_PROBEK            1440
//   HISTORIA_DLUGA_MAX_PROBEK      8000
//   HISTORIA_RESYNC_MARGINES       1000
//   HISTORIA_DLUGA_RESYNC_MARGINES  500
//   HISTORIA_PENDING_LIMIT_B       4096
constexpr unsigned LIMIT_KROTKI = 1440;
constexpr unsigned MARGINES_KROTKI = 1000;
constexpr unsigned LIMIT_DLUGI = 8000;
constexpr unsigned MARGINES_DLUGI = 500;
constexpr std::size_t PENDING_LIMIT_B = 4096;
constexpr std::size_t LINIA_KROTKA_B = 71;
constexpr std::size_t LINIA_DLUGA_B = 180;

int okCount = 0;
int failCount = 0;

void Check(bool warunek, const std::string& nazwa, const std::string& szczegol = "") {
  if (warunek) {
    ++okCount;
    std::printf("  OK   %s%s%s\n", nazwa.c_str(), szczegol.empty() ? "" : " - ", szczegol.c_str());
  } else {
    ++failCount;
    std::printf("  FAIL %s%s%s\n", nazwa.c_str(), szczegol.empty() ? "" : " - ", szczegol.c_str());
  }
}

std::string Linia(std::size_t bajtow) {
  if (bajtow < 2) bajtow = 2;
  std::string s(bajtow - 1, '0');
  s.push_back('\n');
  return s;
}

std::string Mb(double bajty) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.2f MB (%lld B)", bajty / (1024.0 * 1024.0),
                static_cast<long long>(bajty));
  return buf;
}

// Jedna "probka" - dokladnie ta sama kolejnosc, co w firmware.
struct Symulacja {
  limity::FakeFlash flash;
  std::unique_ptr<history193::Engine> io;
  unsigned linie = 0;

  explicit Symulacja(std::size_t pojemnoscBajtow)
      : flash(pojemnoscBajtow, 4096),
        io(std::make_unique<history193::Engine>(flash, nullptr, nullptr)) {}

  history193::Engine& Io() { return *io; }

  // "Restart centrali": nowy Engine na tym samym dysku, licznik linii
  // odtworzony z pliku (w firmware robi to historiaWczytajZFS()).
  void Restart(history193::Stream strumien) {
    io = std::make_unique<history193::Engine>(flash, nullptr, nullptr);
    linie = static_cast<unsigned>(flash.linii(strumien == history193::Stream::Short
                                                  ? "/littlefs/hist_short.csv"
                                                  : "/littlefs/hist_long.csv"));
  }

  bool Probka(history193::Stream strumien, std::size_t dlugoscLinii, unsigned limit,
              unsigned margines) {
    const std::string linia = Linia(dlugoscLinii);
    if (!io->append(strumien, linia.c_str(), linia.size(), rozmiar_)) return false;
    ++linie;
    if (linie > limit + margines) {
      std::uint64_t nowyRozmiar = 0;
      unsigned zachowane = 0;
      if (!io->trim(strumien, limit, nowyRozmiar, zachowane)) return false;
      rozmiar_ = nowyRozmiar;
      linie = zachowane;
    }
    return true;
  }

  std::uint64_t rozmiar_ = 0;
};

std::size_t RozmiarPliku(Symulacja& s, history193::Stream st) {
  return s.flash.rozmiar(st == history193::Stream::Short ? PLIK_KROTKI : PLIK_DLUGI);
}

// ── L1: dluga historia dojezdza do progu rotacji i rotuje ────────────────
void L1() {
  std::printf("\nL1 - dluga historia: 8600 probek na dysku 8 MB (powyzej progu 8500)\n");
  Symulacja s(8u * 1024u * 1024u);
  bool wszystko = true;
  for (int i = 0; i < 8600 && wszystko; ++i)
    wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);

  Check(wszystko, "wszystkie 8600 zapisow przeszly");
  Check(!s.Io().blocked(), "Engine nie zablokowany",
        s.Io().blocked() ? ("fault=" + std::to_string(static_cast<int>(s.Io().fault()))) : "");
  Check(s.linie >= LIMIT_DLUGI && s.linie <= LIMIT_DLUGI + MARGINES_DLUGI + 1,
        "po rotacji linii w [8000, 8501]", "linii=" + std::to_string(s.linie));
  Check(s.flash.zmianyNazw_ > 0, "rotacja faktycznie wykonala rename",
        "rename=" + std::to_string(s.flash.zmianyNazw_));
  std::printf("       plik %.2f MB, szczyt zajecia dysku %s\n",
              RozmiarPliku(s, history193::Stream::Long) / (1024.0 * 1024.0),
              Mb(static_cast<double>(s.flash.szczyt)).c_str());
}

// ── L2: najmniejsza pojemnosc, przy ktorej dluga historia dziala ─────────
void L2() {
  std::printf("\nL2 - najmniejsza pojemnosc dysku, przy ktorej 8600 probek dlugich przechodzi\n");
  std::size_t najmniejsza = 0;
  std::size_t szczyt = 0;
  for (std::size_t kandydat = 1024u * 1024u; kandydat <= 6u * 1024u * 1024u;
       kandydat += 16u * 1024u) {
    Symulacja s(kandydat);
    bool wszystko = true;
    for (int i = 0; i < 8600 && wszystko; ++i)
      wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
    if (wszystko) {
      najmniejsza = kandydat;
      szczyt = static_cast<std::size_t>(s.flash.szczyt);
      break;
    }
  }
  Check(najmniejsza != 0, "znalazlem najmniejsza dzialajaca pojemnosc");
  std::printf("       najmniejsza dzialajaca pojemnosc: %s\n", Mb(static_cast<double>(najmniejsza)).c_str());
  std::printf("       szczyt zajecia w tej symulacji:   %s\n", Mb(static_cast<double>(szczyt)).c_str());

  // Analitycznie: plik przed rotacja + kopia robocza po rotacji.
  const std::size_t przed = (LIMIT_DLUGI + MARGINES_DLUGI + 1) * LINIA_DLUGA_B;
  const std::size_t po = LIMIT_DLUGI * LINIA_DLUGA_B;
  std::printf("       analitycznie: %s + %s = %s\n", Mb(static_cast<double>(przed)).c_str(),
              Mb(static_cast<double>(po)).c_str(),
              Mb(static_cast<double>(przed + po)).c_str());
  const std::size_t dolna = przed + po;
  const std::size_t gorna = dolna + 4096 * 2 + 16 * 1024;  // bloki + krok siatki
  Check(najmniejsza >= dolna && najmniejsza <= gorna,
        "wynik symulacji zgadza sie z analiza (2 x plik + bloki)",
        "przedzial [" + std::to_string(dolna) + ", " + std::to_string(gorna) + "]");
}

// ── L3: brak miejsca W TRAKCIE rotacji - co sie dzieje z historia ────────
void L3() {
  std::printf("\nL3 - brak miejsca w trakcie rotacji (dysk 2,0 MB, szczyt potrzebny ~3 MB)\n");
  Symulacja s(2u * 1024u * 1024u);
  int i = 0;
  for (; i < 8600; ++i) {
    if (!s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI)) break;
  }
  Check(s.Io().blocked(), "Engine zablokowany po nieudanej rotacji",
        "po " + std::to_string(i) + " probkach, fault=" +
            std::to_string(static_cast<int>(s.Io().fault())));
  Check(i > 8000, "do blokady doszlo dopiero przy rotacji, nie przy zapisie",
        "probek zapisanych=" + std::to_string(i));
  Check(s.flash.istnieje(PLIK_DLUGI), "plik historii nadal istnieje (nie skasowany)");
  Check(RozmiarPliku(s, history193::Stream::Long) > 0, "plik nie zostal wyzerowany",
        "rozmiar=" + std::to_string(RozmiarPliku(s, history193::Stream::Long)) + " B");
  Check(s.flash.istnieje("/littlefs/hist_long.csv.rebuild"),
        "po nieudanej rotacji ZOSTAJE plik roboczy .rebuild (Engine go nie kasuje)");

  // Kluczowe: Engine jest JEDEN dla obu historii.
  const bool krotka = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI,
                               MARGINES_KROTKI);
  Check(!krotka, "po bledzie dlugiej historii NIE zapisuje sie tez krotka (wspolny Engine)");

  // Czy blokada mija sama? W firmware nie ma sciezki odblokowania.
  int dalsze = 0;
  for (int k = 0; k < 100; ++k)
    if (s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI)) ++dalsze;
  Check(dalsze == 0, "blokada jest trwala do restartu - 100 kolejnych prob bez skutku");
}

// ── L4: dysk zapchany przez cos innego (logi) ───────────────────────────
void L4() {
  std::printf("\nL4 - dysk 4 MB zapchany logami tak, ze zostaje mniej niz jeden blok\n");
  Symulacja s(4u * 1024u * 1024u);
  const std::size_t doWypelnienia = s.flash.pojemnosc() - 2048;  // pol bloku
  const bool udalo = s.flash.wypelnij("/littlefs/log_b.txt", doWypelnienia);
  Check(udalo, "logi zajely miejsce");
  std::printf("       zajete przed probka: %s z %s\n",
              Mb(static_cast<double>(s.flash.uzyte())).c_str(),
              Mb(static_cast<double>(s.flash.pojemnosc())).c_str());
  const bool zapis = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
  Check(!zapis, "zapis odrzucony przy braku miejsca (ENOSPC)");
  Check(s.Io().blocked(), "pojedynczy ENOSPC blokuje CALY zapis historii (obie historie)");
  Check(s.flash.odrzuconeZapisy > 0, "model odrzucil zapis, nie dopisal czesci",
        "odrzuconych=" + std::to_string(s.flash.odrzuconeZapisy));
}

// ── L4b: urwana ostatnia linia w pliku - czy historia wstaje? ───────────
void L4b() {
  std::printf("\nL4b - plik historii konczy sie w polowie linii (uszkodzony ogon)\n");
  Symulacja s(8u * 1024u * 1024u);
  // Najpierw normalny zapis, potem obciecie ogona tak, jak po uszkodzonym
  // zapisie: ostatnia linia bez znaku konca.
  for (int i = 0; i < 10; ++i)
    s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI, MARGINES_KROTKI);
  s.flash.obetnijOgon("/littlefs/hist_short.csv", 20);
  Check(s.flash.rozmiar("/littlefs/hist_short.csv") > 0, "plik istnieje i ma urwany ogon",
        std::to_string(s.flash.rozmiar("/littlefs/hist_short.csv")) + " B");
  const bool zapis = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI,
                              MARGINES_KROTKI);
  Check(!zapis, "kolejny zapis odrzucony");
  Check(s.Io().fault() == history193::Fault::Boundary, "powod: kontrola granicy linii (Boundary)",
        "fault=" + std::to_string(static_cast<int>(s.Io().fault())));

  s.Restart(history193::Stream::Short);
  const bool poRestarcie = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI,
                                    MARGINES_KROTKI);
  Check(!poRestarcie, "restart NIE naprawia ogona - zapis dalej odrzucany");
  Check(s.flash.istnieje("/littlefs/hist_short.csv"), "dane w pliku nie zostaly skasowane");
}

// ── L3b: stary plik .rebuild (np. po zaniku pradu) blokuje rotacje ──────
void L3b() {
  std::printf("\nL3b - na dysku lezy stary hist_long.csv.rebuild (zanik pradu w trakcie rotacji)\n");
  Symulacja s(8u * 1024u * 1024u);
  const bool udalo = s.flash.wypelnij("/littlefs/hist_long.csv.rebuild", 1024);
  Check(udalo, "stary plik roboczy istnieje na dysku");
  bool wszystko = true;
  int i = 0;
  for (; i < 8600 && wszystko; ++i)
    wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
  Check(!wszystko && s.Io().blocked(), "pierwsza rotacja konczy sie bledem",
        "po " + std::to_string(i) + " probkach, fault=" +
            std::to_string(static_cast<int>(s.Io().fault())));
  Check(s.Io().fault() == history193::Fault::Open, "powod: O_EXCL na istniejacym .rebuild");

  s.Restart(history193::Stream::Long);
  const bool poRestarcie = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI,
                                    MARGINES_DLUGI);
  Check(!poRestarcie, "restart NIE pomaga - rotacja dalej nie przejdzie",
        "fault=" + std::to_string(static_cast<int>(s.Io().fault())));
}

// ── L5: dokladna granica rotacji krotkiej historii (2440 / 2441) ─────────
void L5() {
  std::printf("\nL5 - granica krotkiej historii: 1440 + 1000 = 2440 linii\n");
  {
    Symulacja s(8u * 1024u * 1024u);
    bool wszystko = true;
    for (int i = 0; i < static_cast<int>(LIMIT_KROTKI + MARGINES_KROTKI) && wszystko; ++i)
      wszystko = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI, MARGINES_KROTKI);
    Check(wszystko && s.flash.zmianyNazw_ == 0, "przy 2440 liniach rotacji jeszcze NIE ma",
          "linii=" + std::to_string(s.linie) + " rename=" + std::to_string(s.flash.zmianyNazw_));
  }
  {
    Symulacja s(8u * 1024u * 1024u);
    bool wszystko = true;
    for (int i = 0; i < static_cast<int>(LIMIT_KROTKI + MARGINES_KROTKI) + 1 && wszystko; ++i)
      wszystko = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI, MARGINES_KROTKI);
    Check(wszystko && s.flash.zmianyNazw_ == 1, "przy 2441 linii rotacja robi sie raz",
          "linii=" + std::to_string(s.linie) + " rename=" + std::to_string(s.flash.zmianyNazw_));
    Check(s.linie == LIMIT_KROTKI, "po rotacji zostaje dokladnie 1440 linii",
          "linii=" + std::to_string(s.linie));
  }
}

// ── L6: to samo dla dlugiej (8500 / 8501) ───────────────────────────────
void L6() {
  std::printf("\nL6 - granica dlugiej historii: 8000 + 500 = 8500 linii\n");
  {
    Symulacja s(8u * 1024u * 1024u);
    bool wszystko = true;
    for (int i = 0; i < static_cast<int>(LIMIT_DLUGI + MARGINES_DLUGI) && wszystko; ++i)
      wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
    Check(wszystko && s.flash.zmianyNazw_ == 0, "przy 8500 liniach rotacji jeszcze NIE ma",
          "linii=" + std::to_string(s.linie));
  }
  {
    Symulacja s(8u * 1024u * 1024u);
    bool wszystko = true;
    for (int i = 0; i < static_cast<int>(LIMIT_DLUGI + MARGINES_DLUGI) + 1 && wszystko; ++i)
      wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
    Check(wszystko && s.flash.zmianyNazw_ == 1, "przy 8501 linii rotacja robi sie raz",
          "linii=" + std::to_string(s.linie));
    Check(s.linie == LIMIT_DLUGI, "po rotacji zostaje dokladnie 8000 linii",
          "linii=" + std::to_string(s.linie));
  }
}

// ── L7: kolejka RAM 4096 B - ile probek sie miesci i co sie dzieje dalej ─
void L7() {
  std::printf("\nL7 - kolejka RAM (HISTORIA_PENDING_LIMIT_B = 4096 B): ile probek\n");
  for (std::size_t linia : {LINIA_KROTKA_B, LINIA_DLUGA_B}) {
    std::size_t zajete = 0;
    int przyjete = 0;
    while (true) {
      // historiaDodajPending(): wchodzi tylko to, co sie miesci w limicie.
      if (zajete + linia > PENDING_LIMIT_B) break;
      zajete += linia;
      ++przyjete;
    }
    std::printf("       linia %zu B: miesci sie %d probek (%.1f min przy probce co 60 s)\n", linia,
                przyjete, przyjete * 60.0 / 60.0);
    Check(przyjete > 0, "co najmniej jedna probka wchodzi do kolejki",
          std::to_string(przyjete) + " przy linii " + std::to_string(linia) + " B");
    Check(zajete <= PENDING_LIMIT_B, "limit 4096 B nie zostal przekroczony");
  }
  // Typowy przypadek: flush co 5 probek.
  const std::size_t piecProbek = 5 * LINIA_KROTKA_B;
  Check(piecProbek <= PENDING_LIMIT_B, "typowy wsad (5 probek krotkich) miesci sie z zapasem",
        std::to_string(piecProbek) + " B z 4096 B");
}

// ── L8: obie historie naraz + logi - ile miejsca trzeba realnie ─────────
void L8() {
  std::printf("\nL8: ile miejsca musza zmiescic OBIE historie naraz (dysk 4 MB)\n");
  Symulacja s(4u * 1024u * 1024u);
  // Najpierw krotka do progu rotacji, potem dluga do progu rotacji.
  bool wszystko = true;
  for (int i = 0; i < static_cast<int>(LIMIT_KROTKI + MARGINES_KROTKI) + 1 && wszystko; ++i)
    wszystko = s.Probka(history193::Stream::Short, LINIA_KROTKA_B, LIMIT_KROTKI, MARGINES_KROTKI);
  for (int i = 0; i < static_cast<int>(LIMIT_DLUGI + MARGINES_DLUGI) + 1 && wszystko; ++i)
    wszystko = s.Probka(history193::Stream::Long, LINIA_DLUGA_B, LIMIT_DLUGI, MARGINES_DLUGI);
  Check(wszystko, "obie historie doszly do progu rotacji bez bledu");
  std::printf("       szczyt zajecia (obie + kopia robocza): %s\n",
              Mb(static_cast<double>(s.flash.szczyt)).c_str());
  std::printf("       po rotacjach zajete: %s\n", Mb(static_cast<double>(s.flash.uzyte())).c_str());
}

}  // namespace

int main() {
  std::printf("Symulacja limitow historii - firmware 3.57.8 (prawdziwy history193::Engine)\n");
  std::printf("Linie: krotka %zu B, dluga %zu B (srednie z prawdziwych plikow centrali)\n",
              LINIA_KROTKA_B, LINIA_DLUGA_B);
  L1();
  L2();
  L3();
  L3b();
  L4();
  L4b();
  L5();
  L6();
  L7();
  L8();
  std::printf("\nRAZEM: %d OK / %d FAIL\n", okCount, failCount);
  return failCount == 0 ? 0 : 1;
}
