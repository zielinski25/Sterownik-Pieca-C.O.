#!/usr/bin/env python3
# Buduje main_centrala.cpp v3.58.10 z pliku v3.58.9 (SHA 2b02197f...).
# Uzycie: zrob_35810.py <main_centrala.cpp 3.58.9> <wyjscie 3.58.10>
# Kazda podmiana musi trafic dokladnie raz - inaczej skrypt sie zatrzymuje.
import sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()
if 'const char* FIRMWARE_VERSION = "v3.58.9";' not in t:
    sys.exit("to nie jest plik 3.58.9")

def zamien(stare, nowe, ile=1):
    global t
    n = t.count(stare)
    assert n == ile, (n, stare[:90])
    t = t.replace(stare, nowe)

# ── 1. naglowek pliku ───────────────────────────────────────────────────────
NAGLOWEK = '''// FIRMWARE_VERSION v3.58.9 -> v3.58.10 (2026-11-19) — ZEW 2 BEZ PRZERW: PONOWIENIE PO BŁĘDZIE DHT22 PO 5 s.
//
// [v3.58.10 ZEW2-PONOWIENIE] — 2026-11-19
// Zgłoszenie użytkownika (19.11, 16:24, wykres „Temperatury” po wgraniu 3.58.9):
// Zew 2 wróciła, ale ma przerwy po 2–4 min (≈14:45, 14:55, 15:05, 15:20).
//
// PRZYCZYNA (w kodzie od 3.57.10, nie w filtrze z 3.58.9): DHT22 czytany co
// 120 s, a czytajTempDiag() wygasza Zew 2 po 135 s od ostatniej PRZYJĘTEJ
// wartości. Jeden nieudany odczyt (timeout biblioteki, zła ramka, odrzucony
// po powtórce) → następna próba dopiero za 120 s → między dobrymi wartościami
// 240 s → przez ostatnie ~105 s "t_zewn_2": null → 1–2 próbki minutowe
// z dziurą; dwa nieudane cykle z rzędu = 4–5 min. Margines 15 s był za mały.
//
// CO ROBI 3.58.10 (tylko wilgotnosc() i blok ZEW2-FILTR):
// [1] PONOWIENIE PO BŁĘDZIE. Po nieudanym odczycie (zła ramka / błąd
//     biblioteki / odrzucony) wilgotnosc() czyta DHT22 ponownie po 5 s
//     (zadanie chodzi co 3 s → praktycznie 6 s), najwyżej 2 razy w cyklu;
//     odstęp ≥ 2 s między odczytami zachowany; ponowienie, tak jak powtórka
//     filtra, NIE przesuwa cyklu 120 s. Odczyt „podejrzany” nadal idzie
//     ścieżką powtórki po 3 s; gdy powtórka da „odrzucony” — ponowienie po 5 s.
//     Zdarzenie w logu: DHT22 Zew2 ponowienie=1/2 za 5 s kod=…; licznik
//     zew2_ponowienia w linii „DHT22 diagnostyka”.
// [2] WYGASZANIE 135 s → 255 s (TEMP_DIAG_WYGASZANIE_S). Dwa cykle + 15 s:
//     nawet gdy wszystkie 3 próby w cyklu zawiodą, Zew 2 trzyma ostatnią
//     dobrą wartość do następnego cyklu. Dotyczy tylko Zew 2 (t_zewn_2 w
//     /api/status, /piec/status i próbkach minutowych); progi świeżości
//     telemetrii (RH 135 s itd.) bez zmian — ponowienia pomagają też RH.
// Bez zmian: filtr pików (3 °C / znak / 0,5 °C / 3 s), sterowanie piecem,
//     alarmy, Telegram, OTA, format telemetrii i Firebase.
// Koszt: do 2 dodatkowych odczytów DHT22 po błędzie (zwykle 0); pamięć +~12 B.
//
'''
assert t.startswith("// FIRMWARE_VERSION v3.58.8 -> v3.58.9 (2026-11-19)")
t = NAGLOWEK + t

