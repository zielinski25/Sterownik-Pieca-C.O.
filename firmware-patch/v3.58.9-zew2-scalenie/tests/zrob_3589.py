#!/usr/bin/env python3
# Buduje main_centrala.cpp v3.58.9 z pliku merged2.cpp (3-way merge 3.57.13 + 3.58.8,
# podstawa 3.57.9; konflikty: naglowek -> @@NAGLOWEK@@, FIRMWARE_VERSION -> v3.58.9).
import sys, re
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()

def zamien(stare, nowe, ile=1):
    global t
    n = t.count(stare)
    assert n == ile, (n, stare[:80])
    t = t.replace(stare, nowe)

# ── 1. naglowek pliku ───────────────────────────────────────────────────────
NAGLOWEK = '''// FIRMWARE_VERSION v3.58.8 -> v3.58.9 (2026-11-19) — POŁĄCZENIE LINII 3.57.13 (ZEW 2) I 3.58.8 + FILTR ZEW 2.
//
// [v3.58.9 ZEW2-SCALENIE] — 2026-11-19
// Zgłoszenie użytkownika (19.11, wykres „Temperatury” w aplikacji): po wgraniu
// 3.58.8 ok. 10:40 seria „Zew 2” (temperatura z DHT22) zniknęła („Zew 2: —”),
// „Zew 1” (DS18B20) działa dalej. Na płytce do tej pory chodziła v3.57.13.
//
// PRZYCZYNA: dwie rozjechane linie firmware z jednego pnia v3.57.9 (1.11):
//   * 3.58.0 → 3.58.8 (10–19.11, repozytorium): panel LAN, OTA panelu,
//     wykresy, START-TG, pole "fw" w /api/status;
//   * 3.57.10 → 3.57.13 (15–17.11, paczki KANDYDAT na Dysku Google, poza
//     repozytorium): Zew 2 = temperatura DHT22 w statusie i próbkach
//     Firebase ("t_zewn_2"), nowy harmonogram DS18B20, 12 bitów na GPIO4,
//     DHT co 120 s, progi świeżości, FB_JSON_CAP 4096.
//   Paczki 3.58.7 i 3.58.8 były robione na 3.58.6 z repozytorium, nie na
//   3.57.13 z płytki — nie zawierały więc niczego z 3.57.10–3.57.13.
//   Wgranie 3.58.8 zdjęło z płytki całą linię 3.57.1x, w tym "t_zewn_2".
//
// CO ROBI 3.58.9:
// [1] SCALENIE. Plik = 3.57.13 (podstawa wskazana przez użytkownika) + wszystkie
//     zmiany 3.58.0–3.58.8 (3-way merge względem wspólnej podstawy 3.57.9;
//     2 konflikty: nagłówek pliku i FIRMWARE_VERSION — oba rozwiązane ręcznie).
//     Numer 3.58.9, a nie 3.57.14, bo porownajWersje() w OTA odmawia
//     downgrade'u — Release 3.57.14 nie wgrałby się po OTA na płytkę
//     z 3.58.8. Funkcjonalnie: płytka zachowuje się jak na 3.57.13 i ma
//     wszystko z 3.58.x.
// [2] FILTR ZEW 2 (nowe). Na 3.57.13 wykres Zew 2 miał co ~godzinę pojedyncze
//     piki do ok. −2 °C przy rzeczywistych ok. +2 °C. To obraz przekłamanego
//     bitu znaku (bit 15 słowa temperatury; DHT22 koduje znak osobnym bitem,
//     nie U2) przy POPRAWNEJ sumie kontrolnej — suma kontrolna to zwykła suma
//     8-bitowa czterech bajtów, która nie wykrywa dwóch jednakowych błędów
//     o tej samej wadze w różnych bajtach (128+128 = 256 ≡ 0). Ten sam obraz
//     (pojedyncze −9,9 °C / −12 °C z dobrą sumą) opisują inni użytkownicy
//     DHT22 i leczą go filtrem odstających wartości.
//     Zasada (zew2Filtruj(), wilgotnosc()):
//       a) kontrola CAŁEJ ramki: temperatura −40…80 °C i wilgotność 1…100 %
//          — oba pola przychodzą w jednej ramce, więc nieprawdopodobne jedno
//          dyskwalifikuje też drugie (dotąd wilgotność > 100 % przechodziła);
//       b) odczyt „podejrzany” = różni się od ostatniej przyjętej wartości
//          o > 3 °C albo ma przeciwny znak przy |t| ≥ 0,5 °C (lustrzana
//          wartość = przekłamany bit znaku) → NIE jest przyjmowany; zadanie
//          wilgotnosc() powtarza odczyt po ~3 s (zamiast po 120 s) i przyjmuje
//          wartość tylko, gdy oba odczyty zgadzają się w granicach 0,5 °C;
//          gdy powtórka zgadza się z ostatnią przyjętą wartością — przyjmuje
//          powtórkę; gdy nie zgadza się z niczym — odrzuca obie i czeka na
//          następny cykl;
//       c) pierwszy odczyt po starcie jest przyjmowany bez weryfikacji; jeśli
//          był przekłamany, następny odczyt będzie „podejrzany”, zostanie
//          powtórzony po 3 s i potwierdzony — filtr sam się naprawia (znana
//          wada filtra outlier w Home Assistant, która blokuje się na złym
//          pierwszym odczycie, tu nie występuje);
//       d) odrzucona lub zła ramka NIE kasuje Zew 2 (dotąd zapiszTempDiag(NAN)
//          — seria znikała do następnego odczytu); zostaje poprzednia wartość,
//          którą czytajTempDiag() wygasza po 135 s jak dotąd.
//     Koszt: jeden dodatkowy odczyt DHT22 przy każdym podejrzanym odczycie
//     (≥ 2 s odstępu zachowane), opóźnienie prawdziwej zmiany o > 3 °C: 3 s.
//     Wilgotność: bez zmian poza (a); sterowanie piecem: bez zmian.
//     Logi: DHT22 Zew2 zdarzenie=podejrzany|potwierdzony|odrzucony|zla_ramka,
//     liczniki zew2_* w linii „DHT22 diagnostyka”.
// Bez zmian: automatyka, alarmy, Telegram, OTA, format telemetrii (t_zewn_2
//     jak w 3.57.10–3.57.13, aplikacja 0.31.94 czyta go bez zmian).
//
'''
zamien('@@NAGLOWEK@@\n', NAGLOWEK)

