// ustawienia.h
// ============================================================================
// CENTRALA TESTOWA NA ESP32-C3 — ustawienia pieca (rdzen przenosny)
// ============================================================================
// Lustro 1:1 funkcji ustawParametrPieca() z main_centrala.cpp:
//   - te same nazwy pol (dokladnie te klucze czyta panel LCD w POLA[]),
//   - te same zakresy i TE SAME komunikaty bledow (panel/PC pokaza identyczna
//     odpowiedz jak prawdziwa centrala),
//   - te same zaleznosci miedzy polami (tempOn/tempOff, progAlarmDym vs
//     HISTEREZA_DYMU, dymCzasOn vs dymCzasStabilizacji),
//   - te same, SKROCONE klucze NVS (<=15 znakow) — patrz [FIX NVS-15] w
//     main_centrala.cpp: Preferences::putX() cicho zawodzi dla kluczy dluzszych
//     niz 15 znakow, dlatego "mieszadloCzasOff" w NVS nazywa sie
//     "mieszCzasOff". Tu jest to zachowane swiadomie, zeby test powtarzal
//     zachowanie produktu, a nie jego wade.
//
// Wartosci domyslne = wartosci z main_centrala.cpp tam, gdzie sa zadeklarowane
// wprost (progAlarmTemp=80, Temp_ON=60, Temp_OFF=50, Czas_ON=10, Czas_OFF=30,
// Wybor=1, TrybSerwa=1, antystopWlaczony=true); reszta to sensowne wartosci
// startowe symulatora (kazda do zmiany z panelu/PC).
// ============================================================================
#pragma once

#include <stdint.h>
#include <string.h>
#include <stdio.h>

