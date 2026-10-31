# Analiza strat nocnych — 2026-10-29

Co jest w katalogu:

| plik | co zawiera |
|---|---|
| `straty_nocne.py` | cała analiza — czyta log centrali, liczy model 0.31.52, wypisuje tabele |
| `wynik.txt` | wynik uruchomienia (do porównania, gdy dojdą nowe noce) |
| `229-STRATY-NOCNE.md` | opis po polsku: co wyszło i dlaczego |

## Jak uruchomić

Wrzuć logi centrali (`piec_log*.txt`) do jednego katalogu i:

```
python3 straty_nocne.py <katalog_z_logami>
```

Skrypt nie potrzebuje internetu — zachmurzenie nocne jest wpisane w kodzie
(`CLOUD`), bo Open-Meteo nie jest dostępny z każdego miejsca. Żeby dodać nowe
noce, dopisz je do słownika `CLOUD` i uruchom ponownie.

## Co liczy

- **zbiornik na dachu** (`panel`, BUS_3): stratę przez 11 godzin nocy,
  porównanie z modelem, podział strat na powietrze i niebo,
- **bojler pieca** (`bojler`, BUS_2): pokazuje, że pogoda go nie tłumaczy,
- wpływ jakości danych o chmurzeniu na błąd.

## Wynik w trzech liczbach

- błąd modelu: **0,84 K średnio, 0,41 K mediana** (11 nocy)
- udział nieba w stratach: **62%** w tym okresie, do 86% przy czystej nocy
- bojler: korelacja z chmurzeniem **−0,09** — pogoda nie ma z nim nic wspólnego
