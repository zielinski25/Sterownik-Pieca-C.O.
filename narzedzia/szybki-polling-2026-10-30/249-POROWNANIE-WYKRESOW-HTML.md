# 249 — Porównanie wykresów: Firebase ↔ plik lokalny (drugi plik HTML)

Data: 2026-10-31 · **wersja narzędzia: v2** (poprawka: wchodzi już zalogowany)
Plik: `porownanie-wykresow.html` (w korzeniu repo)
**48 613 B · 1 053 linie · SHA-256 `5a1a87909806d068cf07ce4ac155f90ad3cb3ee9203e6452cccbb46315602abf`**

| wersja | Dysk | uwaga |
|---|---|---|
| **v2 (ta)** | **`1mhffsci_QIm8qc1YHr-Aw2zoyzsM_rA8`** | auto-logowanie, sama pobiera ostatnią dobę |
| v1 | `1f0JiB2-JEYFxYhwoPxETh0wdigfJEhdf` (nazwa zmieniona na `…-v1-BEZ-AUTOLOGOWANIA.html`) | pytała o hasło za każdym otwarciem — **nie używać** |

---

## 0. Poprawka v2 — „ma tam być już wszystko zalogowane" (31.10)

**Co było źle w v1:** przy każdym otwarciu plik pytał o hasło od zera i nie wiedział
nic o sesji, którą panel centrali (`Piec.html`) już dawno zapisał w przeglądarce.
Czyli: panel był zalogowany, a narzędzie obok — nie.

**Co jest teraz (kolejność, w jakiej plik próbuje wejść):**

1. token, który już ma w pamięci karty (ważny? używa),
2. token zapisany w przeglądarce przez **panel** albo przez to narzędzie,
3. **odświeżenie sesji** tokenem odświeżania — bez hasła (endpoint `securetoken.googleapis.com`),
4. hasło zapamiętane przy pierwszym logowaniu,
5. dopiero teraz: pokazuje pole hasła.

Po wejściu **od razu pobiera ostatnią dobę** i rysuje wykresy — nie trzeba klikać
„Pobierz z Firebase".

**Wspólne klucze z panelem** (celowo identyczne, żeby sesja była jedna):

| klucz | co trzyma |
|---|---|
| `piec_fb_email` | e-mail konta pieca |
| `piec_fb_password` | hasło, **tylko jeśli zaznaczysz „zapamiętaj hasło"** |
| `piec_fb_ref_token` | token odświeżania (to on pozwala wchodzić bez hasła) |
| `piec_fb_token` | token ID (pamięć karty, nie dysk) |
| `piec_fb_api_key` | klucz API |

Dzięki temu: logujesz się **raz** (w panelu albo tutaj) → oba pliki wchodzą same.
Przycisk **„Wyloguj / zapomnij hasło"** kasuje hasło i tokeny.

Jeśli nic nie ma zapamiętanego, plik **sam rozwija** sekcję „Konto, hasło i klucz API",
więc widać, gdzie wpisać hasło. Po udanym logowaniu zwija ją z powrotem.

> Uczciwie: to działa, gdy oba pliki otwierasz **tą samą przeglądarką**. Sesja
> w przeglądarce jest przypisana do adresu, z którego plik pochodzi. Otwieranie
> panelu z sieci (`http://192.168.…`) a narzędzia z pliku na dysku to dwa różne
> miejsca — wtedy zadziała punkt 4 (hasło wpisane raz w tym pliku).

---

## 1. Po co to jest

`Piec.html` to panel centrali — pokazuje wykresy z **jednego** źródła.
Ten plik jest **drugim, osobnym narzędziem**: ładuje **dwa źródła naraz** i zestawia je.

| | źródło A | źródło B |
|---|---|---|
| skąd | Firebase RTDB `/piec/telemetry/1m/v1/RRRR/MM/DD` | plik z dysku |
| co | próbki 1-minutowe z chmury | `hist_short.csv` (24 h, 14 kol.) |
| | | `hist_long.csv` (archiwum, 35+ kol.) |
| | | eksport JSON z konsoli Firebase |

## 2. Trzy tryby widoku

| tryb | co pokazuje |
|---|---|
| **OBOK SIEBIE** | dwa wykresy, **identyczna skala Y i identyczna os czasu** |
| **NAKŁADKA** | jeden wykres: A ciągłą, B przerywaną |
| **RÓŻNICA B−A** | wartości `B − A` na sparowanych próbkach, z linią zera |

## 3. Co liczy pod spodem

- **parowanie próbek** po czasie z tolerancją (domyślnie ±90 s, regulowane),
- **średnia i maksymalna różnica** `B − A` na kanał,
- **statystyki** min / średnia / max / liczba próbek dla każdego źródła,
- **brakujące minuty** — ile tylko w A, ile tylko w B, z pierwszymi sześcioma,
- **przerwy > 6 min** w każdym źródle.

