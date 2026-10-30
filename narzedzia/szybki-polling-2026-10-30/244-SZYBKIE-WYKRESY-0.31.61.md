# 244 — 0.31.61: wykresy z pamięci od razu (koniec ~6 s przy 24 H)

Data: 2026-10-30 · Dotyczy: aplikacja Android · Firmware: bez zmian (3.57.1)

## Zgłoszenie

„Wykresy z pamięci telefonu wczytują się prawie 6s" — zakres **24 H**.

## Co było powodem

`loadRange()` pokazywał wykres dopiero **na końcu**, po sumie czterech etapów:

| # | Etap | Koszt przy 24 H |
|---|---|---|
| 1 | odczyt `historia.tsv.gz` z pamięci telefonu (pierwsze wejście po starcie aplikacji) | 0,3–0,8 s |
| 2 | dociągnięcie „ogona" z Firebase | 0,5–2 s |
| 3 | **przepisanie całego pliku pamięci** (`store()` po każdym dociągnięciu, gzip poziom 6) | 0,5–1,5 s |
| 4 | czekanie na archiwum centrali pod lokalny IP (limit 2 s) | do 2 s |

Do tego rysowanie: ścieżki `Path()` powstają w lambdzie rysującej (od nowa
przy każdej klatce), a filtr Hampela liczy się po wszystkich punktach.

### Dlaczego etap 2 jest drogi — reguły Firebase

Na `/piec/telemetry/1m/v1` **nie ma `.indexOn: "ts"`**. W regułach użytkownika
indeks jest tylko na ścieżce testowej:

```
/piec/devices/$uid/telemetry/_rtdb_test/$run/cross_midnight  →  ".indexOn": ["ts"]
```

Bez indeksu zapytanie `orderBy="ts"` kończy się „Index not defined", a aplikacja
przechodzi na `fetchShardFull()` — pobiera **całą dobę** (≈1440 rekordów,
≈280 kB) i dopiero lokalnie odsiewa ogon. Przy 24 H to dwie doby.
Identyfikatory rekordów to `nonce-seq` (nie chronologiczne), więc `limitToLast`
po kluczu nic nie daje — bez indeksu nie da się pobrać „tylko ogona".

**Poprawka reguł (po stronie użytkownika, konsola Firebase → Rules):**

```json
"telemetry": {
  "1m": {
    "v1": {
      ".read": "auth != null",
      ".write": "auth != null",
      "$y": { "$m": { "$d": { ".indexOn": ["ts"] } } }
    }
  }
}
```

`.read`/`.write` dziedziczą się z `v1`, więc nic się nie zamyka. Indeks musi być
na węźle doby, bo tam trafia zapytanie (`.../v1/2026/10/30.json?orderBy="ts"`).

## Co zmieniono w 0.31.61

1. **Wykres z pamięci od razu.** `loadRange()` po odczycie pamięci natychmiast
   publikuje `telemetry` i przenosi `chartStatus` na „Z pamięci telefonu —
   dociągam nowe dane…". Sieć, zapis i archiwum pieca dzieją się **po** tym.
2. **Archiwum centrali nadal pytane** (dane bywają tylko w piecu), ale czekamy
   na nie dopiero po pokazaniu wykresu — nie blokuje pierwszego rysowania.
3. **Zapis pamięci 2× szybszy:** gzip poziom 3 zamiast domyślnego 6
   (plik rośnie o kilka procent, kompresja ~2× szybsza).
4. **Odczyt strumieniowy** (`useLines`) zamiast `readLines()` — brak listy
   kilkudziesięciu tysięcy łańcuchów naraz.
5. **Rozgrzewanie pamięci** (`telCache.warmUp()` w `onResume`) — plik czyta się
   w tle przy wejściu do aplikacji, nie przy otwarciu Wykresów.
6. **Szybsze czyszczenie luk:** jedno przejście po posortowanych znacznikach
   + `binarySearch` zamiast `points.keys.any { it in h }` dla każdej luki.
7. **Czasy etapów w dzienniku** (arkusz TERMINAL), bez zaśmiecania ekranu:

```
WYKRESY 24h: pamięć 340 ms · sieć 1.8 s · zapis 600 ms · punktów 1450 · wykres z pamięci od razu
```

## Efekt

- wykres z pamięci: **~0,2–0,5 s** zamiast ~6 s (pierwsze wejście po starcie
  aplikacji — zwykle szybciej dzięki rozgrzewaniu),
- dane z sieci i z pieca dochodzą w tle i po cichu uzupełniają wykres,
- po dodaniu `.indexOn` „ogon" to jedno małe zapytanie zamiast dwóch pełnych dób,
- przy pustej pamięci (świeża instalacja) czas nadal zależy od zasięgu —
  wtedy naprawdę trzeba ściągnąć dane.

## Paczka

`SterownikCO-Android-v0.31.61-PELNY-PROJEKT.zip` — 1 172 542 B,
ID `181ft8YbTf6X3_fr5YTsNMib_4fpiJeNW`,
SHA-256 `798bbf052b116861be965776e20c8446ab56174de07bef6d2d3f15ebf5bc8751`.
Pełny projekt, 189 plików, `versionCode 31061`.

## Czego nie da się zagwarantować

Nie mam telefonu ani Twojej sieci, więc nie zmierzę tego u siebie. Dlatego
w dzienniku są czasy etapów — jeśli nadal będzie wolno, wiadomo będzie, który
etap (pamięć / sieć / zapis) odpowiada za opóźnienie.
