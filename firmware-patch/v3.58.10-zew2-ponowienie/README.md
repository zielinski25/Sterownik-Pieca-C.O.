# Firmware centrali 3.58.10 — Zew 2 bez przerw: ponowienie odczytu DHT22 po błędzie

Data: 2026-11-19 · następna po **3.58.9** (`firmware-patch/v3.58.9-zew2-scalenie/`)
i zawiera wszystko, co ona. Jeśli 3.58.9 nie była jeszcze kompilowana — wgraj od razu tę.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (50 789 linii, SHA-256 `0f5b7020…`).
Pliki `.h`, `secrets.h`, biblioteki i `platformio.ini` bez zmian. Patch względem 3.58.9:
**11 hunków, +88/−9**, wyłącznie blok Zew 2 (`ZEW2-FILTR`) i funkcja `wilgotnosc()`.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.**
> Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Skąd ta wersja

Zrzut z aplikacji 19.11 16:24, po wgraniu 3.58.9 (≈14:35): Zew 2 wróciła i biegnie
równolegle do Zew 1, ale ma **4 przerwy po 2–4 min** (≈14:45, 14:55, 15:05, 15:20).

Przyczyna jest w kodzie **od 3.57.10**, nie w filtrze z 3.58.9:

* DHT22 czytany co **120 s** (`wilgotnosc()`, bramka `ostatniaProbaCyklicznaMs`),
* `czytajTempDiag()` wygasza Zew 2 po **135 s** od ostatniej *przyjętej* wartości.

Jeden nieudany odczyt (timeout biblioteki, zła ramka, „odrzucony” po powtórce) →
następna próba dopiero za 120 s → 240 s między dobrymi wartościami → przez ostatnie
~105 s `"t_zewn_2": null` → 1–2 próbki minutowe z dziurą. Dwa nieudane cykle z rzędu =
4–5 min, jak na wykresie. Margines 15 s (135 − 120) był od początku za mały.
Powtórka z 3.58.9 (po 3 s) dotyczy tylko odczytów *podejrzanych*, nie błędów —
3.58.9 niczego tu nie pogorszyła (na 3.57.13 błąd kasował Zew 2 natychmiast).

## Co zmienia 3.58.10

1. **Ponowienie po błędzie** (`zew2PlanujPonowienie()` / `zew2PonowienieGotowe()` w bloku
   `ZEW2-FILTR`, bramka w `wilgotnosc()`): po zdarzeniu `zla_ramka` lub `odrzucony`
   następny odczyt DHT22 po **5 s** (zadanie chodzi co 3 s → praktycznie 6 s), najwyżej
   **2 razy w cyklu**; nowy cykl 120 s zeruje limit. Ponowienie, tak jak powtórka filtra,
   omija bramkę 120 s i **nie przesuwa cyklu**; odstęp ≥ 2 s między odczytami zachowany.
   Odczyt „podejrzany” nadal idzie ścieżką powtórki po 3 s; gdy powtórka da „odrzucony” —
   ponowienie po 5 s. Symulacja wilgotności kasuje oczekujące ponowienie.
   Log: `DHT22 Zew2 ponowienie=1/2 za 5 s kod=<kod biblioteki>`; licznik
   `zew2_ponowienia` w linii `DHT22 diagnostyka`.
2. **Wygaszanie Zew 2 135 s → 255 s** (`TEMP_DIAG_WYGASZANIE_S`): dwa cykle + 15 s.
   Dziura na wykresie dopiero, gdy zawiodą 2 kolejne cykle (6 prób). Dotyczy `t_zewn_2`
   w `/api/status`, `/piec/status` i próbkach minutowych (`telemetryBuildSnapshot`).
   Progi świeżości telemetrii (`TELEMETRY_STALE_*`, RH 135 s) bez zmian — RH korzysta
   z tych samych ponowień (ta sama ramka).

Stałe w jednym miejscu: `ZEW2_PONOWIENIE_MS 5000`, `ZEW2_PONOWIENIA_MAX 2` (linie 6266–6267),
`TEMP_DIAG_WYGASZANIE_S 255` (6232). Koszt: do 2 dodatkowych odczytów DHT22 po błędzie
(zwykle 0), ~12 B RAM. Numery linii wszystkich zmian: `WYNIKI/zakres-zmian.txt`.

