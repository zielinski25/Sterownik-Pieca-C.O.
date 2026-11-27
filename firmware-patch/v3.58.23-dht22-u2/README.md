# Firmware centrali 3.58.23 — DHT22: kodowanie U2 16-bit potwierdzone, dekoder docelowy, porządki

Data: 2026-11-27 · następna po **3.58.22** (`firmware-patch/v3.58.22-dht22-log/`), zawiera wszystko, co ona.

Zmienia się **jeden plik**: `src/main_centrala.cpp` (51 656 linii, SHA-256 w `SHA256SUMS.txt`).
Patch względem 3.58.22: **8 hunków, ok. 42 linii** (większość komentarze). Dla użytkownika działa **tak samo jak 3.58.22**;
zmienia się wnętrze dekodera, znika zgadywany wariant 12-bit i WARN o „klonie”, linia `tag=DHT22` w `/logi` jest rzadsza.

> **Ten plik nie był kompilowany w PlatformIO ani uruchamiany na płytce.** Instrukcja: `CZYTAJ-NAJPIERW.txt`.

## Co potwierdził eksport 27.11 (3.58.22, sesja `2DBD4474…`, 07:43–08:00 UTC, 18 rekordów)
| Zew 1 (DS18B20) | `dht_kod` | `dht_surowa` | `t_zewn_2` |
|---|---|---|---|
| +1,3 … +1,8 °C | **0** w każdym rekordzie | −3275,6 / −3275,7 / −3275,8 | −1,2 / −1,1 / −1,0 |

−3275,6 + 3276,8 = 1,2 → słowo w ramce = **0xFFF4 = −12 (U2 16-bit) = −1,2 °C**. Czujnik koduje ujemne dwójkowo
(jak większość klonów z AliExpress), nie znak-modułem z karty katalogowej. Zero kodów 3/5/6 ⇒ transmisja na zimnie
w porządku; problem był wyłącznie w kodowaniu. Dekoder z 3.58.20 trafiał; zgadywany wariant 12-bit nigdy nie wystąpił.

## Zmiany
1. **`zew2DekodujU2(float *t)`** — kod docelowy: odtwarza słowo 16-bit tak, jak przyszło z czujnika
   (`0x8000 | |t|·10`), czyta je jako `int16_t` i dzieli przez 10; akceptuje wyłącznie wynik −400…−1 (−40,0…−0,1 °C).
   Wejście tylko w obrazie −3276,75…−3236,75 (to, co biblioteka oddaje dla 0xFFFF…0xFE70) — nieosiągalne dla
   prawdziwego DHT22, więc zwykłe wartości (także ujemne znak-modułem) przechodzą nietknięte.
   Usunięte: wariant 12-bit (+369,6…+409,5), stałe `ZEW2_U2_12_*`, `ZEW2_U2_BAZA_C`, parametr `wariant`.
2. **`wilgotnosc()`**: bez jednorazowego `WARN "klon koduje ujemne w U2, przeliczam"` (to normalne działanie tego
   egzemplarza); licznik `u2=` zostaje w diagnostyce i w `/logi`.
3. **Linia `tag=DHT22` w `/logi`**: pierwsze **15** prób po starcie (było 30) i zmiany ok↔zła (max raz/10 min);
   **bez „co 10.”** — stały podgląd jest w telemetrii (`dht_kod`/`dht_surowa`). 15 prób ≈ 30 min — ma pokazać,
   co się działo, gdyby powtórzyła się 18-minutowa cisza DHT po starcie 3.58.21 (06:47–07:05 UTC 27.11; po starcie
   3.58.22 czujnik odpowiedział od razu — przyczyna nieznana).

Bez zmian (celowo): filtr Zew 2 (skok / znak / powtórka 3 s / ponowienie 5 s ×2 — chroni przed przekłamanym bitem
znaku, piki +2,1 → −2,1 z 3.58.9 to nie efekt U2), odniesienie DS18B20 (3.58.21), pola telemetrii `dht_kod`/`dht_surowa`
(3.58.22), `/api/status`, WWW, LCD.

## Testy na PC (`tests/uruchom_testy.sh`, wynik w `WYNIKI/testy-pc.txt`)
- **A** blok `ZEW2-FILTR` z nowym dekoderem: `[1]`–`[18]` — **130 sprawdzeń, 0 błędów**. `[14]`: −3275,6/−3275,7/−3275,8
  z eksportu → −1,2/−1,1/−1,0; −3276,65 → −0,1 (zaokrąglenie do słowa); granice −3276,76 / −3236,74 odrzucone;
  +409,5/+408,8/+404,4/+369,6 (dawne 12-bit) zostają → zła ramka. `[15]`: 400 wartości U2 co do 0,1 °C, 400 znak-moduł
  i 801 dodatnich nietkniętych, 400 dawnych 12-bit nie ruszanych, U2 poniżej −40 °C odrzucone.
- **B** prawdziwa `wilgotnosc()`: `[1]`–`[15]` — **87 sprawdzeń, 0 błędów** (`[10]` bez WARN, `[12]` +408,8/+407,9 →
  zła ramka ×2 + timeout, historia filtra −1,2, RH nie przyjęta, `u2=4`; `[15]` 10 ramek po 15. próbie → 0 linii,
  zmiana ok→zła od razu, powrót < 10 min bez linii, po 10 min linia z `surowa=-3276.3 t=-0.5`).
- **B2** telemetria `dht_kod`/`dht_surowa`, maska `sim`, ABI 80 B — **25 sprawdzeń, 0 błędów** (bez zmian od 3.58.22).
- **C** znaczniki 3.58.23; plik = 3.58.22 + patch (SHA); HTML, telemetria, kod za `wilgotnosc()`, blok `ZEW2-FILTR` poza
  `zew2DekodujU2()` identyczne z 3.58.22; w `wilgotnosc()` dokładnie 3 miejsca (−7/+3 linii kodu).
- **D** testy paczki 3.58.22 (A/B/B2/C) na tym pliku: **16 niezaliczeń, każde jest skutkiem celowej zmiany**
  (wariant 16/12, dawne 12-bit, WARN u2, `u2=6`, rytm „co 10.”); żadna inna regresja. Także ASan+UBSan.

## Pliki
`src/main_centrala.cpp` · `main_centrala-v3.58.22-do-v3.58.23.patch` · `tests/` · `WYNIKI/zakres-zmian.txt` ·
`WYNIKI/testy-pc.txt` · `SHA256SUMS.txt` · `CZYTAJ-NAJPIERW.txt`.