# ── 2. historia wersji ──────────────────────────────────────────────────────
zamien("// [v3.58.9] — 2026-11-19 — SCALENIE 3.57.13 + 3.58.8, FILTR ZEW 2\n",
       "// [v3.58.10] — 2026-11-19 — ZEW 2 BEZ PRZERW: PONOWIENIE PO BLEDZIE DHT22\n"
       "// Co: Po nieudanym odczycie DHT22 (zla ramka, blad biblioteki, odrzucony)\n"
       "//     ponowienie po 5 s (max 2 w cyklu 120 s, bez przesuwania cyklu);\n"
       "//     wygaszanie Zew 2 135 s -> 255 s (TEMP_DIAG_WYGASZANIE_S).\n"
       "// Problem: Zew 2 na wykresie miala przerwy po 2-4 min po kazdym nieudanym odczycie.\n"
       "// Przyczyna: cykl 120 s + wygaszanie 135 s = jeden blad to ~105 s bez wartosci.\n"
       "// Naprawa: zew2PlanujPonowienie()/zew2PonowienieGotowe() w bloku ZEW2-FILTR,\n"
       "//     bramka w wilgotnosc(); szczegoly w naglowku [v3.58.10 ZEW2-PONOWIENIE].\n"
       "//\n"
       "// [v3.58.9] — 2026-11-19 — SCALENIE 3.57.13 + 3.58.8, FILTR ZEW 2\n")

# ── 3. numer wersji ─────────────────────────────────────────────────────────
zamien('const char* FIRMWARE_VERSION = "v3.58.9";', 'const char* FIRMWARE_VERSION = "v3.58.10";')

# ── 4. wygaszanie Zew 2: 135 s -> 255 s ─────────────────────────────────────
zamien("static float czytajTempDiag() {\n",
       "// [v3.58.10] 135 s -> 255 s: dwa cykle DHT (120 s) + 15 s. Przy 135 s jeden\n"
       "// nieudany odczyt dawal ~105 s przerwy w Zew 2 (nastepna proba za 120 s).\n"
       "static const uint32_t TEMP_DIAG_WYGASZANIE_S = 255;\n"
       "static float czytajTempDiag() {\n")
zamien("  return isfinite(v) && now >= at && now - at <= 135000000ULL ? v : NAN;\n",
       "  return isfinite(v) && now >= at && now - at <= (uint64_t)TEMP_DIAG_WYGASZANIE_S * 1000000ULL ? v : NAN;\n")
zamien("// zostaje poprzednia wartosc - czytajTempDiag() wygasza ja po 135 s).\n",
       "// zostaje poprzednia wartosc - czytajTempDiag() wygasza ja po TEMP_DIAG_WYGASZANIE_S = 255 s).\n")

# ── 5. blok ZEW2-FILTR: pola, stale, planowanie ponowienia ─────────────────
zamien("  uint32_t przyjete, odrzucone, potwierdzone, zleRamki;\n};\n",
       "  uint32_t przyjete, odrzucone, potwierdzone, zleRamki;\n"
       "  // [v3.58.10] ponowienie po nieudanym odczycie (zla ramka / blad / odrzucony)\n"
       "  bool ponowienie;              // true = wilgotnosc() ma czytac DHT za ~5 s, nie za 120 s\n"
       "  unsigned long ponowienieOdMs; // kiedy zaplanowano\n"
       "  uint8_t ponowieniaWCyklu;     // ile ponowien juz bylo w biezacym cyklu 120 s\n"
       "  uint32_t ponowienia;          // licznik laczny (diagnostyka)\n"
       "};\n")
zamien("static const unsigned long ZEW2_WERYFIKACJA_MAX_MS = 15000UL; // po tylu ms bez powtorki kandydat przepada\n",
       "static const unsigned long ZEW2_WERYFIKACJA_MAX_MS = 15000UL; // po tylu ms bez powtorki kandydat przepada\n"
       "static const unsigned long ZEW2_PONOWIENIE_MS = 5000UL;      // [v3.58.10] po bledzie nastepna proba za 5 s (zadanie co 3 s -> ~6 s)\n"
       "static const uint8_t ZEW2_PONOWIENIA_MAX = 2;                // [v3.58.10] najwyzej tyle dodatkowych prob w cyklu 120 s\n")
