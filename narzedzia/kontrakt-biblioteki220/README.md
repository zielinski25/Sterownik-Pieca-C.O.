# Etap 220 — kontrakt z prawdziwą biblioteką

Cały nasz nowy kod (trasa pobrania, bilety, zapinka pliku) był dotąd kompilowany
**tylko przeciwko naszym własnym zaślepkom** — modelowi serwera HTTP, który sami
napisaliśmy. Jeśli zaślepka ma inną sygnaturę niż prawdziwa biblioteka, to że
testy są zielone nic nie znaczy: u Ciebie plik się nie skompiluje.

Ten etap bierze **prawdziwe nagłówki biblioteki** (przypięte SHA-256 z etapów
196–215) i sprawdza nasz kod na nich. To jest ostatnia kontrola przed
PlatformIO, której nie da się zrobić „na modelu”.

## 7 kontroli

| Kod | Co sprawdza | Wynik |
|---|---|---|
| Z01 | 4 nagłówki biblioteki są te same (SHA-256 zgodne z etapami 196–215) | 4 pliki |
| Z02 | pola klasy bazowej, które ustawiamy (`_code`, `_contentType`, `_contentLength`, `_sendContentLength`, `_chunked`), są `protected`; konstruktor publiczny; destruktor wirtualny | 5 pól |
| Z03 | nasze `override` pasują do prawdziwych metod wirtualnych; nie nadpisujemy `final` | 3 nadpisania |
| Z04 | **każde** wywołanie metody biblioteki ma dokładnie jednego kandydata (rozstrzyganie przeciążeń) | 19 wywołań |
| Z05 | w nagłówkach firmware nie ma nazw istniejących tylko w zaślepkach (`std_str`, `jakoStrumien`, …) | 14 zakazanych nazw |
| Z06 | wywołania `historia193::Calls` istnieją w prawdziwym `CheckedHistoryIO.h` | open, stat, read, close, error |
| Z07 | nowe symbole (`HTTP_ANY`, `AsyncURIMatcher::exact`, `on(matcher, …)`) są w tej samej bibliotece, na której bazowy plik już działa | OK |

### Co konkretnie wyszło z Z04

Silnik rozstrzygania przeciążeń liczy dla każdego wywołania rangę konwersji
(dokładne dopasowanie = 0, konwersja całkowita = 1, konwersja użytkownika = 2,
niedozwolona = odrzucony) i wybiera jednoznacznie najlepszego kandydata.

| Wywołanie (nasz kod) | Prawdziwy kandydat w bibliotece |
|---|---|
| `request->method()` | `WebRequestMethod method() const` |
| `request->getHeader("Authorization")` | `const AsyncWebHeader *getHeader(const char*) const` |
| `request->hasParam("k")` | `bool hasParam(const char*, bool=false, bool=false) const` |
| `request->getParam("k")` | `const AsyncWebParameter *getParam(const char*, bool=false, bool=false) const` |
| `odp->addHeader("Content-Disposition", …)` | `bool addHeader(const char*, const char*, bool=true)` |
| `request->send(odp)` | `void send(AsyncWebServerResponse*)` |
| `request->beginResponse(200, "text/plain", String(bilet))` | `AsyncWebServerResponse *beginResponse(int, const String&, const String&, AwsTemplateProcessor=nullptr)` |

Żadne wywołanie nie jest niejednoznaczne i żadne nie jest odrzucone — czyli
kod się skompiluje i **znaczy to, co myślimy**.

### Gdzie nasza zaślepka kłamie (15 z 19 wywołań)

Model ma inne sygnatury w 15 miejscach. Wszystkie różnice są nieszkodliwe, ale
trzy warto zapamiętać:

| Miejsce | Biblioteka | Nasz model | Czy groźne |
|---|---|---|---|
| `method()` | zwraca `WebRequestMethod` (enum) | zwraca `int` | nie — `HTTP_GET` to wartość tego samego enumu |
| `addHeader` | zwraca `bool`, przyjmuje `const char*` | `void`, przyjmuje `String` | nie — my nie używamy zwrotki; prawdziwy wariant przyjmuje nasze literały bez konwersji |
| `beginResponse(kod, "text/plain", String(...))` | wybiera `(int, const String&, const String&)`, czyli robi `String` z `"text/plain"` | model ma `(int, const char*, const String&)` | nie — ale to pokazuje, że model nie dowodzi, **który** wariant przeciążenia realnie się wybierze |

Poza tym model nie ma domyślnych parametrów (`post`, `file`, `replaceExisting`),
więc nie sprawdzałby np. `hasParam("k", true)`. Nasz kod ich nie używa — i to
właśnie ten etap potwierdza.

## 10 mutantów (wszystkie czerwone)

| Mutant | Co psuje | Czerwone |
|---|---|---|
| M1 | nasze `_fillBuffer` staje się `const` | Z03 |
| M2 | nadpisujemy `_respond` (w bibliotece `final`) | Z03 |
| M3 | w nagłówku firmware pojawia się `std_str()` z modelu | Z05 |
| M4 | `addHeader(1, 2)` — żadne przeciążenie tego nie przyjmie | Z04 |
| M5 | zapinka woła `io.openn()` | Z06 |
| M6 | w bibliotece `_code` staje się `private` | Z02 |
| M7 | podmieniony nagłówek biblioteki (inny SHA) | Z01 |
| M8 | biblioteka nie definiuje `HTTP_ANY` | Z07 |
| M9 | nagłówek firmware wciąga plik zaślepki | Z05 |
| M10 | biblioteka nie ma `AsyncURIMatcher::exact` | Z07 |

Mutanty M2, M6, M8, M10 to dokładnie te błędy, które **przeszłyby przez
wszystkie testy hostowe** i wywaliły się dopiero u Ciebie w PlatformIO.

## Wynik

```
PASS kontrakt220 7 kontroli, 19 wywolan biblioteki, model rozny w 15
PASS brama220 kontrol=7 mutantow=10 wywolan=19
```

## Czego ten etap NIE dowodzi

- że PlatformIO skompiluje plik — nie mamy kompilatora ESP (to Ty),
- że zachowanie w czasie działania jest poprawne — to robią etapy 211–219,
- że Twoja wersja biblioteki jest dokładnie ta z repozytorium ESP32Async
  (ref `c47917d`); jeśli masz inną wersję, `AsyncURIMatcher::exact` i `HTTP_ANY`
  nadal istnieją, bo korzysta z nich już bazowy plik,
- że inne biblioteki (Arduino core, FreeRTOS) są zgodne — `esp_random()`,
  `millis()`, `xSemaphoreTake()` są używane od dawna w bazowym pliku.

## Uruchomienie

```
python3 -B narzedzia/kontrakt-biblioteki220/run.py
```

## Polecenie dla użytkownika

**Bez zmian** — plik do wgrania to nadal v3.56.4 z etapu 218:

```
patch -p1 -i main_centrala-v3.55.8-to-v3.56.4.patch
cp narzedzia/od-przycisku216/HistoriaPobranie.h .
```

Potem PlatformIO. Po wgraniu: telefon → „⬇ HISTORIA CSV” → „Krótka (24 h)”
zapisuje `historia-krotka.csv`, „Długa (archiwum)” — `historia-dluga.csv`.
