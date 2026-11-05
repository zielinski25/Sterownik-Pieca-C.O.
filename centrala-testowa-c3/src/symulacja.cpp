// symulacja.cpp
// ============================================================================
// CENTRALA TESTOWA NA ESP32-C3 — implementacja silnika symulacji
// ============================================================================
// Rdzen przenosny (brak Arduino) — kompiluje sie identycznie na PC
// (testy_host/) jak i na ESP32-C3 (przez main.cpp).
// ============================================================================
#include "symulacja.h"

namespace sym {

Stan st;
ProbkaKrotka histKrotka[HIST_KROTKA_MAX];
ProbkaDluga histDluga[HIST_DLUGA_MAX];
int histKrotkaLiczba = 0;
int histKrotkaGlowa = 0;
int histDlugaLiczba = 0;
int histDlugaGlowa = 0;
Akumulator akum;

// ===========================================================================
// POMOCNICZE
// ===========================================================================
static inline float dazenie(float obecna, float cel, float tauS, float dtS = 1.0f) {
  if (tauS <= 0.0f) return cel;
  float alfa = dtS / tauS;
  if (alfa > 1.0f) alfa = 1.0f;
  return obecna + (cel - obecna) * alfa;
}

// Deterministyczny "szum" (bez rand() — powtarzalny miedzy uruchomieniami,
// zeby test na PC i na plytce dal te same liczby).
static inline float szum(float t, float amplituda, float czestotliwosc) {
  float s = sinf(t * czestotliwosc) * 0.6f + sinf(t * czestotliwosc * 2.7f + 1.3f) * 0.4f;
  return s * amplituda;
}

// ===========================================================================
// RESET
// ===========================================================================
void reset() {
  st = Stan();
  histKrotkaLiczba = 0;
  histKrotkaGlowa = 0;
  histDlugaLiczba = 0;
  histDlugaGlowa = 0;
  akum = Akumulator();
  st.pompaZmianaMs = 0;
  st.mieszadloFazaMs = 0;
  st.pompaCyklStart = false;
  st.zatrzaskPompy = false;
  st.blokadaNiskiejTemp = false;
  st.ostatniaProbkaSek = 0;
  st.ostatniaGodzina = -1;
}

// ===========================================================================
// POMPA — odpowiednik pompaUstaw() z main_centrala.cpp
// ===========================================================================
// Ta sama siatka bezpieczenstwa: pompa raz przelaczona zostaje w danym stanie
// minimum 60 s (POMPA_MIN_CZAS_STANU_MS), chyba ze zadanie jest wymuszone
// (reczny rozkaz usera z panelu). Dzieki temu panel zobaczy "pompa_ms_od_zmiany"
// zmieniajace sie tak samo jak na produkcie, a nie skaczace co sekunde.
static const unsigned long POMPA_MIN_CZAS_STANU_MS = 60000UL;

static void pompaUstaw(bool wlaczona, bool wymus, unsigned long terazMs) {
  if (!wymus && wlaczona != st.pompaOn && st.pompaZmianaMs != 0 &&
      (terazMs - st.pompaZmianaMs) < POMPA_MIN_CZAS_STANU_MS) {
    return;  // za wczesnie na kolejna zmiane — blokada anty-short-cycling
  }
  if (wlaczona != st.pompaOn || st.pompaZmianaMs == 0) {
    st.pompaZmianaMs = terazMs;
  }
  st.pompaOn = wlaczona;
}

// ===========================================================================
// SERWO — odpowiednik servo()/servo_manual() z main_centrala.cpp
// ===========================================================================
static inline int procentNaKat(int procent, int maxDeg) {
  if (procent < 0) procent = 0;
  if (procent > 100) procent = 100;
  return (procent * maxDeg) / 100;
}

static void serwoRuch(unsigned long terazMs, const ustaw::Stan& u) {
  const bool alarmAktywny = st.alarmDym || st.alarmOgrz || st.alarmPanel;

  // [BEZPIECZENSTWO PRZEDE WSZYSTKIM] Zaden tryb reczny/auto nie moze
  // nadpisac pozycji bezpiecznej wymuszonej przez alarm — ta sama zasada co
  // w servo_manual() centrali (wczesny return przy alarmach).
  if (alarmAktywny) {
    if (st.klapaDeg > 0) st.klapaDeg -= 3;
    if (st.klapaDeg < 0) st.klapaDeg = 0;
    if (st.syberkaDeg < SYBERKA_MAX_DEG) st.syberkaDeg += 2;
    if (st.syberkaDeg > SYBERKA_MAX_DEG) st.syberkaDeg = SYBERKA_MAX_DEG;
    return;
  }

  // --- TRYB 3: BEZPIECZNA --- klapa 0, syberek 90 (pelne otwarcie komina)
  if (u.trybSerwa == 3) {
    if (st.klapaDeg > 0) st.klapaDeg = wsp::ogranicz(st.klapaDeg - 3, 0, KLAPA_MAX_DEG);
    if (st.syberkaDeg < SYBERKA_MAX_DEG)
      st.syberkaDeg = wsp::ogranicz(st.syberkaDeg + 2, 0, SYBERKA_MAX_DEG);
    return;
  }

  // --- TRYB 2: RECZNY --- jedzie do pozycji zleconej z panelu (w procentach,
  // przeliczonych na stopnie przy przyjeciu zadania), z predkoscia ~2 st./s
  // zamiast realnych ~1,8 s na 90 st. (przyspieszenie testowe).
  if (u.trybSerwa == 2) {
    if (st.celKlapaDeg >= 0) {
      if (st.klapaDeg < st.celKlapaDeg) st.klapaDeg = wsp::ogranicz(st.klapaDeg + 4, 0, st.celKlapaDeg);
      else if (st.klapaDeg > st.celKlapaDeg) st.klapaDeg = wsp::ogranicz(st.klapaDeg - 4, st.celKlapaDeg, KLAPA_MAX_DEG);
    }
    if (st.celSyberkaDeg >= 0) {
      if (st.syberkaDeg < st.celSyberkaDeg) st.syberkaDeg = wsp::ogranicz(st.syberkaDeg + 4, 0, st.celSyberkaDeg);
      else if (st.syberkaDeg > st.celSyberkaDeg) st.syberkaDeg = wsp::ogranicz(st.syberkaDeg - 4, st.celSyberkaDeg, SYBERKA_MAX_DEG);
    }
    return;
  }

  // --- TRYB 1: AUTO --- histereza wokol tempZadServo, krok = skokKlapy /
  // skokSyberka (procent zakresu), mnozony przez `mnoznik` gdy odchylenie
  // przekracza `odchylTemp`. Kolejnosc ruchu (przy zamykaniu: najpierw klapa,
  // potem syberek; przy otwieraniu odwrotnie) — 1:1 z servo() centrali,
  // wraz z interlockiem "druga os rusza sie tylko od skrajnej pozycji pierwszej".
  const float progGorny = (float)u.tempZadServo + (float)u.histerServo;
  const float progDolny = (float)u.tempZadServo - (float)u.histerServo;
  const bool zamykac = (st.tOgrzSr > progGorny);
  const bool otwierac = (st.tOgrzSr < progDolny);
  if (!zamykac && !otwierac) return;
  if (st.serwoAutoKrokMs != 0 && (terazMs - st.serwoAutoKrokMs) < SERWO_AUTO_MIN_MS) return;
  st.serwoAutoKrokMs = terazMs;

  int krokKlapa = procentNaKat(u.skokKlapy, KLAPA_MAX_DEG);
  int krokSyberka = procentNaKat(u.skokSyberka, SYBERKA_MAX_DEG);
  const float odchylenie = fabsf(st.tOgrzSr - (float)u.tempZadServo);
  if (odchylenie > (float)u.odchylTemp) {
    krokKlapa = wsp::ogranicz(krokKlapa * u.mnoznik, 0, KLAPA_MAX_DEG);
    krokSyberka = wsp::ogranicz(krokSyberka * u.mnoznik, 0, SYBERKA_MAX_DEG);
  }
  if (krokKlapa < 1) krokKlapa = 1;
  if (krokSyberka < 1) krokSyberka = 1;

  if (zamykac) {
    if (st.klapaDeg > 0) {
      st.klapaDeg = wsp::ogranicz(st.klapaDeg - krokKlapa, 0, KLAPA_MAX_DEG);
    } else if (st.klapaDeg <= 0 && st.syberkaDeg > 0) {
      st.syberkaDeg = wsp::ogranicz(st.syberkaDeg - krokSyberka, 0, SYBERKA_MAX_DEG);
    }
  } else if (otwierac) {
    if (st.syberkaDeg < SYBERKA_MAX_DEG) {
      st.syberkaDeg = wsp::ogranicz(st.syberkaDeg + krokSyberka, 0, SYBERKA_MAX_DEG);
    } else if (st.syberkaDeg >= SYBERKA_MAX_DEG && st.klapaDeg < KLAPA_MAX_DEG) {
      st.klapaDeg = wsp::ogranicz(st.klapaDeg + krokKlapa, 0, KLAPA_MAX_DEG);
    }
  }
}

// ===========================================================================
// HISTORIA
// ===========================================================================
static void akumulujProbke() {
  if (akum.n == 0) {
    for (int i = 0; i < POL_DLUGICH; i++) {
      akum.min_[i] = 1e9f;
      akum.max_[i] = -1e9f;
      akum.sum[i] = 0;
    }
  }
  const float wart[POL_DLUGICH] = {
      st.tZewn, st.tBojler, st.tOgrz, st.tOgrzSr, st.tPowrot,
      st.tPanel, st.tPokoj, st.cisnienie, st.wilgotnosc,
      (float)st.klapaDeg, (float)st.syberkaDeg};
  for (int i = 0; i < POL_DLUGICH; i++) {
    akum.sum[i] += wart[i];
    if (wart[i] < akum.min_[i]) akum.min_[i] = wart[i];
    if (wart[i] > akum.max_[i]) akum.max_[i] = wart[i];
  }
  akum.n++;
}

static void zamknijGodzine() {
  if (akum.n == 0) return;
  ProbkaDluga& d = histDluga[histDlugaGlowa];
  snprintf(d.etykieta, sizeof(d.etykieta), "%s", akum.etykieta);
  for (int i = 0; i < POL_DLUGICH; i++) {
    d.sr_x10[i] = (int16_t)wsp::roundInt(akum.sum[i] / (float)akum.n * 10.0f);
    d.min_x10[i] = (int16_t)wsp::roundInt(akum.min_[i] * 10.0f);
    d.max_x10[i] = (int16_t)wsp::roundInt(akum.max_[i] * 10.0f);
  }
  d.czas_unix = akum.czas;
  histDlugaGlowa = (histDlugaGlowa + 1) % HIST_DLUGA_MAX;
  if (histDlugaLiczba < HIST_DLUGA_MAX) histDlugaLiczba++;
  akum = Akumulator();
}

static void zapiszProbke(const wsp::Czas& czas) {
  ProbkaKrotka& p = histKrotka[histKrotkaGlowa];
  if (czas.ok) {
    snprintf(p.etykieta, sizeof(p.etykieta), "%02d:%02d", czas.godzina, czas.minuta);
  } else {
    // Centrala zapisuje "--:--" dla probek bez wiarygodnego czasu (patrz
    // [DZIURY-CZAS] w main_centrala.cpp) — panel radzi sobie z tym etykietą.
    snprintf(p.etykieta, sizeof(p.etykieta), "--:--");
  }
  p.t_zewn_x10 = (int16_t)wsp::roundInt(st.tZewn * 10.0f);
  p.t_bojler_x10 = (int16_t)wsp::roundInt(st.tBojler * 10.0f);
  p.t_ogrz_x10 = (int16_t)wsp::roundInt(st.tOgrz * 10.0f);
  p.t_ogrz_sr_x10 = (int16_t)wsp::roundInt(st.tOgrzSr * 10.0f);
  p.t_powrot_x10 = (int16_t)wsp::roundInt(st.tPowrot * 10.0f);
  p.t_panel_x10 = (int16_t)wsp::roundInt(st.tPanel * 10.0f);
  p.t_pokoj_x10 = (int16_t)wsp::roundInt(st.tPokoj * 10.0f);
  p.pompa = st.pompaOn ? 1 : 0;
  p.cisnienie_x10 = (int16_t)wsp::roundInt(st.cisnienie * 10.0f);
  p.wilgotnosc_x10 = (int16_t)wsp::roundInt(st.wilgotnosc * 10.0f);
  p.klapa_deg = (int16_t)st.klapaDeg;
  p.syberka_deg = (int16_t)st.syberkaDeg;
  p.czas_unix = czas.ok ? czas.unix : 0;
  histKrotkaGlowa = (histKrotkaGlowa + 1) % HIST_KROTKA_MAX;
  if (histKrotkaLiczba < HIST_KROTKA_MAX) histKrotkaLiczba++;

  // Akumulacja do bufora dlugiego + domykanie godziny przy zmianie godziny.
  if (akum.n == 0) {
    if (czas.ok) {
      snprintf(akum.etykieta, sizeof(akum.etykieta), "%02d.%02d %02d:00",
               czas.dzien, czas.miesiac, czas.godzina);
    } else {
      snprintf(akum.etykieta, sizeof(akum.etykieta), "--.-- --:--");
    }
    akum.czas = czas.ok ? czas.unix : 0;
  }
  akumulujProbke();
  if (czas.ok && st.ostatniaGodzina >= 0 && czas.godzina != st.ostatniaGodzina) {
    zamknijGodzine();
  }
  if (czas.ok) st.ostatniaGodzina = czas.godzina;
}

void wyczyscHistorie() {
  histKrotkaLiczba = 0;
  histKrotkaGlowa = 0;
  histDlugaLiczba = 0;
  histDlugaGlowa = 0;
  akum = Akumulator();
  st.ostatniaProbkaSek = 0;
}

// ===========================================================================
// TICK — glowny krok symulacji
// ===========================================================================
void tick(unsigned long terazMs, const wsp::Czas& czas, ustaw::Stan& u) {
  const float tSek = (float)(terazMs / 1000UL);
  const int godzina = czas.ok ? czas.godzina : 12;

  // --- 1. Wygasanie override'ow (odpowiednik obslugaWygasanSymulacjiElementow) ---
  if (st.pompaOverrideDo != 0 && terazMs >= st.pompaOverrideDo) {
    st.pompaOverrideDo = 0;  // wracamy do automatyki (jak w centrali)
  }
  if (u.trybSerwa == 2 && st.serwoOverrideDo != 0 && terazMs >= st.serwoOverrideDo) {
    st.serwoOverrideDo = 0;
    st.celKlapaDeg = -1;
    st.celSyberkaDeg = -1;
    u.trybSerwa = 1;  // powrot do Auto — dokladnie jak w centrali
  }
  if (st.mieszadloOverrideDo != 0 && terazMs >= st.mieszadloOverrideDo) {
    st.mieszadloOverrideDo = 0;
  }

  // --- 2. Otoczenie (zalezy od godziny; bez RTC godzina=12 - bezpieczny default) ---
  const float doba = (float)godzina + (czas.ok ? (float)czas.minuta / 60.0f : 0.0f);
  st.tZewn = 8.0f + 4.0f * sinf((doba - 9.0f) / 24.0f * 2.0f * (float)M_PI) +
             szum(tSek, 0.35f, 0.0009f);

  // Slonce: 0 w nocy, maksimum ok. 12:00. Panel grzewczy podgrzewa sie tylko
  // w dzien — dzieki temu kafelek PANEL i przelaczanie sezonu Lato/Zima
  // zachowuja sie jak na produkcie.
  float wzniesienie = 0.0f;
  if (doba > 6.0f && doba < 18.0f) {
    wzniesienie = sinf((doba - 6.0f) / 12.0f * (float)M_PI);
  }
  st.tPanel = (wzniesienie > 0.0f) ? (2.0f + 72.0f * wzniesienie + szum(tSek, 1.5f, 0.004f))
                                   : (-1.5f + st.tZewn * 0.5f);

  st.wilgotnosc = 46.0f + 9.0f * (1.0f - wzniesienie) + szum(tSek, 1.0f, 0.0016f);
  st.cisnienie = 1012.5f + 4.0f * sinf(tSek / 5400.0f) + szum(tSek, 0.4f, 0.0007f);

  // --- 3. Serwo (przed piecem — klapa napędza spalanie) ---
  serwoRuch(terazMs, u);

  // --- 4. Piec CO: temperatura zalezna od ciagu (klapa + syberek) ---
  float celOgrz = 27.0f + 0.285f * (float)st.klapaDeg + 0.11f * (float)st.syberkaDeg;
  celOgrz = wsp::ogranicz(celOgrz, 15.0f, 95.0f);
  if (celOgrz > st.tOgrz) {
    st.tOgrz = dazenie(st.tOgrz, celOgrz, TAU_GRZANIE_S);
  } else {
    st.tOgrz = dazenie(st.tOgrz, celOgrz, TAU_STYGNIECIE_S);
  }
  st.tOgrz += szum(tSek, 0.06f, 0.02f);
  st.tOgrzSr = st.tOgrzSr + (st.tOgrz - st.tOgrzSr) * SREDNIA_ALFA;

  // --- 5. Pompa: tryb pracy 1/2/3 + reczny override z panelu ---
  const bool pompaOverride = (st.pompaOverrideDo != 0);
  if (pompaOverride) {
    pompaUstaw(st.pompaOverrideStan, true, terazMs);
  } else if (u.wybor == 1 && u.czasOn > 0 && u.czasOff > 0) {
    // TRYB 1 (Trociniak/cykl czasowy) — 1:1 z zadanieObslugaPompy() centrali,
    // wraz z blokada "piec za zimny" i histereza jej odblokowania.
    if (st.tOgrz < (float)ustaw::TEMP_MIN_POMPY) {
      if (!st.blokadaNiskiejTemp) {
        pompaUstaw(false, false, terazMs);
        st.pompaCyklStart = false;
        st.blokadaNiskiejTemp = true;
      }
    } else if (st.blokadaNiskiejTemp) {
      if (st.tOgrz > (float)ustaw::TEMP_MIN_POMPY + (float)ustaw::HISTEREZA_POMPY) {
        st.blokadaNiskiejTemp = false;
      }
    } else {
      if (!st.pompaCyklStart) {
        st.pompaCyklStartMs = terazMs;
        st.pompaCyklStart = true;
      }
      const unsigned long wlaczMs = (unsigned long)u.czasOn * 60000UL;
      const unsigned long wylaczMs = (unsigned long)u.czasOff * 60000UL;
      const unsigned long cykl = wlaczMs + wylaczMs;
      const unsigned long faza = (terazMs - st.pompaCyklStartMs) % cykl;
      pompaUstaw(faza < wlaczMs, false, terazMs);
    }
  } else if (u.wybor == 2 && u.tempOn > 0 && u.tempOff > 0) {
    // TRYB 2 (Kopciuch/histereza temperatury) — zatrzask jak zatrzaskWybor2.
    if (st.tOgrz >= (float)u.tempOn) {
      st.zatrzaskPompy = false;
      pompaUstaw(true, false, terazMs);
    }
    if (st.tOgrz <= (float)u.tempOff && !st.zatrzaskPompy) {
      pompaUstaw(false, false, terazMs);
      st.zatrzaskPompy = true;
    }
  } else if (u.wybor == 3) {
    // TRYB 3 (Automatyczny) — uproszczona stabilizacja powrotu: grzeje, az
    // roznica zasilanie/powrot sie ustabilizuje, potem przerwa Czas_OFF.
    if (st.tOgrz > (float)ustaw::TEMP_MIN_POMPY) {
      const float roznica = fabsf(st.tOgrz - st.tPowrot);
      if (roznica < 1.0f && st.pompaOn && (terazMs - st.pompaZmianaMs) > 20000UL) {
        pompaUstaw(false, false, terazMs);
      } else if (!st.pompaOn && (terazMs - st.pompaZmianaMs) > (unsigned long)u.czasOff * 60000UL) {
        pompaUstaw(true, false, terazMs);
      }
    } else {
      pompaUstaw(false, false, terazMs);
    }
  } else {
    pompaUstaw(false, false, terazMs);
  }

  // --- 6. Bojler, powrot, pokoj, trociny ---
  const float celBojler = st.pompaOn ? wsp::ogranicz(st.tOgrz - 8.0f, 20.0f, 70.0f) : st.tPokoj;
  st.tBojler = dazenie(st.tBojler, celBojler, st.pompaOn ? TAU_BOJLER_S : TAU_BOJLER_S * 3.0f);
  st.tBojler += szum(tSek, 0.05f, 0.013f);

  const float celPowrot = st.pompaOn ? (st.tOgrz - 8.0f + szum(tSek, 0.4f, 0.01f))
                                     : (st.tPokoj + 1.5f);
  st.tPowrot = dazenie(st.tPowrot, celPowrot, TAU_POWROT_S);

  const float celPokoj = 21.2f + (st.pompaOn ? 0.7f : 0.0f);
  st.tPokoj = dazenie(st.tPokoj, celPokoj, TAU_POKOJ_S) + szum(tSek, 0.02f, 0.0035f);

  const float celTrociny = (st.tOgrz > 30.0f) ? (st.tOgrz - 12.0f) : st.tPokoj;
  st.tTrociny = dazenie(st.tTrociny, celTrociny, TAU_TROCINY_S);

  // --- 7. Mieszadlo (cykl ON/OFF + reczny override) ---
  if (st.mieszadloOverrideDo != 0) {
    // stan ustawiony bezposrednio przez /api/mieszadlo-manual
  } else {
    const bool pracaDozwolona =
        u.mieszadloWlaczony && (st.rozpalanie || st.tOgrz >= (float)ustaw::TEMP_MIN_POMPY);
    if (!pracaDozwolona) {
      st.mieszadloOn = false;
      st.mieszadloFazaMs = terazMs;
    } else if (st.mieszadloOn) {
      if (terazMs - st.mieszadloFazaMs >= (unsigned long)u.mieszadloCzasOn * 1000UL) {
        st.mieszadloOn = false;
        st.mieszadloFazaMs = terazMs;
      }
    } else {
      if (terazMs - st.mieszadloFazaMs >= (unsigned long)u.mieszadloCzasOff * 60000UL) {
        st.mieszadloOn = true;
        st.mieszadloFazaMs = terazMs;
      }
    }
  }
  // Rozpalanie gasnie samo po osiagnieciu progu (jak w mieszadlo() centrali).
  if (st.rozpalanie && st.tOgrz >= (float)ustaw::TEMP_MIN_POMPY) {
    st.rozpalanie = false;
  }
  u.rozpalanie = st.rozpalanie;

  // --- 8. Czujnik dymu + alarmy ---
  st.dymWlaczonyTemp = (st.tOgrz >= (float)u.dymProgTemp);
  int dymBaza = 240 + (int)(szum(tSek, 30.0f, 0.021f)) + (st.klapaDeg > 150 ? 70 : 0);
  if (st.injDym) dymBaza += 1400;  // "podlozenie ognia" do testu alarmu
  st.dym = wsp::ogranicz(dymBaza, 0, 4095);
  st.dymSwiezy = (u.dymTrybPracy == 1) ? true : ((terazMs / 1000UL) % 60 < 20);

  // Alarm dymu: zapala sie po dymCzasStabilizacji nad progiem, gasnie po
  // dymCzasOff pod progiem (prog - HISTEREZA_DYMU) — 1:1 z pozar() centrali.
  const int progZalaczenia = u.progAlarmDym;
  const int progWygaszenia = u.progAlarmDym - ustaw::HISTEREZA_DYMU;
  if (st.dym > progZalaczenia) {
    st.dymPodProgiemOd = 0;
    if (st.dymNadProgiemOd == 0) st.dymNadProgiemOd = terazMs;
    if (!st.alarmDym &&
        (terazMs - st.dymNadProgiemOd) >= (unsigned long)u.dymCzasStabilizacji * 1000UL) {
      st.alarmDym = true;
    }
  } else {
    st.dymNadProgiemOd = 0;
    if (st.alarmDym && st.dym < progWygaszenia) {
      if (st.dymPodProgiemOd == 0) st.dymPodProgiemOd = terazMs;
      if ((terazMs - st.dymPodProgiemOd) >= (unsigned long)u.dymCzasOff * 1000UL) {
        st.alarmDym = false;
        st.dymPodProgiemOd = 0;
      }
    } else if (st.dym >= progWygaszenia) {
      st.dymPodProgiemOd = 0;
    }
  }

  // Alarm CO (przegrzanie pieca) — z histereza 5 C przy wygaszaniu.
  if (st.injAlarmOgrz || st.tOgrz >= (float)u.progAlarmTemp) {
    st.alarmOgrz = true;
  } else if (!st.injAlarmOgrz && st.tOgrz < (float)u.progAlarmTemp - 5.0f) {
    st.alarmOgrz = false;
  }

  // Alarm panelu slonecznego — prog 95 C, histereza 10 C (iniekcja z API).
  if (st.injAlarmPanel || st.tPanel >= 95.0f) {
    st.alarmPanel = true;
  } else if (!st.injAlarmPanel && st.tPanel < 85.0f) {
    st.alarmPanel = false;
  }

  // --- 9. Probka historii (co 60 s wg zegara, z fallbackiem na uptime) ---
  const unsigned long znacznikSek = czas.ok ? (unsigned long)czas.unix : (terazMs / 1000UL);
  if (st.ostatniaProbkaSek == 0) {
    zapiszProbke(czas);
    st.ostatniaProbkaSek = znacznikSek;
  } else if (znacznikSek >= st.ostatniaProbkaSek &&
             znacznikSek - st.ostatniaProbkaSek >= PROBKA_CO_MS / 1000UL) {
    zapiszProbke(czas);
    st.ostatniaProbkaSek = znacznikSek;
  }
}

// ===========================================================================
// ZADANIA Z API
// ===========================================================================
int minutyDo(unsigned long terazMs, unsigned long docelowy) {
  if (docelowy == 0 || docelowy <= terazMs) return 0;
  return (int)((docelowy - terazMs) / 60000UL + 1);
}

bool zlecSerwo(const char* os, int procent, ustaw::Stan& u, unsigned long terazMs,
               char* err, size_t errLen) {
  if (u.trybSerwa != 2) {
    if (err && errLen) snprintf(err, errLen, "tryb serwa nie jest reczny");
    return false;
  }
  if (!strcmp(os, "klapa")) {
    st.celKlapaDeg = procentNaKat(wsp::ogranicz(procent, 0, 100), KLAPA_MAX_DEG);
  } else if (!strcmp(os, "syberek")) {
    st.celSyberkaDeg = procentNaKat(wsp::ogranicz(procent, 0, 100), SYBERKA_MAX_DEG);
  } else {
    if (err && errLen) snprintf(err, errLen, "os musi byc klapa/syberek");
    return false;
  }
  // Auto-wygasniecie trybu Recznego po 60 min — jak w /api/servo-manual
  // centrali (SYMULACJA_DOMYSLNY_CZAS_MS).
  st.serwoOverrideDo = terazMs + ustaw::POMPA_OVERRIDE_MS;
  return true;
}

bool zlecPompe(const char* stan, unsigned long terazMs, char* err, size_t errLen) {
  if (!strcmp(stan, "auto")) {
    st.pompaOverrideDo = 0;
    return true;
  }
  if (!strcmp(stan, "wl")) {
    pompaUstaw(true, true, terazMs);
    st.pompaOverrideStan = true;
    st.pompaOverrideDo = terazMs + ustaw::POMPA_OVERRIDE_MS;
    return true;
  }
  if (!strcmp(stan, "wyl")) {
    pompaUstaw(false, true, terazMs);
    st.pompaOverrideStan = false;
    st.pompaOverrideDo = terazMs + ustaw::POMPA_OVERRIDE_MS;
    return true;
  }
  if (err && errLen) snprintf(err, errLen, "stan musi byc auto/wl/wyl");
  return false;
}

bool zlecMieszadlo(const char* stan, unsigned long terazMs, char* err, size_t errLen) {
  if (!strcmp(stan, "auto")) {
    st.mieszadloOverrideDo = 0;
    return true;
  }
  if (!strcmp(stan, "wl")) {
    st.mieszadloOn = true;
    st.mieszadloOverrideDo = terazMs + ustaw::POMPA_OVERRIDE_MS;
    return true;
  }
  if (!strcmp(stan, "wyl")) {
    st.mieszadloOn = false;
    st.mieszadloOverrideDo = terazMs + ustaw::POMPA_OVERRIDE_MS;
    return true;
  }
  if (err && errLen) snprintf(err, errLen, "stan musi byc auto/wl/wyl");
  return false;
}

// ===========================================================================
// JSON: /api/status
// ===========================================================================
// Kolejnosc i nazwy pol 1:1 z wifiZbudujJsonStatus() w main_centrala.cpp.
// Panel LCD parsuje po nazwach (ArduinoJson), wiec kolejnosc nie jest krytyczna,
// ale trzymamy ja zgodna, zeby latwo bylo porownac z prawdziwa centrala.
size_t zbudujStatus(char* out, size_t n, const Kontekst& k, const ustaw::Stan& u,
                    const wsp::Czas& czas) {
  wsp::JsonBuf j(out, n);
  j.poczatek();

  j.pole("wifi_mode", k.wifiMode);
  j.pole("ip", k.ip);
  j.pole("wifi_ssid", k.wifiSsid);
  j.poleInt("rssi", k.rssi);

  j.poleFloat("t_zewn", st.tZewn, 1);
  j.poleFloat("t_bojler", st.tBojler, 1);
  j.poleFloat("t_ogrz", st.tOgrz, 1);
  j.poleFloat("t_ogrz_sr", st.tOgrzSr, 1);
  j.poleFloat("t_panel", st.tPanel, 1);
  j.poleFloat("t_pokoj", st.tPokoj, 1);
  j.poleFloat("t_trociny", st.tTrociny, 1);
  j.poleFloat("t_powrot", st.tPowrot, 1);
  j.poleFloat("wilgotnosc", st.wilgotnosc, 0);
  j.poleFloat("cisnienie", st.cisnienie, 2);

  j.poleInt("dym", st.dym);
  j.poleBool("dym_swiezy", st.dymSwiezy);
  j.poleBool("dym_wlaczony", st.dymWlaczonyTemp);
  j.poleBool("dym_alarm", st.alarmDym);

  // UWAGA: stopnie, nie procent — patrz komentarz przy KLAPA_MAX_DEG.
  j.poleInt("klapa", st.klapaDeg);
  j.poleInt("syberka", st.syberkaDeg);
  j.poleInt("tryb_serwa", u.trybSerwa);

  j.poleBool("pompa", st.pompaOn);
  j.poleInt("pompa_ms_od_zmiany", (long)(k.terazMs - st.pompaZmianaMs));
  j.poleInt("pompa_override_min", minutyDo(k.terazMs, st.pompaOverrideDo));

  j.poleInt("wybor", u.wybor);
  j.poleBool("mieszadlo", st.mieszadloOn);
  j.poleBool("rozpalanie", st.rozpalanie);
  j.poleBool("alarm_ogrzewanie", st.alarmOgrz);
  j.poleBool("alarm_panel", st.alarmPanel);

  // Czas: 0 gdy niewiarygodny (panel traktuje to jako "brak RTC").
  const int miesiac = czas.ok ? czas.miesiac : 0;
  j.poleInt("miesiac", miesiac);
  j.poleInt("godzina", czas.ok ? czas.godzina : 0);
  j.poleInt("minuta", czas.ok ? czas.minuta : 0);
  j.poleInt("dzien", czas.ok ? czas.dzien : 0);
  j.poleInt("rok", czas.ok ? czas.rok : 0);
  j.poleBool("rtc_ok", czas.ok);

  j.poleBool("internet", k.internet);
  j.poleBool("panel_ota_dostepna", k.panelOtaDostepna);
  j.pole("panel_ota_wersja", k.panelOtaWersja);

  // Pola symulacji per czujnik — panel ich nie czyta, ale panel WWW
  // prawdziwej centrali tak; zostawiamy dla zgodnosci ksztaltu JSON-a.
  j.poleBool("symulacja_dym", st.injDym);
  j.poleInt("symulacja_dym_min", 0);
  j.poleBool("symulacja_ogrz", st.injAlarmOgrz);
  j.poleInt("symulacja_ogrz_min", 0);
  j.poleBool("symulacja_bojler", false);
  j.poleInt("symulacja_bojler_min", 0);
  j.poleBool("symulacja_panel", st.injAlarmPanel);
  j.poleInt("symulacja_panel_min", 0);
  j.poleBool("symulacja_pokoj", false);
  j.poleInt("symulacja_pokoj_min", 0);
  j.poleBool("symulacja_zewn", false);
  j.poleInt("symulacja_zewn_min", 0);
  j.poleBool("symulacja_ogrz_powrot", false);
  j.poleInt("symulacja_ogrz_powrot_min", 0);
  j.poleBool("symulacja_ogrz_trociny", false);
  j.poleInt("symulacja_ogrz_trociny_min", 0);
  j.poleBool("symulacja_cisnienie", false);
  j.poleInt("symulacja_cisnienie_min", 0);
  j.poleBool("symulacja_wilgotnosc", false);
  j.poleInt("symulacja_wilgotnosc_min", 0);

  j.poleInt("serwo_override_min", minutyDo(k.terazMs, st.serwoOverrideDo));
  j.poleInt("mieszadlo_override_min", minutyDo(k.terazMs, st.mieszadloOverrideDo));
  j.poleInt("deszcz_stan", st.deszczStan);
  j.poleInt("deszcz_min", st.deszczMin);

  // Pola dodatkowe, wylacznie diagnostyczne (panel ich nie zna i ignoruje).
  j.poleBool("symulator", true);
  j.pole("symulator_panel_fw", k.panelFwZgloszony);
  j.pole("symulator_panel_reset", k.panelResetPowod);
  j.poleInt("symulator_stacje", k.stacje);
  j.poleInt("symulator_heap", (long)k.heapFree);
  j.poleInt("symulator_uptime_s", (long)k.uptimeS);

  j.koniec();
  return j.dlugosc();
}

// ===========================================================================
// JSON: /api/ustawienia-get
// ===========================================================================
size_t zbudujUstawienia(char* out, size_t n, const ustaw::Stan& u) {
  wsp::JsonBuf j(out, n);
  j.poczatek();
  j.poleInt("wybor", u.wybor);
  j.poleInt("trybSerwa", u.trybSerwa);
  j.poleInt("czasOn", u.czasOn);
  j.poleInt("czasOff", u.czasOff);
  j.poleInt("tempOn", u.tempOn);
  j.poleInt("tempOff", u.tempOff);
  j.poleBool("antystopWlaczony", u.antystopWlaczony);
  j.poleInt("antystopDni", u.antystopDni);
  j.poleBool("mieszadloWlaczony", u.mieszadloWlaczony);
  j.poleInt("mieszadloCzasOn", u.mieszadloCzasOn);
  j.poleInt("mieszadloCzasOff", u.mieszadloCzasOff);
  j.poleBool("rozpalanie", st.rozpalanie);
  j.poleInt("tempZadServo", u.tempZadServo);
  j.poleInt("histerServo", u.histerServo);
  j.poleInt("skokKlapy", u.skokKlapy);
  j.poleInt("skokSyberka", u.skokSyberka);
  j.poleInt("odchylTemp", u.odchylTemp);
  j.poleInt("mnoznik", u.mnoznik);
  j.poleInt("progAlarmTemp", u.progAlarmTemp);
  j.poleInt("progAlarmDym", u.progAlarmDym);
  j.poleInt("dymCzasOn", u.dymCzasOn);
  j.poleInt("dymCzasOff", u.dymCzasOff);
  j.poleInt("dymCzasStabilizacji", u.dymCzasStabilizacji);
  j.poleInt("dymProgTemp", u.dymProgTemp);
  j.poleInt("dymTrybPracy", u.dymTrybPracy);
  j.koniec();
  return j.dlugosc();
}

// ===========================================================================
// CSV HISTORII
// ===========================================================================
// Krotki bufor — 14 kolumn, dokladnie jak historiaLiniaCsv() centrali:
//  0 etykieta | 1 t_zewn | 2 t_bojler | 3 t_ogrz | 4 t_ogrz_sr | 5 t_powrot |
//  6 t_panel | 7 t_pokoj | 8 pompa(0/1) | 9 cisnienie | 10 wilgotnosc |
//  11 klapa | 12 syberko | 13 czas_unix
void historiaCsv(bool dlugi, wsp::PisarzFn pisarz, void* ctx) {
  if (!pisarz) return;
  char linia[224];
  if (!dlugi) {
    // Od najstarszej do najnowszej (pierscien).
    const int start = (histKrotkaLiczba < HIST_KROTKA_MAX) ? 0 : histKrotkaGlowa;
    for (int i = 0; i < histKrotkaLiczba; i++) {
      const int idx = (start + i) % HIST_KROTKA_MAX;
      const ProbkaKrotka& p = histKrotka[idx];
      char t1[16], t2[16], t3[16], t4[16], t5[16], t6[16], t7[16], tc[16], tw[16];
      wsp::formatFloat(t1, sizeof(t1), p.t_zewn_x10 / 10.0f, 1);
      wsp::formatFloat(t2, sizeof(t2), p.t_bojler_x10 / 10.0f, 1);
      wsp::formatFloat(t3, sizeof(t3), p.t_ogrz_x10 / 10.0f, 1);
      wsp::formatFloat(t4, sizeof(t4), p.t_ogrz_sr_x10 / 10.0f, 1);
      wsp::formatFloat(t5, sizeof(t5), p.t_powrot_x10 / 10.0f, 1);
      wsp::formatFloat(t6, sizeof(t6), p.t_panel_x10 / 10.0f, 1);
      wsp::formatFloat(t7, sizeof(t7), p.t_pokoj_x10 / 10.0f, 1);
      wsp::formatFloat(tc, sizeof(tc), p.cisnienie_x10 / 10.0f, 1);
      wsp::formatFloat(tw, sizeof(tw), p.wilgotnosc_x10 / 10.0f, 0);
      const int dl = snprintf(linia, sizeof(linia),
                              "%s,%s,%s,%s,%s,%s,%s,%s,%d,%s,%s,%d,%d,%lu\n",
                              p.etykieta, t1, t2, t3, t4, t5, t6, t7,
                              (int)p.pompa, tc, tw, (int)p.klapa_deg,
                              (int)p.syberka_deg, (unsigned long)p.czas_unix);
      if (dl > 0) pisarz(ctx, linia, (size_t)dl);
    }
    return;
  }

  // Dlugi bufor — 35 kolumn: etykieta + 11 pol x (sr,min,max) + czas_unix.
  const int start = (histDlugaLiczba < HIST_DLUGA_MAX) ? 0 : histDlugaGlowa;
  char bufor[512];
  int poz = 0;
  for (int i = 0; i < histDlugaLiczba; i++) {
    const int idx = (start + i) % HIST_DLUGA_MAX;
    const ProbkaDluga& p = histDluga[idx];
    poz = snprintf(bufor, sizeof(bufor), "%s", p.etykieta);
    for (int pole = 0; pole < POL_DLUGICH && poz > 0 && poz < (int)sizeof(bufor); pole++) {
      const int miejsca = (pole == 8) ? 0 : 1;  // wilgotnosc bez miejsc dziesietnych
      char a[16], b[16], c[16];
      wsp::formatFloat(a, sizeof(a), p.sr_x10[pole] / 10.0f, miejsca);
      wsp::formatFloat(b, sizeof(b), p.min_x10[pole] / 10.0f, miejsca);
      wsp::formatFloat(c, sizeof(c), p.max_x10[pole] / 10.0f, miejsca);
      poz += snprintf(bufor + poz, sizeof(bufor) - (size_t)poz, ",%s,%s,%s", a, b, c);
    }
    if (poz <= 0 || poz >= (int)sizeof(bufor)) continue;
    poz += snprintf(bufor + poz, sizeof(bufor) - (size_t)poz, ",%lu\n",
                    (unsigned long)p.czas_unix);
    if (poz > 0) pisarz(ctx, bufor, (size_t)poz);
  }
}

}  // namespace sym
