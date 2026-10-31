# 249 — Porównanie wykresów: Firebase ↔ plik lokalny (drugi plik HTML)

Data: 2026-10-31
Plik: `porownanie-wykresow.html` (w korzeniu repo)
Dysk: **`1f0JiB2-JEYFxYhwoPxETh0wdigfJEhdf`** · 40 289 B ·
SHA-256 `aa3752b157e0c0b343b17e24a6eb8e18c67629ba68e783359cf0af81ca7df58c`

---

## 1. Po co to jest

`Piec.html` to panel centrali — pokazuje wykresy z **jednego** źródła, wybranego w danej chwili.
Ten plik jest **drugim, osobnym narzędziem**: ładuje **dwa źródła naraz** i pozwala je zestawić.

| | źródło A | źródło B |
|---|---|---|
| skąd | Firebase RTDB `/piec/telemetry/1m/v1/RRRR/MM/DD` | plik z dysku |
| co | próbki 1-minutowe z chmury | `hist_short.csv` (24 h, 14 kol.) |
| | | `hist_long.csv` (archiwum, 35+ kol.) |
| | | eksport JSON z konsoli Firebase |

## 2. Trzy tryby widoku

| tryb | co pokazuje |
|---|---|
| **OBOK SIEBIE** | dwa wykresy, **identyczna skala Y i identyczna os czasu** — różnicę widać bez mierzenia |
| **NAKŁADKA** | jeden wykres: A ciągłą, B przerywaną |
| **RÓŻNICA B−A** | jeden wykres wartości `B − A` na sparowanych próbkach, z linią zera |

## 3. Co liczy pod spodem

- **parowanie próbek** po czasie z tolerancją (domyślnie ±90 s, regulowane),
- **średnia i maksymalna różnica** `B − A` na kanał,
- **statystyki** min / średnia / max / liczba próbek dla każdego źródła,
- **brakujące minuty** — ile minut jest tylko w A, a ile tylko w B, z pierwszymi sześcioma
  (to bezpośrednio pokazuje luki, o których była mowa przy wykresach),
- **przerwy > 6 min** w każdym źródle.

## 4. Kanały

13 sygnałów + pompa:

`Zewn.` · `Bojler` · `Piec` · `Piec śr.` · `Powrót` · `Panel` · `Pokój` · `Trociny` ·
`Wilg.` · `Ciśn.` · `Dym` · `Klapa %` · `Syberek %` · `Pompa %`

Domyślnie włączone: Piec, Bojler, Zewn.

## 5. Zgodność z tym, co robi firmware (sprawdzone w kodzie)

Rekord w RTDB (`telemetryAppendPathForSample`, v3.57.x):

```json
{ "seq":…, "ts":<unix s>, "mono":…,
  "a":[tZewn,tBojler,tOgrz,tOgrzSr,tPowrot,tPanel,tPokoj,tTrociny,rh,pressure,dym],
  "k":klapaDeg, "s":syberkoDeg, "state":…, "q":…, "sim":…, "age":[…] }
```

- `a[0..9]` to **dziesiąte części** (°C ×10, % ×10, hPa ×10) → dzielone przez 10,
- `a[10]` (dym) — surowe, bez skalowania,
- `k` 0–180° → % (×100/180), `s` 0–90° → % (×100/90),
- `state` bit 0 → pompa 0/100 %,
- `q` — 2 bity jakości na kanał; `≥2` = odrzucone (ta sama reguła co w `Piec.html`),
- kody błędów DS18B20 (`≤ −1200`, `≥ 15000`) odrzucane.

CSV: `hist_short.csv` — 14 kolumn, czas w kolumnie 13 (unix s);
`hist_long.csv` — 35 kolumn (+ opcjonalna 36. = pompa), czas w kolumnie 34.
Kolumnę pompy i pozycje temperatur odwzorowano 1:1 z parserów `Piec.html`,
więc plik pobrany z panelu i rekord z chmury opisują to samo.

## 6. Bezpieczeństwo

- hasło **nie jest nigdzie zapisywane** — trafia tylko do `identitytoolkit` Google,
- token trzymany **w pamięci karty**, nie w `localStorage`,
- poza zapytaniami do Google (logowanie + odczyt RTDB) nic nie wychodzi z przeglądarki,
- brak zewnętrznych bibliotek i CDN — plik działa offline (poza samym pobieraniem danych).

## 7. Jak użyć

1. Pobierz `porownanie-wykresow.html` i otwórz w przeglądarce (dwuklik).
2. **A:** wpisz hasło do konta `piec_co_boot@akwarium.local`, wybierz zakres (6 h / 24 h / 7 dni / 30 dni), „Pobierz z Firebase".
3. **B:** przeciągnij plik `hist_short.csv` albo `hist_long.csv` (albo JSON z Firebase).
4. Wybierz sygnały i tryb widoku. Na dole statystyki i lista brakujących minut.
5. „Zapisz CSV" — zestawienie par (A, B, różnica dla każdego kanału).
   „Zapisz PNG" — obraz wykresów (w trybie obok siebie oba sklejone pionowo).

## 8. Ograniczenia — mówię wprost

- **Nie testowałem tego w przeglądarce** — sandbox nie ma przeglądarki ani dostępu
  do `firebaseio`. Sprawdzone: składnia JS (`node --check`), wszystkie trzy parsery
  na danych wzorcowych, rysowanie w trzech trybach na zaślepce DOM (bez wyjątków).
  Pierwsze uruchomienie u Ciebie to prawdziwy test.
- Logowanie wymaga hasła — nie da się go wpisać za Ciebie.
- Odczyt RTDB zależy od reguł (`auth != null`) — ten samym mechanizmem co `Piec.html`.
- Bardzo duże zakresy (30 dni ≈ 43 200 próbek) rysują się wolniej; dane są pełne,
  rysowanie idzie po wszystkich punktach.