`porownajWersje()` porównuje człony liczbowo — **3.58.10 > 3.58.9**, OTA z 3.58.9 przejdzie.

## Zew 1 (DS18B20) — „zęby” w dół na zrzucie

Dwa pojedyncze spadki po ~0,5 °C (≈13:50 i ≈14:20) na Zew 1 to inny czujnik (DS18B20 na
GPIO4, 12 bitów od 3.57.13). Sprawdziłem tor odczytu: harmonogram DS czeka 750 ms + tick
na konwersję i odrzuca odczyt bez potwierdzonej konwersji (`konwersja_timeout`), filtr
wiarygodności przepuszcza skoki ≤ 15 °C jako prawdziwe. Z punktu widzenia firmware 0,5 °C
w 120 s to normalny pomiar — bez logu z tych minut nie da się odróżnić podmuchu od szumu
czujnika. **Nic tu nie zmieniam**; jeśli zęby będą się powtarzać regularnie, przyślij
linie `tag=DS18B20` z tych chwil.

## Co było sprawdzone (`WYNIKI/`)

`tests/uruchom_testy.sh` (także `SANITIZER=1`) → `WYNIKI/testy-pc.txt`:

| część | co | wynik |
|---|---|---|
| A | blok ZEW2-FILTR wycięty z pliku: 12 scenariuszy z 3.58.9 + **[13]** ponowienie (zła ramka → 1/2, timeout → 2/2, limit, nowy cykl zeruje, podejrzany bez ponowienia, odrzucony → ponowienie → przyjęty, przepełnienie `millis()`) | 66 sprawdzeń, 0 błędów (g++ `-Werror`, ASan+UBSan) |
| B | **prawdziwa `wilgotnosc()`** z pliku + atrapy: 8 scenariuszy z 3.58.9 zaktualizowane (zła ramka → ponowienie po 6 s, nie 3 s; po timeout dokładnie 2 ponowienia, potem zwykły cykl 120 s; Zew 2 trzyma wartość 120–255 s, null po > 255 s; symulacja kasuje ponowienie) + **[9]** odrzucony po powtórce → ponowienie po 5 s daje prawdę | 47 sprawdzeń, 0 błędów, 26 odczytów DHT; test mutacyjny: bez ponowienia → 9 błędów, powrót do 135 s → 1 błąd |
| C | 29 znaczników (obie linie + 3.58.9 + 3.58.10), brak starych fragmentów (`135000000ULL`, stara bramka); patch na 3.58.9 **i** `zrob_35810.py` → ten sam SHA `0f5b7020…`; zgodność z `SHA256SUMS.txt` | OK |
| D | `sprawdz_pola_statusu.py`: 81 pól `/api/status` vs 47 czytanych przez panel LCD | zgodne |
| E | testy paczek 3.58.7 (START-TG 40) i 3.58.6 (OTA panelu 166, wykresy 2697) na tym pliku | OK |

`tests/sprawdz_naglowki_esp32.sh` (`RDZEN=3.3.8 OSTRZEZENIA=1`) → `WYNIKI/naglowki-esp32.txt`:
wycinki Zew 2 + `wilgotnosc()`, START-TG, OTA panelu i wykresów skompilowane clangiem 22
z **prawdziwymi nagłówkami arduino-esp32 3.3.8** — bez błędów, 0 ostrzeżeń.

**Nie było:** pełnego buildu PlatformIO, uruchomienia na płytce, testu na prawdziwym DHT22.

## Pliki

```
src/main_centrala.cpp                       v3.58.10, kompletny (tylko ten plik podmieniasz)
main_centrala-v3.58.9-do-v3.58.10.patch     -p1, na plik 3.58.9 (SHA 2b02197f…)
SHA256SUMS.txt                              sumy: wynik, patch, podstawa 3.58.9
CZYTAJ-NAJPIERW.txt                         3 kroki: kopia, Build/Upload, sprawdzenie
WYNIKI/zakres-zmian.txt                     co i gdzie (numery linii)
WYNIKI/testy-pc.txt, naglowki-esp32.txt     wydruki testów
tests/                                      skrypty (tests/README.txt); zrob_35810.py buduje
                                            3.58.10 z 3.58.9 — dokumentuje każdą zmianę
```

Powrót: kopia `.bak-przed-3.58.10` (3.58.9).
