# 249 — Porównanie wykresów: Firebase ↔ plik lokalny (drugi plik HTML)

Data: 2026-10-31 · **wersja narzędzia: v4** (bez okna logowania + tabela największych różnic)
Plik: `porownanie-wykresow.html` (w korzeniu repo)
**51 520 B · SHA-256 `fd9358b0b8be481eef788e894f2156ecff5b7f42ca30e31aaa21d9d391cfd6b7`**

| wersja | Dysk | uwaga |
|---|---|---|
| **v4 (ta)** | **patrz `LINKI-2026-10-29.md`** | **bez okna logowania** — hasło pytane raz na komputer, potem zero pytań |
| v3 | `1EuJkotLEVpuseQpeXKrvzos5315RRgYu` | + tabela „Największe różnice” |
| v2 | `1mhffsci_QIm8qc1YHr-Aw2zoyzsM_rA8` | auto-logowanie, sama pobiera ostatnią dobę |
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

## 0a. v3 — tabela „Największe różnice (B − A)” (31.10)

Po pierwszych wykresach z prawdziwych danych było widać skoki w trybie RÓŻNICA B−A.
Żeby dało się je rozpoznać bez zgadywania, pod statystykami jest teraz tabela
**15 największych różnic** w wybranych kanałach. Dla każdej pary pokazuje:

| kolumna | co mówi |
|---|---|
| czas | kiedy (czas lokalny, z próbki B) |
| kanał | który sygnał |
| A (chmura) / B (plik) | **surowe wartości z obu źródeł** — nie różnica |
| B − A | różnica |
| przesunięcie pary | ile sekund było między próbką B i próbką A, z którą ją sparowano |

**Jak to czytać (najważniejsze w całym narzędziu):**

- **duże przesunięcie + kanał skokowy** (pompa 0/100, klapa, syberek) → w oknie
  tolerancji zmienił się stan. Porównujesz migawkę sprzed zmiany z próbką po zmianie,
  stąd skok ±100. To **nie błąd danych**. Zmniejsz tolerancję (np. do 10 s).
- **małe przesunięcie + duża różnica** → naprawdę różne wartości w tym samym czasie.
  Wtedy warto patrzeć na czujnik albo na to, czy próbka B nie jest zerem / kodem błędu.

## 0b. Skąd biorą się skoki na wykresie różnic (31.10)

Z pierwszego uruchomienia na danych użytkownika (24.10 19:06 — 31.10 07:25):

| sygnał | średnia B−A | max \|B−A\| | co to znaczy |
|---|---|---|---|
| Zewn. | −0,0 °C | 1,0 °C | zgoda — mapowanie kolumn jest poprawne |
| Bojler | −0,0 °C | 2,3 °C | zgoda |
| Piec | +0,1 °C | 12,9 °C | pojedyncze skoki |
| Piec śr. | +0,1 °C | 46,0 °C | pojedyncze skoki (średnia po restarcie) |

Średnia ≈ 0 przy każdym kanale dowodzi, że **kolumny nie są przesunięte** — gdyby
były, średnia byłaby ogromna. Skoki to sprawa pojedynczych próbek:

1. **skokowe sygnały** (pompa, klapa, syberek) — porównanie migawki z próbką minutową,
   patrz 0a;
2. **momenty restartu centrali** — średnia `t_ogrz_sr` startuje od nowa, serwa wracają
   do pozycji wyjściowej; w chmurze i w archiwum widać to w tej samej sekundzie;
3. **wilgotność** — skoki ±100 wymagają jeszcze potwierdzenia w pliku; tabela
   z 0a pokaże wprost, czy B ma tam 0, 99 czy 100.

## 0c. v4 — nie ma czego wypełniać (31.10, na polecenie użytkownika)

**Było:** rozwinięta sekcja „Konto, hasło i klucz API” z e-mailem, polem hasła,
check boxem i przyciskiem „Wyloguj”. Wyglądało jak formularz do wypełniania.

**Jest:**

- **konto i klucz API są w pliku, ale niewidoczne** — nie ma czego uzupełniać,
- plik **wchodzi sam**: token z pamięci → sesja zapamiętana przez panel → odświeżenie
  bez hasła → hasło zapamiętane,
- **pole hasła pojawia się tylko wtedy**, gdy nie ma z czego się zalogować —
  czyli **raz na komputer**. Po pierwszym udanym logowaniu chowa się i nie wraca,
- po zalogowaniu od razu pobiera ostatnią dobę i rysuje wykresy.

### Chcesz zero pytań także przy pierwszym uruchomieniu?

W pliku jest jedna linia (207):

```javascript
const FB_HASLO_WBUDOWANE = "";
```

Wpisz hasło w cudzysłów, zapisz plik — i wtedy **nigdy** o nic nie zapyta, nawet
po wyczyszczeniu przeglądarki, na każdym komputerze.

> **Uwaga, mówię wprost:** plik z wpisanym hasłem jest **poufny** — kto go ma, ma dostęp
> do bazy pieca. Nie publikuj takiej kopii na publicznym linku (Dysk/GitHub).
> Wersja podawana na Dysku ma tę linię **pustą** i dlatego jest bezpieczna.

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