zamien("  f.ostatniaDobra = t; ++f.przyjete; *wynik = t;\n  return ZEW2_PRZYJETY;\n}\n// --- ZEW2-FILTR-KONIEC ---\n",
       "  f.ostatniaDobra = t; ++f.przyjete; *wynik = t;\n  return ZEW2_PRZYJETY;\n}\n"
       "\n"
       "// [v3.58.10] Po decyzji filtra: czy czytac DHT jeszcze raz za ZEW2_PONOWIENIE_MS?\n"
       "// Tak po zlej ramce / bledzie biblioteki / odrzuceniu, najwyzej ZEW2_PONOWIENIA_MAX\n"
       "// razy w jednym cyklu 120 s (cykliczna = ta proba otworzyla nowy cykl). Odczyt\n"
       "// podejrzany nie planuje ponowienia - ma swoja powtorke po 3 s (weryfikacja).\n"
       "static void zew2PlanujPonowienie(Zew2Filtr &f, Zew2Decyzja d, bool cykliczna, unsigned long nowMs) {\n"
       "  if (cykliczna) f.ponowieniaWCyklu = 0;\n"
       "  const bool nieudany = (d == ZEW2_ZLA_RAMKA || d == ZEW2_ODRZUCONY);\n"
       "  if (nieudany && f.ponowieniaWCyklu < ZEW2_PONOWIENIA_MAX) {\n"
       "    f.ponowienie = true; f.ponowienieOdMs = nowMs; ++f.ponowieniaWCyklu; ++f.ponowienia;\n"
       "  } else {\n"
       "    f.ponowienie = false;\n"
       "  }\n"
       "}\n"
       "static bool zew2PonowienieGotowe(const Zew2Filtr &f, unsigned long nowMs) {\n"
       "  return f.ponowienie && (unsigned long)(nowMs - f.ponowienieOdMs) >= ZEW2_PONOWIENIE_MS;\n"
       "}\n"
       "// --- ZEW2-FILTR-KONIEC ---\n")
zamien("static Zew2Filtr g_zew2Filtr = {NAN, NAN, 0, false, 0, 0, 0, 0};\n",
       "static Zew2Filtr g_zew2Filtr = {NAN, NAN, 0, false, 0, 0, 0, 0, false, 0, 0, 0};\n")

# ── 6. wilgotnosc(): bramka, symulacja, planowanie, logi ───────────────────
zamien("  // [v3.58.9 ZEW2-FILTR] Powtorka podejrzanego odczytu omija bramke 120 s\n"
       "  // (zadanie chodzi co 3 s; ponizsza bramka 2 s pilnuje odstepu DHT22).\n"
       "  const bool powtorkaZew2 = g_zew2Filtr.weryfikacja;\n"
       "  if (!powtorkaZew2 && bylaProbaCykliczna && (unsigned long)(czasBiezacy - ostatniaProbaCyklicznaMs) < 120000UL) return;\n",
       "  // [v3.58.9 ZEW2-FILTR] Powtorka podejrzanego odczytu omija bramke 120 s\n"
       "  // (zadanie chodzi co 3 s; ponizsza bramka 2 s pilnuje odstepu DHT22).\n"
       "  // [v3.58.10] To samo ponowienie po nieudanym odczycie (po 5 s, max 2 w cyklu).\n"
       "  const bool powtorkaZew2 = g_zew2Filtr.weryfikacja;\n"
       "  const bool ponowienieZew2 = zew2PonowienieGotowe(g_zew2Filtr, czasBiezacy);\n"
       "  const bool dodatkowaZew2 = powtorkaZew2 || ponowienieZew2;\n"
       "  if (!dodatkowaZew2 && bylaProbaCykliczna && (unsigned long)(czasBiezacy - ostatniaProbaCyklicznaMs) < 120000UL) return;\n")
