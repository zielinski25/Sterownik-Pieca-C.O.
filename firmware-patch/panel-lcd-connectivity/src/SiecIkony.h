#pragma once

namespace SiecIkony {
constexpr int NIEZNANE = -128;
constexpr int ROZLACZONE = -129;
constexpr int INTERNET_NIEZNANY = -1;
enum class Stan { Nieznany, Brak, Dostepny };
struct Dane { int internet; };
struct Widok { Stan wifi; int kreski; Stan internet; };

// Wylacznie nowe pole: starsze firmware centrali nie sprawdza WAN i jego
// "internet" oznacza Firebase, wiec nie wolno go uzywac jako zamiennika.
template <typename Document>
Dane odczytaj(Document& doc) {
  Dane wynik{INTERNET_NIEZNANY};
  if (doc["internet_wan"].template is<bool>())
    wynik.internet = doc["internet_wan"].template as<bool>() ? 1 : 0;
  return wynik;
}

// rssi pochodzi z WiFi.RSSI() LCD wzgledem AP centrali, nie z /api/status.
inline Widok oblicz(bool kontakt, int rssi, int internet) {
  Widok wynik{Stan::Nieznany, 0, Stan::Nieznany};
  if (rssi == ROZLACZONE) wynik.wifi = Stan::Brak;
  else if (rssi >= -127 && rssi <= 10 && rssi != 0) {
    wynik.wifi = Stan::Dostepny;
    const int progi[] = {-95, -89, -83, -77, -71, -65, -55};
    for (int i = 0; i < 7; ++i)
      if (rssi >= progi[i]) wynik.kreski = i + 1;
  }
  if (kontakt && rssi != ROZLACZONE) {
    if (internet == 0) wynik.internet = Stan::Brak;
    else if (internet == 1) wynik.internet = Stan::Dostepny;
  }
  return wynik;
}
} // namespace SiecIkony