## 4. Kanały

`Zewn.` · `Bojler` · `Piec` · `Piec śr.` · `Powrót` · `Panel` · `Pokój` · `Trociny` ·
`Wilg.` · `Ciśn.` · `Dym` · `Klapa %` · `Syberek %` · `Pompa %`
(Domyślnie włączone: Piec, Bojler, Zewn.)

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
- `q` — 2 bity jakości na kanał; `≥2` = odrzucone (reguła jak w `Piec.html`),
- kody błędów DS18B20 (`≤ −1200`, `≥ 15000`) odrzucane.

CSV: `hist_short.csv` — 14 kolumn, czas w kolumnie 13 (unix s);
`hist_long.csv` — 35 kolumn (+ opcjonalna 36. = pompa), czas w kolumnie 34.

## 6. Bezpieczeństwo — jak jest naprawdę

- Hasło **nie jest wkodowane w plik** — jest puste, wpisujesz swoje.
- Hasło i tokeny leżą **tylko w pamięci przeglądarki na Twoim komputerze**
  (`localStorage`), dokładnie tak jak w panelu. Nie idą nigdzie poza Google.
- Jeśli nie chcesz zapamiętywać hasła — odznacz „zapamiętaj" przy logowaniu;
  zostanie sam token odświeżania (ważny do unieważnienia sesji w konsoli Firebase).
- Brak CDN i bibliotek zewnętrznych — poza zapytaniami do Google plik działa offline.

## 7. Jak użyć

1. Pobierz plik, otwórz **dwuklikiem** w przeglądarce (Chrome/Edge).
2. Jeśli wcześniej logowałeś się w panelu z „zapamiętaj" — **jest już zalogowany**
   i sam wczytuje ostatnią dobę. Jeśli nie — rozwiń „Konto, hasło i klucz API",
   wpisz hasło, zaznacz „zapamiętaj" (domyślnie zaznaczone).
3. **B:** przeciągnij `hist_short.csv` / `hist_long.csv` (albo JSON z Firebase).
4. Wybierz sygnały i tryb widoku. Na dole statystyki i brakujące minuty.
5. „Zapisz CSV" — pary (A, B, różnica). „Zapisz PNG" — obraz wykresów.

## 8. Co sprawdzono i czego nie

**Sprawdzone 31.10 — w prawdziwym DOM (`jsdom`), nie na mojej zaślepce:
`WYNIK jsdom: 34 OK, 0 FAIL`.** Test leży w repo: `narzedzia/testy-html/`
(`test-porownanie-jsdom.js` + `README.md` — jak uruchomić u siebie, jedna komenda).

| grupa | sprawdzenia | wynik |
|---|---|---|
| struktura | wszystkie 37 elementów, których szuka skrypt | OK |
| start bez sesji | brak zapytań, ostrzeżenie „wpisz hasło", konto rozwinięte | OK |
| start z sesją | odświeżenie bez hasła, pobranie bazy, rysowanie, statystyki, tabela | OK |
| tryby | kliknięcie 3 przycisków, poprawny tryb po kliknięciu | OK |
| wylogowanie | czyści token i hasło, rozwija konto | OK |
| token martwy | odświeżenie → hasło, dane pobrane | OK |
| złe hasło | czytelny komunikat, brak danych | OK |

Dodatkowo wcześniej: składnia JS (`node --check`), trzy parsery na danych
wzorcowych, rysowanie w trzech trybach, 22 sprawdzenia logiki bez DOM.

**Czego to dalej nie dowodzi:**

- **Prawdziwe rysowanie** — `jsdom` nie ma canvasa; test liczy wywołania rysowania.
  Krzywy wykres (zła geometria) wyjdzie tylko u Ciebie na ekranie.
- **Prawdziwe Google** — sieć w teście jest zaślepiona. Złe hasło albo zmienione
  reguły bazy wyjdą dopiero w rzeczywistym połączeniu.
- **Wygląd (CSS)** — tego `jsdom` nie liczy.

Pierwsze otwarcie u Ciebie to więc wciąż pierwszy prawdziwy kontakt z siecią.
Jak coś nie zagra — F12 → Console i podeślij tekst.

**Znane ograniczenia samego narzędzia:**

- Reguły bazy wymagają `auth != null` → **logowanie jest obowiązkowe**, nie ma
  czytania anonimowego.
- Podgląd w aplikacji (piaskownica) bywa bez sieci i bez pamięci przeglądarki —
  tam logowanie nie zadziała. Potrzebna jest prawdziwa przeglądarka.