# ── 2. HISTORIA WERSJI ─────────────────────────────────────────────────────
zamien('''// ============================================================================
// HISTORIA WERSJI (CHANGELOG) — main.cpp / Piec (sterownik CO, ESP32-S3)
// ============================================================================
// [v3.57.10] — 2026-11-15''', '''// ============================================================================
// HISTORIA WERSJI (CHANGELOG) — main.cpp / Piec (sterownik CO, ESP32-S3)
// ============================================================================
// [v3.58.9] — 2026-11-19 — SCALENIE 3.57.13 + 3.58.8, FILTR ZEW 2
// Co: Jeden plik z obu linii (Zew 2 "t_zewn_2", harmonogram DS18B20, 12 bitow
//     z 3.57.1x; panel LAN, OTA panelu, START-TG, "fw" w /api/status z 3.58.x)
//     + filtr blednych ramek DHT22 dla Zew 2 (powtorka po 3 s, kontrola ramki).
// Problem: po wgraniu 3.58.8 na plytke z 3.57.13 zniknela seria Zew 2.
// Przyczyna: 3.58.x rozwijane od 3.57.9, bez zmian 3.57.10-3.57.13.
// Naprawa: 3-way merge; szczegoly w naglowku pliku [v3.58.9 ZEW2-SCALENIE].
//
// [v3.57.10] — 2026-11-15''')

