#pragma once
#include <cstddef>
#include <cstdint>

// [v3.56.1 HISTORIA-BILETY] Jednorazowe, krotko zyjace bilety na pobranie historii.
//
// Po co: panel WWW ma miec przycisk "Pobierz historie", ale NIE moze wyslac
// stalego tokenu z secrets.h do przegladarki. Zamiast tego panel prosi centrale
// o bilet (GET /history/link), dostaje 32 znaki hex i otwiera /history?k=<bilet>.
// Bilet jest zwiazany ze strumieniem (krotki/dluga), jest JEDNORAZOWY i wygasa
// po 5 minutach. Skopiowany z historii przegladarki link jest wiec bezuzyteczny
// po pierwszym uzyciu.
//
// Czego to NIE jest: nie jest to uwierzytelnianie panelu. Kto dosiegnie panelu,
// ten dostanie bilet - dokladnie tak, jak dzis dosiegnie /api/history bez
// zadnego tokenu. Zysk jest waski i swiadomy: staly token nie wycieka do HTML-a,
// a link, ktory komus wyslesz, sam sie spali.
//
// Bez dynamiki: cztery miejsca, brak alokacji, brak zaleznosci od Arduino.

namespace historia213 {

// Strumien jako zwykla liczba, zeby ten plik nie zalezal od zadnego innego
// naglowka (trasa ma wlasny enum Strumien; tlumaczenie jest w miejscu uzycia).
using Strumien = std::uint8_t;
static constexpr Strumien kKrotki = 0;
static constexpr Strumien kDluga = 1;

static constexpr std::size_t kZnakow = 32;                       // 16 bajtow -> 32 znaki hex
static constexpr std::size_t kMiejsc = 4;
static constexpr std::uint32_t kWaznoscMs = 5u * 60u * 1000u;    // 5 minut

class Bilety {
 public:
  using Los = void (*)(std::uint8_t *out, std::size_t n);
  using Zegar = std::uint32_t (*)();

  Bilety(Los los, Zegar zegar) : los_(los), zegar_(zegar) {}

  // Wydaje nowy bilet. Zwraca false tylko przy bledzie konfiguracji (brak
  // zrodla losowosci albo zegara) - miejsca zawsze sie znajda, najwyzej
  // najstarszy bilet zostanie uniewazniony.
  bool wydaj(Strumien s, char *wyjscie) {
    if (los_ == nullptr || zegar_ == nullptr || wyjscie == nullptr) return false;
    std::size_t miejsce = kMiejsc;
    for (std::size_t i = 0; i < kMiejsc; ++i) {
      if (!bilety_[i].zajety) {
        miejsce = i;
        break;
      }
    }
    if (miejsce == kMiejsc) {  // wszystkie zajete: wybierz ten, ktory wygasa najwczesniej
      std::uint32_t najwczesniej = bilety_[0].wygasa;
      miejsce = 0;
      for (std::size_t i = 1; i < kMiejsc; ++i) {
        if (static_cast<std::int32_t>(bilety_[i].wygasa - najwczesniej) < 0) {
          najwczesniej = bilety_[i].wygasa;
          miejsce = i;
        }
      }
    }

    std::uint8_t surowe[16];
    los_(surowe, sizeof(surowe));
    static const char hex[] = "0123456789abcdef";
    for (std::size_t i = 0; i < sizeof(surowe); ++i) {
      wyjscie[2 * i] = hex[surowe[i] >> 4];
      wyjscie[2 * i + 1] = hex[surowe[i] & 0x0F];
    }
    wyjscie[kZnakow] = '\0';
    for (std::size_t i = 0; i < sizeof(surowe); ++i) surowe[i] = 0;  // nie zostawiaj losowania na stosie

    bilety_[miejsce].zajety = true;
    bilety_[miejsce].wygasa = zegar_() + kWaznoscMs;
    bilety_[miejsce].strumien = s;
    for (std::size_t i = 0; i <= kZnakow; ++i) bilety_[miejsce].kod[i] = wyjscie[i];
    return true;
  }

  // Sprawdza i JEDNOCZESNIE uniewaznia bilet. Praca zalezy tylko od dlugosci
  // podanego kodu, nie od tego, na ktorym znaku sie rozni (stala liczba
  // porownan: kMiejsc * kZnakow).
  bool zuzyj(const char *kod, std::size_t dlugosc, Strumien s) {
    if (kod == nullptr || dlugosc != kZnakow || zegar_ == nullptr) return false;
    const std::uint32_t teraz = zegar_();
    std::size_t trafienie = kMiejsc;
    for (std::size_t i = 0; i < kMiejsc; ++i) {
      unsigned char inny = static_cast<unsigned char>(bilety_[i].zajety && bilety_[i].strumien == s ? 0 : 1);
      for (std::size_t j = 0; j < kZnakow; ++j) {
        const unsigned char a = static_cast<unsigned char>(kod[j]);
        const unsigned char b = static_cast<unsigned char>(bilety_[i].kod[j]);
        inny = static_cast<unsigned char>(inny | (a ^ b));
      }
      if (static_cast<std::int32_t>(teraz - bilety_[i].wygasa) >= 0) inny = 1;  // wygasl
      if (inny == 0 && trafienie == kMiejsc) trafienie = i;
    }
    if (trafienie == kMiejsc) return false;
    bilety_[trafienie].zajety = false;
    for (std::size_t i = 0; i <= kZnakow; ++i) bilety_[trafienie].kod[i] = '0';
    return true;
  }

  // Tylko do testow i do startu po restarcie: czysci wszystkie miejsca.
  void wyczysc() {
    for (std::size_t i = 0; i < kMiejsc; ++i) {
      bilety_[i].zajety = false;
      bilety_[i].wygasa = 0;
      for (std::size_t j = 0; j <= kZnakow; ++j) bilety_[i].kod[j] = '0';
    }
  }

  std::size_t zajete() const {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kMiejsc; ++i) {
      if (bilety_[i].zajety) ++n;
    }
    return n;
  }

 private:
  struct Bilet {
    bool zajety = false;
    char kod[kZnakow + 1] = {};
    std::uint32_t wygasa = 0;
    Strumien strumien = kKrotki;
  };

  Los los_;
  Zegar zegar_;
  Bilet bilety_[kMiejsc];
};

}  // namespace historia213