namespace ustaw {

// Stale produktu, ktore wystepuja w walidacji / logice centrali.
static const int POMPA_MIN_ROZNICA_TEMP = 2;   // main_centrala.cpp
static const int HISTEREZA_DYMU = 30;          // main_centrala.cpp
static const int TEMP_MIN_POMPY = 55;          // float w centrali, tu int (prog)
static const int HISTEREZA_POMPY = 5;          // float w centrali
static const unsigned long POMPA_OVERRIDE_MS = 60UL * 60UL * 1000UL;  // 1 h

// ---------------------------------------------------------------------------
// Wszystkie pola /api/ustawienia-get (kolejnosc jak w centrali).
// ---------------------------------------------------------------------------
struct Stan {
  int wybor = 1;              // 1=Trociniak (cykl czasowy) 2=Kopciuch (temp.) 3=Automatyczny
  int trybSerwa = 1;          // 1=Auto 2=Reczny 3=Bezpieczna
  int czasOn = 10;            // min
  int czasOff = 30;           // min
  int tempOn = 60;            // C
  int tempOff = 50;           // C
  bool antystopWlaczony = true;
  int antystopDni = 7;
  bool mieszadloWlaczony = true;
  int mieszadloCzasOn = 30;   // s
  int mieszadloCzasOff = 120; // min
  bool rozpalanie = false;    // stan przejsciowy — CELOWO nie zapisywany do NVS
  int tempZadServo = 60;      // C
  int histerServo = 3;        // C
  int skokKlapy = 5;          // %
  int skokSyberka = 5;        // %
  int odchylTemp = 5;         // C
  int mnoznik = 2;            // x
  int progAlarmTemp = 80;     // C
  int progAlarmDym = 1200;    // ADC 0..4095
  int dymCzasOn = 60;         // s
  int dymCzasOff = 120;       // s
  int dymCzasStabilizacji = 20;  // s
  int dymProgTemp = 45;       // C
  int dymTrybPracy = 1;       // 0=impulsowo 1=ciagle
};

// ---------------------------------------------------------------------------
// Klucz NVS dla pola — CELOWO skrocony do <=15 znakow (patrz naglowek pliku).
// nullptr = pole nie jest zapisywane (rozpalanie).
// ---------------------------------------------------------------------------
inline const char* kluczNvs(const char* pole) {
  if (!strcmp(pole, "wybor")) return "wybor";
  if (!strcmp(pole, "trybSerwa")) return "trybSerwa";
  if (!strcmp(pole, "czasOn")) return "czasOn";
  if (!strcmp(pole, "czasOff")) return "czasOff";
  if (!strcmp(pole, "tempOn")) return "tempOn";
  if (!strcmp(pole, "tempOff")) return "tempOff";
  if (!strcmp(pole, "antystopWlaczony")) return "antystopWlacz";  // 12
  if (!strcmp(pole, "antystopDni")) return "antystopDni";
  if (!strcmp(pole, "mieszadloWlaczony")) return "mieszWlaczony";  // 13
  if (!strcmp(pole, "mieszadloCzasOn")) return "mieszadloCzasOn";  // 15
  if (!strcmp(pole, "mieszadloCzasOff")) return "mieszCzasOff";    // 12
  if (!strcmp(pole, "rozpalanie")) return nullptr;
  if (!strcmp(pole, "tempZadServo")) return "tempZadServo";
  if (!strcmp(pole, "histerServo")) return "histerServo";
  if (!strcmp(pole, "skokKlapy")) return "skokKlapy";
  if (!strcmp(pole, "skokSyberka")) return "skokSyberka";
  if (!strcmp(pole, "odchylTemp")) return "odchylTemp";
  if (!strcmp(pole, "mnoznik")) return "mnoznik";
  if (!strcmp(pole, "progAlarmTemp")) return "progAlarmTemp";
  if (!strcmp(pole, "progAlarmDym")) return "progAlarmDym";
  if (!strcmp(pole, "dymCzasOn")) return "dymCzasOn";
  if (!strcmp(pole, "dymCzasOff")) return "dymCzasOff";
  if (!strcmp(pole, "dymCzasStabilizacji")) return "dymCzasStab";  // 11
  if (!strcmp(pole, "dymProgTemp")) return "dymProgTemp";
  if (!strcmp(pole, "dymTrybPracy")) return "dymTrybPracy";
  return nullptr;
}

// ---------------------------------------------------------------------------
// Walidacja + zapis jednego parametru. Semantyka i komunikaty 1:1 z
// ustawParametrPieca() z main_centrala.cpp.
//
// Zwraca true = przyjeto (Stan zaktualizowany, `err` nietkniete).
// Zwraca false = odrzucono, `err` zawiera komunikat do JSON-a {"ok":false}.
//
// UWAGA: ta funkcja NIE dotyka NVS — zapis robi warstwa Arduino na podstawie
// kluczNvs(pole). Dzieki temu rdzen jest przenosny i testowalny na PC.
// ---------------------------------------------------------------------------
inline bool ustaw(Stan& s, const char* pole, long w, char* err, size_t errLen) {
  auto blad = [&](const char* tekst) -> bool {
    if (err && errLen) snprintf(err, errLen, "%s", tekst);
    return false;
  };

  if (!strcmp(pole, "wybor")) {
    if (w < 1 || w > 3) return blad("wybor musi byc 1-3");
    s.wybor = (int)w;
  } else if (!strcmp(pole, "trybSerwa")) {
    if (w < 1 || w > 3) return blad("trybSerwa musi byc 1-3");
    s.trybSerwa = (int)w;
  } else if (!strcmp(pole, "czasOn")) {
    if (w < 1 || w > 180) return blad("czasOn poza zakresem 1-180");
    s.czasOn = (int)w;
  } else if (!strcmp(pole, "czasOff")) {
    if (w < 1 || w > 180) return blad("czasOff poza zakresem 1-180");
    s.czasOff = (int)w;
  } else if (!strcmp(pole, "tempOn")) {
    if (w < 0 || w > 100) return blad("tempOn poza zakresem 0-100");
    if (w < (long)s.tempOff + POMPA_MIN_ROZNICA_TEMP) {
      if (err && errLen) {
        snprintf(err, errLen,
                 "tempOn musi byc co najmniej %d C wyzsze niz tempOff (obecnie %d)",
                 POMPA_MIN_ROZNICA_TEMP, s.tempOff);
      }
      return false;
    }
    s.tempOn = (int)w;
  } else if (!strcmp(pole, "tempOff")) {
    if (w < 0 || w > 100) return blad("tempOff poza zakresem 0-100");
    if (w > (long)s.tempOn - POMPA_MIN_ROZNICA_TEMP) {
      if (err && errLen) {
        snprintf(err, errLen,
                 "tempOff musi byc co najmniej %d C nizsze niz tempOn (obecnie %d)",
                 POMPA_MIN_ROZNICA_TEMP, s.tempOn);
      }
      return false;
    }
    s.tempOff = (int)w;
  } else if (!strcmp(pole, "antystopWlaczony")) {
    if (w != 0 && w != 1) return blad("antystopWlaczony musi byc 0 lub 1");
    s.antystopWlaczony = (w != 0);
  } else if (!strcmp(pole, "antystopDni")) {
    if (w < 1 || w > 45) return blad("antystopDni poza zakresem 1-45");
    s.antystopDni = (int)w;
  } else if (!strcmp(pole, "mieszadloWlaczony")) {
    if (w != 0 && w != 1) return blad("mieszadloWlaczony musi byc 0 lub 1");
    s.mieszadloWlaczony = (w != 0);
  } else if (!strcmp(pole, "mieszadloCzasOn")) {
    if (w < 1 || w > 255) return blad("mieszadloCzasOn poza zakresem 1-255");
    s.mieszadloCzasOn = (int)w;
  } else if (!strcmp(pole, "mieszadloCzasOff")) {
    if (w < 1 || w > 180) return blad("mieszadloCzasOff poza zakresem 1-180");
    s.mieszadloCzasOff = (int)w;
  } else if (!strcmp(pole, "rozpalanie")) {
    if (w != 0 && w != 1) return blad("rozpalanie musi byc 0 lub 1");
    s.rozpalanie = (w != 0);
  } else if (!strcmp(pole, "tempZadServo")) {
    if (w < 0 || w > 100) return blad("tempZadServo poza zakresem 0-100");
    s.tempZadServo = (int)w;
  } else if (!strcmp(pole, "histerServo")) {
    if (w < 0 || w > 50) return blad("histerServo poza zakresem 0-50");
    s.histerServo = (int)w;
  } else if (!strcmp(pole, "skokKlapy")) {
    if (w < 1 || w > 100) return blad("skokKlapy poza zakresem 1-100");
    s.skokKlapy = (int)w;
  } else if (!strcmp(pole, "skokSyberka")) {
    if (w < 1 || w > 100) return blad("skokSyberka poza zakresem 1-100");
    s.skokSyberka = (int)w;
  } else if (!strcmp(pole, "odchylTemp")) {
    if (w < 1 || w > 50) return blad("odchylTemp poza zakresem 1-50");
    s.odchylTemp = (int)w;
  } else if (!strcmp(pole, "mnoznik")) {
    if (w < 1 || w > 10) return blad("mnoznik poza zakresem 1-10");
    s.mnoznik = (int)w;
  } else if (!strcmp(pole, "progAlarmTemp")) {
    if (w < 0 || w > 100) return blad("progAlarmTemp poza zakresem 0-100");
    s.progAlarmTemp = (int)w;
  } else if (!strcmp(pole, "progAlarmDym")) {
    if (w < 0 || w > 4095) return blad("progAlarmDym poza zakresem 0-4095");
    if (w <= HISTEREZA_DYMU) {
      if (err && errLen) {
        snprintf(err, errLen,
                 "progAlarmDym musi byc wiekszy niz HISTEREZA_DYMU (%d), "
                 "inaczej alarm nie zdola sam wygasnac",
                 HISTEREZA_DYMU);
      }
      return false;
    }
    s.progAlarmDym = (int)w;
  } else if (!strcmp(pole, "dymCzasOn")) {
    if (w < 20 || w > 600) return blad("dymCzasOn poza zakresem 20-600");
    if (w <= (long)s.dymCzasStabilizacji + 5) {
      if (err && errLen) {
        snprintf(err, errLen,
                 "dymCzasOn musi byc co najmniej 5s wieksze niz dymCzasStabilizacji "
                 "(obecnie %d) - inaczej czujnik nigdy nie zdazy zmierzyc",
                 s.dymCzasStabilizacji);
      }
      return false;
    }
    s.dymCzasOn = (int)w;
  } else if (!strcmp(pole, "dymCzasOff")) {
    if (w < 10 || w > 600) return blad("dymCzasOff poza zakresem 10-600");
    s.dymCzasOff = (int)w;
  } else if (!strcmp(pole, "dymCzasStabilizacji")) {
    if (w < 5 || w > 590) return blad("dymCzasStabilizacji poza zakresem 5-590");
    if (w >= (long)s.dymCzasOn - 5) {
      if (err && errLen) {
        snprintf(err, errLen,
                 "dymCzasStabilizacji musi byc co najmniej 5s mniejsze niz dymCzasOn "
                 "(obecnie %d)", s.dymCzasOn);
      }
      return false;
    }
    s.dymCzasStabilizacji = (int)w;
  } else if (!strcmp(pole, "dymProgTemp")) {
    if (w < 0 || w > 200) return blad("dymProgTemp poza zakresem 0-200");
    s.dymProgTemp = (int)w;
  } else if (!strcmp(pole, "dymTrybPracy")) {
    if (w < 0 || w > 1) return blad("dymTrybPracy musi byc 0 (impulsowo) lub 1 (ciagle)");
    s.dymTrybPracy = (int)w;
  } else {
    if (err && errLen) snprintf(err, errLen, "nieznane pole: %s", pole);
    return false;
  }
  return true;
}

// Odczyt pola jako long (do zapisu do NVS w warstwie Arduino).
inline long wartosc(const Stan& s, const char* pole) {
  if (!strcmp(pole, "wybor")) return s.wybor;
  if (!strcmp(pole, "trybSerwa")) return s.trybSerwa;
  if (!strcmp(pole, "czasOn")) return s.czasOn;
  if (!strcmp(pole, "czasOff")) return s.czasOff;
  if (!strcmp(pole, "tempOn")) return s.tempOn;
  if (!strcmp(pole, "tempOff")) return s.tempOff;
  if (!strcmp(pole, "antystopWlaczony")) return s.antystopWlaczony ? 1 : 0;
  if (!strcmp(pole, "antystopDni")) return s.antystopDni;
  if (!strcmp(pole, "mieszadloWlaczony")) return s.mieszadloWlaczony ? 1 : 0;
  if (!strcmp(pole, "mieszadloCzasOn")) return s.mieszadloCzasOn;
  if (!strcmp(pole, "mieszadloCzasOff")) return s.mieszadloCzasOff;
  if (!strcmp(pole, "rozpalanie")) return s.rozpalanie ? 1 : 0;
  if (!strcmp(pole, "tempZadServo")) return s.tempZadServo;
  if (!strcmp(pole, "histerServo")) return s.histerServo;
  if (!strcmp(pole, "skokKlapy")) return s.skokKlapy;
  if (!strcmp(pole, "skokSyberka")) return s.skokSyberka;
  if (!strcmp(pole, "odchylTemp")) return s.odchylTemp;
  if (!strcmp(pole, "mnoznik")) return s.mnoznik;
  if (!strcmp(pole, "progAlarmTemp")) return s.progAlarmTemp;
  if (!strcmp(pole, "progAlarmDym")) return s.progAlarmDym;
  if (!strcmp(pole, "dymCzasOn")) return s.dymCzasOn;
  if (!strcmp(pole, "dymCzasOff")) return s.dymCzasOff;
  if (!strcmp(pole, "dymCzasStabilizacji")) return s.dymCzasStabilizacji;
  if (!strcmp(pole, "dymProgTemp")) return s.dymProgTemp;
  if (!strcmp(pole, "dymTrybPracy")) return s.dymTrybPracy;
  return 0;
}

}  // namespace ustaw