# ── 3. filtr Zew 2 — definicje zaraz po jsonTempDiag() ─────────────────────
zamien('''static String jsonTempDiag() {
  const float v = czytajTempDiag();
  return isfinite(v) ? String(v, 1) : String("null");
}
''', '''static String jsonTempDiag() {
  const float v = czytajTempDiag();
  return isfinite(v) ? String(v, 1) : String("null");
}

// [v3.58.9 ZEW2-FILTR] Filtr blednych ramek DHT22 dla Zew 2 — opis w naglowku
// pliku. Czysta logika (bez sprzetu), zeby dalo sie ja testowac na PC:
// tests/test_zew2_filtr.cpp w paczce 3.58.9 wycina ten blok i uruchamia go.
// --- ZEW2-FILTR-POCZATEK ---
struct Zew2Filtr {
  float ostatniaDobra;          // ostatnia PRZYJETA temperatura DHT22 (NAN = brak)
  float kandydat;               // odczyt podejrzany, czeka na powtorke
  unsigned long kandydatOdMs;   // kiedy kandydat powstal (limit oczekiwania)
  bool weryfikacja;             // true = wilgotnosc() ma czytac DHT za ~3 s, nie za 120 s
  uint32_t przyjete, odrzucone, potwierdzone, zleRamki;
};
static const float ZEW2_PROG_SKOKU_C            = 3.0f;   // wiekszy skok -> powtorka
static const float ZEW2_PROG_ZNAKU_C            = 0.5f;   // zmiana znaku przy |t| >= 0,5 -> powtorka
static const float ZEW2_TOLERANCJA_POWTORKI_C   = 0.5f;   // tyle moga sie roznic dwa odczyty
static const unsigned long ZEW2_WERYFIKACJA_MAX_MS = 15000UL; // po tylu ms bez powtorki kandydat przepada

enum Zew2Decyzja : uint8_t {
  ZEW2_PRZYJETY = 0,       // zwykly odczyt, zapisany
  ZEW2_DO_WERYFIKACJI = 1, // podejrzany: nic nie zapisano, powtorka za ~3 s
  ZEW2_POTWIERDZONY = 2,   // powtorka zgodna z kandydatem, zapisany
  ZEW2_ODRZUCONY = 3,      // powtorka niezgodna z kandydatem i z ostatnia wartoscia
  ZEW2_ZLA_RAMKA = 4       // blad odczytu albo wartosci poza zakresem (t lub RH)
};

// Ramka DHT22 niesie temperature i wilgotnosc razem; nieprawdopodobna jedna
// wartosc dyskwalifikuje cala ramke (suma kontrolna przepuszcza podwojne bledy).
static bool zew2RamkaPoprawna(uint8_t dhtErr, float t, float rh) {
  return dhtErr == DHT_OK && isfinite(t) && isfinite(rh) &&
         t >= -40.0f && t <= 80.0f && rh >= 1.0f && rh <= 100.0f;
}

static bool zew2Podejrzany(float t, float ostatnia) {
  if (!isfinite(ostatnia)) return false;                  // brak historii: przyjmij
  if (fabsf(t - ostatnia) > ZEW2_PROG_SKOKU_C) return true; // skok > 3 C w 120 s
  // Lustrzana wartosc (+2,1 -> -2,1): przeklamany bit znaku. Wokol 0 C
  // (|t| < 0,5) prawdziwe przejscia przez zero przechodza bez powtorki.
  return ((t < 0.0f) != (ostatnia < 0.0f)) &&
         fabsf(t) >= ZEW2_PROG_ZNAKU_C && fabsf(ostatnia) >= ZEW2_PROG_ZNAKU_C;
}

// Zwraca decyzje; *wynik = temperatura do zapisania (NAN = nic nie zapisuj,
// zostaje poprzednia wartosc - czytajTempDiag() wygasza ja po 135 s).
static Zew2Decyzja zew2Filtruj(Zew2Filtr &f, bool ramkaOk, float t, unsigned long nowMs, float *wynik) {
  *wynik = NAN;
  if (f.weryfikacja && (unsigned long)(nowMs - f.kandydatOdMs) > ZEW2_WERYFIKACJA_MAX_MS) {
    f.weryfikacja = false; f.kandydat = NAN; ++f.odrzucone;  // powtorka nie doszla do skutku
  }
  if (!ramkaOk) {
    ++f.zleRamki;
    if (f.weryfikacja) { f.weryfikacja = false; f.kandydat = NAN; ++f.odrzucone; }
    return ZEW2_ZLA_RAMKA;
  }
  const bool podejrzany = zew2Podejrzany(t, f.ostatniaDobra);
  if (f.weryfikacja) {
    f.weryfikacja = false;
    const float kand = f.kandydat;
    f.kandydat = NAN;
    if (fabsf(t - kand) <= ZEW2_TOLERANCJA_POWTORKI_C) {
      f.ostatniaDobra = t; ++f.potwierdzone; ++f.przyjete; *wynik = t;
      return ZEW2_POTWIERDZONY;                       // dwa zgodne odczyty = prawda
    }
    ++f.odrzucone;                                    // kandydat byl przeklamany
    if (!podejrzany) {
      f.ostatniaDobra = t; ++f.przyjete; *wynik = t;  // powtorka pasuje do historii
      return ZEW2_PRZYJETY;
    }
    return ZEW2_ODRZUCONY;                            // nic do niczego nie pasuje
  }
  if (podejrzany) {
    f.kandydat = t; f.kandydatOdMs = nowMs; f.weryfikacja = true;
    return ZEW2_DO_WERYFIKACJI;
  }
  f.ostatniaDobra = t; ++f.przyjete; *wynik = t;
  return ZEW2_PRZYJETY;
}
// --- ZEW2-FILTR-KONIEC ---
static Zew2Filtr g_zew2Filtr = {NAN, NAN, 0, false, 0, 0, 0, 0};
''')

