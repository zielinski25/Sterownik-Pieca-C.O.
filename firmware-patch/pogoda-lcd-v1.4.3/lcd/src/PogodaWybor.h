#pragma once
namespace PogodaIkony {
enum Wariant { Nieznana, Slonce, Ksiezyc, SlonceChmura, KsiezycChmura, Chmura, Mgla, Deszcz, Snieg, Burza };
// Klasy jak kafelek WWW; kod i pora musza byc znane.
inline Wariant wybierz(int kod, int dzien) {
  if (kod < 0 || kod > 99 || (dzien != 0 && dzien != 1)) return Nieznana;
  if (kod == 0) return dzien ? Slonce : Ksiezyc;
  if (kod == 1 || kod == 2) return dzien ? SlonceChmura : KsiezycChmura;
  if (kod == 3) return Chmura;
  if (kod == 45 || kod == 48) return Mgla;
  if ((kod >= 51 && kod <= 67) || (kod >= 80 && kod <= 82)) return Deszcz;
  if ((kod >= 71 && kod <= 77) || (kod >= 85 && kod <= 86)) return Snieg;
  if (kod >= 95) return Burza;
  return dzien ? SlonceChmura : KsiezycChmura;
}
}