zamien("  if (!powtorkaZew2) {                 // powtorka nie przesuwa cyklu 120 s\n",
       "  if (!dodatkowaZew2) {                // powtorka ani ponowienie nie przesuwaja cyklu 120 s\n")
zamien("    g_zew2Filtr.weryfikacja = false; g_zew2Filtr.kandydat = NAN; // [v3.58.9]\n",
       "    g_zew2Filtr.weryfikacja = false; g_zew2Filtr.kandydat = NAN; // [v3.58.9]\n"
       "    g_zew2Filtr.ponowienie = false;                                // [v3.58.10]\n")
zamien("      default: break;\n    }\n    // Diagnostyka bez odrzucania prawidlowych 0 C. Kody pochodza z biblioteki.\n",
       "      default: break;\n    }\n"
       "    // [v3.58.10] Po nieudanym odczycie nastepna proba za 5 s (max 2 w cyklu),\n"
       "    // nie za 120 s - inaczej Zew 2 znikala z wykresu na 2-4 min.\n"
       "    zew2PlanujPonowienie(g_zew2Filtr, zew2Dec, !dodatkowaZew2, czasBiezacy);\n"
       "    if (g_zew2Filtr.ponowienie) {\n"
       "      DLOGF(DC_SENSOR, DL_INFO, \"DHT22 Zew2 ponowienie=%u/%u za 5 s kod=%u\",\n"
       "            (unsigned)g_zew2Filtr.ponowieniaWCyklu, (unsigned)ZEW2_PONOWIENIA_MAX, (unsigned)dhtErr);\n"
       "    }\n"
       "    // Diagnostyka bez odrzucania prawidlowych 0 C. Kody pochodza z biblioteki.\n")
zamien('    DLOGF(DC_SENSOR, DL_INFO, "DHT22 diagnostyka proby=%lu bledy_rh=%lu zera_temp=%lu kod=%u zew2_przyjete=%lu zew2_potwierdzone=%lu zew2_odrzucone=%lu",\n'
       "          (unsigned long)dhtProby, (unsigned long)dhtBledy, (unsigned long)dhtZeraC, (unsigned)dhtErr,\n"
       "          (unsigned long)g_zew2Filtr.przyjete, (unsigned long)g_zew2Filtr.potwierdzone, (unsigned long)g_zew2Filtr.odrzucone);\n",
       '    DLOGF(DC_SENSOR, DL_INFO, "DHT22 diagnostyka proby=%lu bledy_rh=%lu zera_temp=%lu kod=%u zew2_przyjete=%lu zew2_potwierdzone=%lu zew2_odrzucone=%lu zew2_ponowienia=%lu",\n'
       "          (unsigned long)dhtProby, (unsigned long)dhtBledy, (unsigned long)dhtZeraC, (unsigned)dhtErr,\n"
       "          (unsigned long)g_zew2Filtr.przyjete, (unsigned long)g_zew2Filtr.potwierdzone, (unsigned long)g_zew2Filtr.odrzucone,\n"
       "          (unsigned long)g_zew2Filtr.ponowienia);\n")

# ── 7. log startowy ─────────────────────────────────────────────────────────
zamien('F("v3.58.9 odczyty: DS osobno; CO/powrot 3/15/60s; trociny/panel/bojler 60s; pokoj/zew1 120s (+powtorka Zew2 po 3 s); DHT 120s; BMP 180s")',
       'F("v3.58.10 odczyty: DS osobno; CO/powrot 3/15/60s; trociny/panel/bojler 60s; pokoj/zew1 120s; DHT 120s (+powtorka Zew2 po 3 s, ponowienie po bledzie 5 s x2, wygaszanie 255 s); BMP 180s")')

open(dst, "w", encoding="utf-8", newline="\n").write(t)
print("zapisano %s (%d linii)" % (dst, t.count("\n")))