# ── 4. wilgotnosc(): bramka 120 s z wyjatkiem dla powtorki ─────────────────
zamien('''void wilgotnosc() {
  static bool bylaProbaCykliczna = false;
  static unsigned long ostatniaProbaCyklicznaMs = 0;
  unsigned long czasBiezacy = millis();
  if (bylaProbaCykliczna && (unsigned long)(czasBiezacy - ostatniaProbaCyklicznaMs) < 120000UL) return;
  // Nie czytaj DHT wczesniej niz 2 s po PROBIE setup(), nawet przy dlugim rozruchu.
  if (dhtBylaProba && (unsigned long)(czasBiezacy - dhtOstatniaProbaMs) < 2000UL) return;
  bylaProbaCykliczna = true;
  ostatniaProbaCyklicznaMs = czasBiezacy;
''', '''void wilgotnosc() {
  static bool bylaProbaCykliczna = false;
  static unsigned long ostatniaProbaCyklicznaMs = 0;
  unsigned long czasBiezacy = millis();
  // [v3.58.9 ZEW2-FILTR] Powtorka podejrzanego odczytu omija bramke 120 s
  // (zadanie chodzi co 3 s; ponizsza bramka 2 s pilnuje odstepu DHT22).
  const bool powtorkaZew2 = g_zew2Filtr.weryfikacja;
  if (!powtorkaZew2 && bylaProbaCykliczna && (unsigned long)(czasBiezacy - ostatniaProbaCyklicznaMs) < 120000UL) return;
  // Nie czytaj DHT wczesniej niz 2 s po PROBIE setup(), nawet przy dlugim rozruchu.
  if (dhtBylaProba && (unsigned long)(czasBiezacy - dhtOstatniaProbaMs) < 2000UL) return;
  if (!powtorkaZew2) {                 // powtorka nie przesuwa cyklu 120 s
    bylaProbaCykliczna = true;
    ostatniaProbaCyklicznaMs = czasBiezacy;
  }
''')

# ── 5. wilgotnosc(): symulacja — zeruj oczekujaca powtorke ─────────────────
zamien('''    zapiszTempDiag(NAN); // symulowana wilgotnosc nie jest temperatura
    wilgotnoscOdczytanaLok = symulowanaWilgotnosc;''', '''    zapiszTempDiag(NAN); // symulowana wilgotnosc nie jest temperatura
    g_zew2Filtr.weryfikacja = false; g_zew2Filtr.kandydat = NAN; // [v3.58.9]
    wilgotnoscOdczytanaLok = symulowanaWilgotnosc;''')

# ── 6. wilgotnosc(): sam odczyt — filtr zamiast bezposredniego zapisu ──────
zamien('''    uint8_t dhtErr = read_dht(dhtTempPomijana, wilgotnoscOdczytanaLok, DHT_PIN, DHT22);
    zapiszTempDiag(dhtErr == DHT_OK && isfinite(dhtTempPomijana) &&
        dhtTempPomijana >= -40.0f && dhtTempPomijana <= 80.0f ? dhtTempPomijana : NAN);
    // Diagnostyka bez odrzucania prawidlowych 0 C. Kody pochodza z biblioteki.
    static uint32_t dhtProby = 0, dhtBledy = 0, dhtZeraC = 0;
    ++dhtProby;
    if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f) ++dhtBledy;
    if (dhtErr == DHT_OK && isfinite(dhtTempPomijana) && dhtTempPomijana == 0.0f) ++dhtZeraC;
    DLOGF(DC_SENSOR, DL_INFO, "DHT22 diagnostyka proby=%lu bledy_rh=%lu zera_temp=%lu kod=%u",
          (unsigned long)dhtProby, (unsigned long)dhtBledy, (unsigned long)dhtZeraC, (unsigned)dhtErr);
    // [v3.57.4] 0,0 % z DHT22 to odczyt smieciowy, nie pomiar - traktujemy go
    // jak blad (zostaje poprzednia dobra wartosc). W kotlowni nie ma 0 %.
    if (dhtErr != DHT_OK || !isfinite(wilgotnoscOdczytanaLok) || wilgotnoscOdczytanaLok <= 0.1f) {
      wilgotnoscOdczytanaLok = NAN;
      telemetryMarkInvalid(TELEMETRY_CH_RH);
    } else {
      telemetryMarkAccepted(TELEMETRY_CH_RH, telemetryRhNowMs, false);
    }
''', '''    uint8_t dhtErr = read_dht(dhtTempPomijana, wilgotnoscOdczytanaLok, DHT_PIN, DHT22);
    // [v3.58.9 ZEW2-FILTR] Bylo: zapiszTempDiag(t w zakresie ? t : NAN) - kazda
    // ramka z dobra suma kontrolna szla prosto na wykres (piki do -2 C), a zla
    // kasowala Zew 2. Teraz: kontrola calej ramki + powtorka podejrzanego
    // odczytu po ~3 s; odrzucenie zostawia poprzednia wartosc.
    const bool ramkaOk = zew2RamkaPoprawna(dhtErr, dhtTempPomijana, wilgotnoscOdczytanaLok);
    float tZew2 = NAN;
    const Zew2Decyzja zew2Dec = zew2Filtruj(g_zew2Filtr, ramkaOk, dhtTempPomijana, czasBiezacy, &tZew2);
    if (isfinite(tZew2)) zapiszTempDiag(tZew2);
    switch (zew2Dec) {
      case ZEW2_DO_WERYFIKACJI:
        DLOGF(DC_SENSOR, DL_WARN, "DHT22 Zew2 zdarzenie=podejrzany t=%.1f ostatnia=%.1f msg=\\"powtorka za 3 s\\"",
              (double)dhtTempPomijana, (double)g_zew2Filtr.ostatniaDobra);
        break;
      case ZEW2_POTWIERDZONY:
        DLOGF(DC_SENSOR, DL_INFO, "DHT22 Zew2 zdarzenie=potwierdzony t=%.1f", (double)dhtTempPomijana);
        break;
      case ZEW2_ODRZUCONY:
        DLOGF(DC_SENSOR, DL_WARN, "DHT22 Zew2 zdarzenie=odrzucony powtorka=%.1f ostatnia=%.1f msg=\\"dwa niezgodne odczyty\\"",
              (double)dhtTempPomijana, (double)g_zew2Filtr.ostatniaDobra);
        break;
      case ZEW2_ZLA_RAMKA:
        if (dhtErr == DHT_OK) {   // suma kontrolna dobra, wartosci nie - warte logu
          DLOGF(DC_SENSOR, DL_WARN, "DHT22 Zew2 zdarzenie=zla_ramka t=%.1f rh=%.1f",
                (double)dhtTempPomijana, (double)wilgotnoscOdczytanaLok);
        }
        break;
      default: break;
    }
    // Diagnostyka bez odrzucania prawidlowych 0 C. Kody pochodza z biblioteki.
    static uint32_t dhtProby = 0, dhtBledy = 0, dhtZeraC = 0;
    ++dhtProby;
    if (!ramkaOk) ++dhtBledy;
    if (dhtErr == DHT_OK && isfinite(dhtTempPomijana) && dhtTempPomijana == 0.0f) ++dhtZeraC;
    DLOGF(DC_SENSOR, DL_INFO, "DHT22 diagnostyka proby=%lu bledy_rh=%lu zera_temp=%lu kod=%u zew2_przyjete=%lu zew2_potwierdzone=%lu zew2_odrzucone=%lu",
          (unsigned long)dhtProby, (unsigned long)dhtBledy, (unsigned long)dhtZeraC, (unsigned)dhtErr,
          (unsigned long)g_zew2Filtr.przyjete, (unsigned long)g_zew2Filtr.potwierdzone, (unsigned long)g_zew2Filtr.odrzucone);
    // [v3.57.4] 0,0 % z DHT22 to odczyt smieciowy, nie pomiar - traktujemy go
    // jak blad (zostaje poprzednia dobra wartosc). W kotlowni nie ma 0 %.
    // [v3.58.9] To samo dla RH > 100 % i temperatury poza -40..80 C (cala ramka).
    if (!ramkaOk) {
      wilgotnoscOdczytanaLok = NAN;
      telemetryMarkInvalid(TELEMETRY_CH_RH);
    } else {
      telemetryMarkAccepted(TELEMETRY_CH_RH, telemetryRhNowMs, false);
    }
''')

# ── 7. log startowy z 3.57.13 -> 3.58.9 ────────────────────────────────────
zamien('DLOG(DC_SENSOR, DL_INFO, F("v3.57.13 odczyty: DS osobno; CO/powrot 3/15/60s; trociny/panel/bojler 60s; pokoj/zew1 120s; DHT 120s; BMP 180s"));',
       'DLOG(DC_SENSOR, DL_INFO, F("v3.58.9 odczyty: DS osobno; CO/powrot 3/15/60s; trociny/panel/bojler 60s; pokoj/zew1 120s (+powtorka Zew2 po 3 s); DHT 120s; BMP 180s"));')

assert '<<<<<<<' not in t and '>>>>>>>' not in t and '@@NAGLOWEK@@' not in t
assert t.count('FIRMWARE_VERSION = "v3.58.9"') == 1
open(dst, "w", encoding="utf-8", newline="\n").write(t)
print("OK", dst, len(t.splitlines()), "linii")
