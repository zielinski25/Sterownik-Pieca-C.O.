# STEROWNIK CO — ANDROID — MASTER PLAN

**Stan na:** 2026-10-02
**Aktualna wersja robocza:** `0.17.0-NATIVE-PREMIUM-VISUAL`
**Główny etap:** Dashboard 1.0 + Wykresy 1.0

## Legenda
- ✅ zrobione / potwierdzone
- 🟨 częściowo / do testu
- ⬜ do zrobienia
- ⚠️ otwarte / ryzyko

---

# 1. Cel aplikacji

Androidowa aplikacja STEROWNIK CO jest mobilnym klientem istniejącego sterownika ESP32. Android ma prezentować stan, historię i ustawienia, a logika sterowania pozostaje po stronie firmware.

Docelowe moduły:

- Dashboard
- Ustawienia
- Wykresy
- Pogoda
- Logi
- Terminal
- OTA

---

# 2. Fundamenty

## 2.1 Logowanie i sesja

- ✅ Firebase Authentication: e-mail + hasło przy pierwszym logowaniu.
- ✅ Hasło nie jest zapisywane.
- ✅ Refresh token jest przechowywany szyfrowanie przez Android Keystore.
- ✅ ID token jest automatycznie odnawiany.
- ✅ Chwilowy brak sieci/HTTP nie powinien kasować zapisanej sesji.
- ✅ Ręczne `WYLOGUJ` usuwa sesję.
- 🟨 Długoterminowy test po wielu godzinach / po restarcie telefonu nadal do wykonania na v0.10.16.
- ⚠️ Pojawienie się loginu w testowanym wcześniej v0.10.13 po kilku godzinach było związane ze starszą logiką sesji; od v0.10.15 logika została zaostrzona tak, aby błędy przejściowe nie czyściły sesji.

## 2.2 Środowisko

- ✅ package/applicationId: `pl.sterownikco.dev`
- ✅ Firebase DEV
- ✅ telefon testowy przez Wi-Fi
- ✅ AGP 9.4.x / Java 17
- ⚠️ `gradle-wrapper.jar` nadal warto dołączyć do finalnej paczki, aby projekt był w pełni odtwarzalny poza konkretnym środowiskiem Android Studio.

## 2.3 Bezpieczeństwo komend

- ✅ firmware ma istniejący mailbox `/piec/cmd` i ACK `/piec/ack`.
- ✅ firmware wymaga tokenu komendy i obsługuje `cmdId` oraz deduplikację retry.
- 🟨 Android v0.10.16 ma klienta komend oraz podstawowe menu.
- ⬜ zamknięcie Security Rules dla mobilnego zapisu.
- ⬜ testy retry/timeout/ACK w Androidzie.
- ⬜ oddzielenie uprawnień od samego tokenu DEV przed release.

---

# 3. Dashboard 1.0

## 3.1 Wzorzec referencyjny

Istniejący panel WWW/firmware ma 13 kafelków i osobną obsługę kliknięć. Android ma przenieść ten model, a nie tylko listę wartości.

Lista referencyjna:

1. Zewnętrzna
2. Piec C.O.
3. Bojler
4. Panel słon.
5. Pomieszczenie
6. Ciśnienie
7. Wilgotność
8. Pompa
9. Serwo
10. Mieszadło
11. Czujnik dymu
12. Wykresy
13. Data i czas

### 3.2 Status i dane

- ✅ odczyt rzeczywistego `/piec/status`
- ✅ Zewnętrzna
- ✅ Piec C.O.
- ✅ Bojler
- ✅ Panel słon.
- ✅ Pomieszczenie
- ✅ Ciśnienie
- ✅ Wilgotność
- ✅ Klapa/Syberek
- ✅ Pompa
- ✅ WiFi RSSI w warstwie statusowej
- ✅ tryb serwa
- ✅ alarm ogrzewania/panelu/dymu
- 🟨 nie wszystkie pola diagnostyczne są jeszcze osobnymi kafelkami na Androidzie

### 3.3 Interakcja kafelków

- ✅ od v0.10.16 wszystkie 13 pozycji są utworzone jako osobne kafelki
- ✅ każdy kafelek operacyjny ma własny handler
- ✅ kafelki mają `pressed` feedback (scale + border/background)
- ✅ animowane ikony reagują na stan tam, gdzie ma to sens
- 🟨 v0.10.17 — poprawka builda po błędach Kotlin; test Dashboardu po udanym buildzie

### 3.4 Menu kafelków — mapowanie do WWW/ESP

- ✅ Zewnętrzna → menu czujnika/symulacji
- ✅ Bojler → menu czujnika/symulacji
- ✅ Pomieszczenie → menu czujnika/symulacji
- ✅ Ciśnienie → menu czujnika/symulacji
- ✅ Wilgotność → menu czujnika/symulacji
- ✅ Piec C.O. → próg przegrzania, reset/wyciszenie, symulacja
- ✅ Panel słon. → próg przegrzania, reset/wyciszenie, symulacja
- ✅ Pompa → strategia + WŁ./WYŁ./Auto + czasy
- ✅ Serwo → Auto/Ręczny/Bezpieczna + Klapa/Syberek
- ✅ Mieszadło → Włączone + WŁ./WYŁ./Auto
- ✅ Czujnik dymu → odczyt + wyciszenie + próg + test/symulacja
- ✅ Data i czas → informacja o czasie telefonu
- ✅ Wykresy → przejście do historii

### 3.5 Animacje

- ✅ płomień: niezależny ruch warstw
- ✅ bojler: wizualne napełnienie + efekt bąbelków
- ✅ panel słoneczny: energia/słońce
- ✅ ciśnienie: wskazówka
- ✅ wilgotność: fala
- ✅ pompa: obrót łopatek tylko gdy aktywna
- ✅ serwo: pozycja mechaniczna zależna od procentu
- ✅ mieszadło: obrót gdy aktywne
- ✅ dym: falowanie
- ✅ wykresy: animowane słupki
- ✅ zegar: sekundnik
- ✅ alarm: mocniejsze obramowanie/puls
- 🟨 test wizualny na realnym telefonie v0.10.16

---

# 4. Wykresy 1.0

## 4.1 Działa

- ✅ 24 H / 7 DNI / 30 DNI
- ✅ temperatura Zewn./Bojler/Ogrzewanie
- ✅ Klapa/Syberek
- ✅ GAP > 150 s
- ✅ deduplikacja `ts`
- ✅ jakość `q`, `sim`, `age` w diagnostyce
- ✅ filtr prezentacyjny anomalii
- ✅ lżejsze rysowanie długiej historii
- ✅ crosshair + tooltip
- ✅ pan
- ✅ pinch zoom
- ✅ zoom +/-/reset

## 4.2 Do poprawy

- 🟨 UX gestów jest lepszy niż początkowo, ale wymaga finalnego testu użytkownika
- 🟨 zaznaczenie zakresu musi być zawsze jednoznacznie widoczne
- 🟨 dalsze dopracowanie osi czasu i skali przy 7/30 dniach
- ⬜ decyzja, czy custom Canvas zostaje, czy robimy benchmark MPAndroidChart/Vico

---

# 5. Serwa

- ✅ `k` i `s` traktowane jako kąty telemetryczne
- ✅ Klapa `0..180° -> 0..100%`
- ✅ Syberek `0..90° -> 0..100%`
- ✅ ręczne komendy firmware przyjmują procenty
- 🟨 Android nadal powinien wyraźnie rozdzielić `actual position` i `target/command`

---

# 6. Pozostałe moduły

## Ustawienia

- 🟨 część funkcji jest już osiągalna z menu kafelków
- ⬜ pełna zakładka Ustawienia 1:1 z WWW
- ⬜ grupowanie nastaw
- ⬜ walidacja wartości
- ⬜ bezpieczeństwo zapisu

## Pogoda

- ⬜ Open-Meteo
- ⬜ bieżące dane
- ⬜ prognoza
- ⬜ wykres
- ⬜ cache/offline

## Logi

- ⬜ logi sterownika
- ⬜ filtrowanie
- ⬜ diagnostyka

## Terminal

- ⬜ tryb serwisowy
- ⬜ komunikacja i ograniczenia uprawnień

## OTA

- ⬜ bezpieczny update
- ⬜ potwierdzenie wersji
- ⬜ rollback/procedura awaryjna

---

# 7. Cache / offline

- ⬜ lokalny cache historii
- ⬜ merge po `record_id`
- ⬜ UI offline
- ⬜ retencja

---

# 8. Kontrakt telemetrii — zamrożony

Wire:

`seq, ts, mono, a[11], k, s, state, q, sim, age[11]`

Tożsamość rekordu:

`<16 hex sessionNonce>-<8 hex sampleSeq>`

`ts=0` powinno trafiać do `unknown`, a nie do przypadkowego dnia.

Dane Firebase pozostają źródłem RAW. Anomalia prezentacyjna nie kasuje rekordu.

---

# 9. Historia wersji Android

- ✅ v0.10.5 — realny Firebase Auth + persistent session
- ✅ v0.10.6 — realna historia + pierwsze wykresy
- ✅ v0.10.7 — poprawka konstruktora TelemetryChartView
- ✅ v0.10.8 — poprawka crasha `UnknownFormatConversionException` + jednostki serw
- ✅ v0.10.9 — q/sim/age, GAP, dedup i diagnostyka
- ✅ v0.10.10 — stabilność/filtracja/decymacja
- 🟨 v0.10.11 — korekta czytelności
- 🟨 v0.10.13 — pierwszy UX wykresów
- 🟨 v0.10.14 — LIVE Dashboard + UX interakcji
- ✅ v0.10.15 — trwałość sesji / refresh token
- 🟨 v0.10.16 — pełny Dashboard 13 kafelków + menu + animacje; **do testu użytkownika**

---

# 10. Najbliższy etap

**Najpierw test v0.10.16 Dashboardu.**

Nie dodajemy jeszcze kolejnych „ładnych” elementów na ślepo. Sprawdzamy:

- czy wszystkie 13 kafelków są obecne,
- czy każdy otwiera właściwe menu,
- czy animacje nie spowalniają przewijania,
- czy komunikaty i stany LIVE są czytelne,
- czy komendy nie wykonują się podwójnie,
- czy ACK działa,
- czy po kilku godzinach sesja nadal pozostaje aktywna.

Dopiero po tym przechodzimy do pełnej zakładki Ustawienia i dalszego dopracowania wykresów.

---

# 11. Historia planu — 2026-10-02

- ✅ porównano Android Dashboard z rzeczywistym panelem WWW/firmware
- ✅ potwierdzono 13 kafelków i osobne menu/handlery
- ✅ ustalono zasadę: Android Dashboard ma odwzorowywać funkcjonalnie WWW, a nie tylko prezentować statyczne wartości
- ✅ przygotowano v0.10.16 jako pierwszy pełny krok w tym kierunku
- 🟨 v0.10.16 oczekuje na test telefonu

## Referencja WWW/ESP

Kod panelu WWW zawiera listę 13 kafelków i mapę `KLIK_HANDLERY`; jest to referencja funkcjonalna dla Androida.

## 2026-10-02 — v0.10.18 — FIX BUILD po błędach v0.10.17

- [x] Zidentyfikowano 4 błędy Kotlin w `MainActivity.kt` przy `SeekBar`: parametr funkcji `max: Float` kolidował z właściwością `SeekBar.max`.
- [x] Poprawiono oba miejsca na jawne `it.max = ...` / `it.progress = ...`.
- [ ] Build Android Studio — do potwierdzenia po otwarciu v0.10.18.
- [ ] Uruchomienie na telefonie po Wi-Fi — do potwierdzenia.
- [ ] Test pełnego Dashboardu 13 kafelków — po udanym buildzie.


## 2026-10-02 — v0.11.0-NATIVE-PRO

- [x] cofnięto podejście WebView/wrapper WWW,
- [x] Dashboard jest natywny, z 13 kafelkami i natywnymi animacjami,
- [x] każde menu kafelka jest otwierane natywnie,
- [x] Wykresy otrzymały natywny silnik z wieloma seriami, trybami i interakcjami,
- [x] Firebase Auth/REST pozostaje poza warstwą UI,
- [x] brak HTML/WebView w aplikacji.
- [ ] build Android Studio,
- [ ] test na telefonie po Wi-Fi,
- [ ] audyt wizualny Dashboardu,
- [ ] audyt wizualny Wykresów,
- [ ] dopracowanie Ustawień/Pogody/Logów/Terminala/OTA.


## 2026-10-02 — v0.11.1-NATIVE-PRO-BUILD-FIX

- [x] usunięto błędy składniowe powodujące lawinę błędów `MainActivity.kt`,
- [x] poprawiono błędy typów w `DashboardTileView.kt`,
- [x] poprawiono błąd nullable `Double?` w `ProfessionalTelemetryChartView.kt`,
- [x] ponownie sprawdzono składnię wszystkich plików Kotlin (`kotlinc` — brak błędów składni),
- [ ] pełny `Build → Make Project` w Android Studio — do potwierdzenia na PC,
- [ ] uruchomienie na telefonie po Wi-Fi,
- [ ] audyt wizualny Dashboardu i Wykresów po udanym buildzie.


## 2026-10-02 — v0.11.2-NATIVE-PRO-BUILD-FIX2

- [x] naprawiono kolejne błędy `Long` → `Int` w `MainActivity.kt` wskazane przez Android Studio,
- [x] dodano brakujący import `DecelerateInterpolator`,
- [x] sprawdzono pozostałe 8-znakowe literały kolorów w kodzie MainActivity,
- [ ] pełny `Build → Make Project` w Android Studio,
- [ ] uruchomienie na telefonie po Wi-Fi.

## 2026-10-02 — v0.11.3-NATIVE-PRO-UI

- [x] usunięto widoczne w aplikacji oznaczenia „NATIVE PRO” z nagłówka i zastąpiono je zwykłym interfejsem produktu,
- [x] zmniejszono wysokość nagłówka i poprawiono proporcje hero,
- [x] dodano bezpieczne marginesy systemowe dla górnego i dolnego obszaru aplikacji,
- [x] dolna nawigacja ma aktywny stan i pełny obszar dotykowy,
- [x] hero Dashboardu jest aktualizowany rzeczywistymi wartościami temperatury/trybu/pompy/alarmu,
- [x] Trend LIVE otrzymuje próbki z rzeczywistego statusu,
- [x] poprawiono etykietę trybu „NORMALIZOWANE”,
- [x] wykres respektuje przerwę GAP > 150 s i nie rysuje sztucznego połączenia przez lukę,
- [x] poprawiono metadane projektu: `rootProject.name`, `versionCode`, `versionName`,
- [ ] pełny Build Android Studio,
- [ ] test na realnym telefonie po Wi‑Fi,
- [ ] test kompaktowy/duży ekran,
- [ ] dalsza rozbudowa ekranów Pogoda/Logi/Terminal/OTA.

## 2026-10-02 — v0.11.3-NATIVE-PRO-UI — PACZKA TESTOWA

- [x] przygotowano pełną paczkę ZIP `sterownik-co-android_v0.11.3-NATIVE-PRO-UI.zip`,
- [x] test archiwum ZIP `unzip -t` — OK,
- [x] audyt statyczny: brak WebView / `android.webkit` / HTML jako UI / błędnego `android:selected`,
- [x] brak plików `.bak` w projekcie,
- [x] README startowe zawiera instrukcję Windows + Android Studio + telefon po Wi-Fi,
- [x] usunięto niepotrzebne użycie operatora `?: run { return@scheduleAtFixedRate }` w pollingu i zastąpiono jawnym warunkiem `token == null`,
- [ ] `Build → Make Project` w Android Studio,
- [ ] `Run ▶` na telefonie po Wi-Fi,
- [ ] test login → Dashboard → Wykresy → zakres → restart aplikacji,
- [ ] test wizualny Compact/Medium/Expanded,
- [ ] dopiero po tym rozpocząć v0.12 Design System + Dashboard PRO.

## 2026-10-02 — v0.11.4-NATIVE-VISUAL-REWORK

- [x] przebudowano top app bar na kompaktowy natywny układ,
- [x] usunięto zawijanie tytułu i ciężar wizualny nagłówka,
- [x] dodano natywną ilustrację kotła w HERO,
- [x] przebudowano kafelki pomiarowe: aura + badge ikony + status pill + wartość monospaced,
- [x] przebudowano zakładkę Wykresy, aby kontrolki nie nachodziły i nie łamały tekstów,
- [x] przeniesiono tooltip wykresu pod dane zamiast na środek wykresu,
- [x] dodano subtelny glow linii wykresu,
- [x] zachowano GAP jako przerwę w przebiegu,
- [x] brak WebView/HTML jako UI,
- [ ] build Android Studio,
- [ ] test na telefonie po Wi-Fi,
- [ ] porównanie screenshotów z referencją WWW po uruchomieniu,
- [ ] finalny audyt Dashboardu/Wykresów,
- [ ] pełne Ustawienia/Pogoda/Logi/Terminal/OTA,
- [ ] adaptive layout Compact/Medium/Expanded.

## 2026-10-02 — v0.12.0-NATIVE-REBUILD — START

- ✅ potraktowano zrzuty telefonu z v0.11.4 jako negatywny audyt UX, nie jako bazę do dalszego kosmetycznego poprawiania,
- ✅ rozpoczęto pełną przebudowę hierarchii Dashboardu,
- ✅ rozpoczęto pełną przebudowę ekranów Wykresy i menu narzędzi,
- ✅ zachowano natywny Android bez WebView/HTML,
- ✅ zachowano kontrakt Firebase/Auth/telemetrii,
- ✅ rozdzielono wybór serii / widoku / narzędzi na natywne panele,
- ✅ pozycje serw otrzymały stałą skalę 0–100% i tryb schodkowy,
- ⬜ pełny Build Android Studio,
- ⬜ test na telefonie po Wi-Fi,
- ⬜ test wszystkich 13 kafelków,
- ⬜ test menu i ACK komend,
- ⬜ test wykresów 6 h / 24 h / 7 d / 30 d,
- ⬜ dopracowanie delta A−B, alarmów, analizy cykli i odchylenia od normy,
- ⬜ pełna implementacja Pogody / Logów / Terminala / OTA.


## 2026-10-02 — v0.13.0-NATIVE-PREMIUM — PEŁNA PRZEBUDOWA UI

- [x] usunięto duży techniczny nagłówek z pełnym e-mailem operatora z głównego widoku,
- [x] Dashboard przebudowany hierarchicznie: Stan instalacji → Najważniejsze → Sterowanie → System,
- [x] nowe kompaktowe kafelki z ikoną, stanem, wartością i paskiem aktywności,
- [x] nowa ilustracja HERO kotła z pierścieniem stanu i animowanym palnikiem,
- [x] Wykresy przebudowane: zakres jako osobny panel, serie jako przewijane chipy, narzędzia w jednym miejscu,
- [x] bez wielkich rzędów pomarańczowych przycisków,
- [x] bez ucinania kontrolek poziomych,
- [x] tooltip wykresu przeniesiony bliżej zaznaczenia i ograniczony do rzeczywistych danych,
- [x] dodano czytelne etykiety bieżących wartości serii po prawej stronie wykresu,
- [x] poprawiono glow, siatkę, osie i linię schodkową/gładką,
- [x] zachowano natywny Android Views — brak WebView/HTML jako UI,
- [x] zachowano Firebase/Auth/REST, telemetrię, GAP, q/sim oraz komendy/ACK,
- [ ] pełny Build → Make Project w Android Studio,
- [ ] test v0.13.0 na realnym telefonie po Wi-Fi,
- [ ] audyt screenshotów Dashboard/Wykresy po instalacji.



## 2026-10-02 — v0.14.0-NATIVE-PREMIUM-2 — REBUILD 2

- [x] zmieniono hierarchię Dashboardu na `POMIARY → STEROWANIE → ANALIZA I SYSTEM`,
- [x] Wilgotność przeniesiona do sekcji pomiarowej,
- [x] stabilizacja wykresów: sortowanie po `ts` i deduplikacja timestampów,
- [x] przy zoom=1 gest przesuwa marker zamiast nieoczekiwanie panoramować,
- [x] pan aktywny dopiero po zoomie,
- [x] 7/30 dni otrzymują etykiety osi uwzględniające datę,
- [x] dodano czytelniejsze pole wykresu, jednostkę osi i oznaczenia GAP,
- [x] pasmo min–max ograniczone do wspólnej osi,
- [x] usunięto duplikat metody `rebuildLegend()` z MainActivity,
- [x] brak WebView / HTML jako UI,
- [ ] pełny Build Android Studio na PC,
- [ ] test 6h/24h/7d/30d na realnym telefonie,
- [ ] test zoom/pan/marker na telefonie,
- [ ] audyt porównawczy z referencją WWW.


## 2026-10-02 — v0.15.0-NATIVE-CHART-STUDIO

- [x] przebudowano ekran Wykresy do jednego spójnego Chart Studio,
- [x] rozdzielono fokus TEMPERATURY / SERWA bez dwóch konkurujących kart na ekranie,
- [x] zakres 6 H / 24 H / 7 DNI / 30 DNI ma jeden wspólny wybór,
- [x] seria / widok / narzędzia zostały przeniesione do zwartego paska akcji i paneli natywnych,
- [x] historia 24 H uruchamia się automatycznie po przywróceniu sesji,
- [x] zachowano GAP, marker, zoom, q/sim, filtr anomalii i fixed range serw,
- [ ] pełny Build Android Studio,
- [ ] ponowny audyt na realnym telefonie.


# 2026-10-02 — v0.16.0-NATIVE-CHART-STUDIO-PRO

- [x] przebudowano ekran WYKRESY jako jeden spójny workspace,
- [x] usunięto pomarańczowe „ściany” przycisków z głównego ekranu,
- [x] wyodrębniono SERIE / OŚ / FILTRY / ANALIZA,
- [x] dodano przewijany wybór aktywnych serii,
- [x] dodano kompaktowy marker/tooltip i punkty kursora,
- [x] dodano kontekstowy pasek zoomu,
- [x] dodano rzeczywistą drugą oś w trybie PODZIELONE dla różnych jednostek,
- [x] dodano Analizę PRO z min/max/średnią/zmianą/GAP/A−B,
- [x] domyślny widok temperatur ustawiono na WSPÓLNA OŚ,
- [ ] Build Android Studio,
- [ ] test 24H/7D/30D na telefonie,
- [ ] audyt wizualny po realnym uruchomieniu.


## 2026-10-02 — v0.17.0-NATIVE-PREMIUM-VISUAL

- [x] pełna przebudowa kompozycji Dashboardu,
- [x] nowy HERO instalacji + szybkie sterowanie,
- [x] uproszczona hierarchia pomiarów/sterowania/analizy,
- [x] przebudowany workspace Wykresów,
- [x] zoom dotyka punktu gestu zamiast przypadkowo zmieniać zakres,
- [x] bezpieczniejsze pozycjonowanie tooltipa,
- [x] nadal zero WebView/HTML jako UI,
- [ ] build Android Studio,
- [ ] test realnego telefonu po Wi‑Fi,
- [ ] audyt wizualny po realnym zrzucie ekranu.


# 2026-10-02 — v0.18.0 NATIVE PREMIUM UX

- ✅ zastąpiono Unicode/emoji w głównej nawigacji własnymi natywnymi ikonami Canvas,
- ✅ dodano wyraźne stany pressed/selected + ripple dla akcji,
- ✅ uporządkowano nawigację dolną,
- ✅ dodano KPI TERAZ / ZAKRES / PUNKTY w module Wykresy,
- ✅ zachowano natywne Firebase/Auth/telemetrię i brak WebView,
- ⬜ build Android Studio na PC,
- ⬜ test na realnym telefonie po Wi‑Fi,
- ⬜ pełny audyt wizualny 13 kafelków,
- ⬜ pełna implementacja modułów Ustawienia/Pogoda/Logi/Terminal/OTA,
- ⬜ pełna parytetowa implementacja narzędzi WWW w Wykresach.


# 2026-10-02 — v0.19.0-NATIVE-VISUAL-PARITY

- [x] przebudowano grafiki kafelków na wielowarstwowe ilustracje natywne inspirowane referencją WWW,
- [x] przebudowano grafikę HERO kotła z rurami, wskaźnikiem temperatury i palnikiem,
- [x] poprawiono głębię kart i stany aktywne/alarmowe,
- [x] skrócono górny i dolny chrome Dashboardu,
- [x] poprawiono czytelność przestrzeni roboczej wykresu,
- [x] znaleziono i poprawiono błąd parsera `row.sim ushr it` w rendererze wykresu,
- [x] wzmocniono siatkę i grubość przebiegów,
- [ ] pełny Build Android Studio na PC,
- [ ] test wizualny na telefonie.

## 2026-10-02 — v0.20.0-NATIVE-PREMIUM-CHART-STUDIO-2

- [x] przebudowano ekran Wykresy na jeden spójny workspace,
- [x] uproszczono kontrolki zakresu i widoku,
- [x] usunięto powtarzające się badge'e serii z wnętrza wykresu,
- [x] ograniczono dekoracyjne wypełnienie serii dla lepszej czytelności,
- [x] zachowano marker, tooltip, zoom, pan, GAP, tryby osi oraz filtry,
- [x] zachowano natywny Android bez WebView,
- [ ] Build Android Studio,
- [ ] test 24 H / 7 DNI / 30 DNI na telefonie,
- [ ] test dotyku i zoomu,
- [ ] finalny audyt Dashboard + Wykresy po realnym buildzie.


## 2026-10-02 — v0.21.0-NATIVE-PRODUCT-UI

- [x] przybliżono paletę i hierarchię UI do referencyjnego WWW bez kopiowania HTML,
- [x] cyan stał się podstawowym akcentem nawigacji i analizatora,
- [x] uporządkowano natywny bottom sheet,
- [x] poprawiono warstwę renderowania wykresu pod kątem sprzętowego Canvas,
- [ ] pełny build Android Studio,
- [ ] test realnego telefonu po Wi‑Fi,
- [ ] audyt wizualny 24 H / 7 DNI / 30 DNI,
- [ ] dopięcie funkcji Delta / alarmy / analiza cykli / odchylenie od normy w nowym UX.


## 2026-10-02 — v0.21.1-NATIVE-PRODUCT-UI

- [x] common action glyphs replaced with native Canvas icons,
- [x] chart selected-state unified to cyan,
- [x] bottom sheet close action uses native icon.


# 2026-10-02 — v0.22.0-NATIVE-PREMIUM-FINISH

- [x] usunięto tekstowe strzałki z kluczowych komponentów UI,
- [x] rozszerzono natywne ikony o następny krok, pogodę i OTA,
- [x] szybkie sterowanie otrzymało spójny komponent ikona + opis + akcja,
- [x] wykres otrzymał marker OSTATNI i subtelną powierzchnię pojedynczej serii,
- [x] podniesiono versionCode do 2200,
- [ ] pełny Build Android Studio,
- [ ] test realnego telefonu po Wi‑Fi,
- [ ] dalsza funkcjonalna parytetowość Ustawienia/Pogoda/Logi/Terminal/OTA.


# 2026-10-02 — v0.23.0-NATIVE-PREMIUM-LONG-BLOCK

- [x] zrobiono większy blok prac zamiast kolejnej mikro-łatki UI,
- [x] Dashboard dostał dynamiczny HERO i natywny banner alarmowy,
- [x] szybkie sterowanie pokazuje rzeczywisty stan pompy/serwa/mieszadła,
- [x] kafelki mają LIVE/STALE/ALARM i wielowarstwowe natywne ilustracje,
- [x] Chart Studio dostało mini-nawigator, marker i stabilniejszy zoom/pan,
- [x] dodano pomocnicze linie alarmowe LoLo/Lo/Hi/HiHi,
- [x] moduł Pogoda korzysta natywnie z Open-Meteo,
- [x] Logi/Terminal/OTA/Sesja mają natywne ekrany zamiast Toast jako jedynej odpowiedzi,
- [x] usunięto błąd składniowy w `DashboardTileView.setStale`,
- [x] brak WebView/HTML jako warstwy UI,
- [ ] pełny Build Android Studio na PC,
- [ ] test 6 H / 24 H / 7 DNI / 30 DNI na telefonie,
- [ ] test komend i ACK po Wi-Fi,
- [ ] audyt wizualny po realnym zrzucie ekranu.

# 2026-10-02 — v0.24.0-NATIVE-PRODUCT-INTERACTION
- [x] przeprojektowano Dashboard pod hierarchię stan → szybkie sterowanie → pomiary → analiza,
- [x] uporządkowano dolną nawigację i sesję,
- [x] przebudowano workspace Wykresów,
- [x] 7/30 dni otrzymały datę + godzinę na osi,
- [x] pasmo min–max rozszerzono na cały aktywny zestaw serii,
- [x] potwierdzono brak WebView/HTML jako UI,
- [ ] pełny Build Android Studio,
- [ ] test realnego telefonu.

# 2026-10-02 — v0.25.0 NATIVE ANALYTICS SUITE
- [x] szybkie akcje Chart Studio: A−B / Cykle / Norma / Alarm,
- [x] analiza A−B z aktywnych serii na wspólnych danych,
- [x] analiza cykli pracy kotła z progiem 45°C i histerezą 2°C,
- [x] analiza normy: średnia / zmiana / STD / MAD,
- [x] graficzny znacznik aktywnej serii zamiast tekstowej kropki,
- [x] powiększony bottom sheet do obsługi dłuższych zestawów opcji,
- [x] pogłębiono wizualną warstwę workspace wykresu,
- [ ] pełny Build Android Studio,
- [ ] test 6 H / 24 H / 7 DNI / 30 DNI,
- [ ] test dotyku, zoomu, A−B i analizy cykli na telefonie,
- [ ] dalsza parytetowość WWW: własny zakres, timeline zdarzeń, eksport/udostępnienie.


## 2026-10-02 — v0.26.0 NATIVE PREMIUM EXPERIENCE

- [x] pełnoekranowy natywny analizator wykresu,
- [x] natywna ikona Expand bez emoji,
- [x] zachowanie aktywnych serii i trybu w pełnym ekranie,
- [x] dalsze odchudzenie interfejsu wykresów,
- [x] brak WebView/HTML jako UI,
- [ ] potwierdzenie build na telefonie po Wi-Fi,
- [ ] wizualny audyt 0.26.0 na realnym urządzeniu.


# 2026-10-02 — v0.27.0-NATIVE-ADAPTIVE-PRO

- [x] przeprojektowano Dashboard jako jedną zwartą powierzchnię operacyjną,
- [x] zredukowano wysokość chrome i ciężar wizualny nagłówków,
- [x] HERO pokazuje temperaturę, tryb, pompę i bezpieczeństwo bez przeładowania,
- [x] szybkie sterowanie otrzymało jednolity komponent ikonowy,
- [x] Wykresy przebudowano do jednego Chart Workspace bez ściany przycisków,
- [x] seria / zakres / filtry / analiza są osobnymi, krótkimi torami sterowania,
- [x] zachowano marker, zoom, GAP, q/sim, A-B, cykle, normę, alarmy i pełny ekran,
- [x] poprawiono paletę Cyan/Ember/TextDim zgodnie z referencją WWW,
- [x] brak WebView/HTML jako UI,
- [ ] pełny Build Android Studio,
- [ ] test na telefonie po Wi-Fi,
- [ ] audyt wizualny Dashboard + Wykresy na realnym urządzeniu,
- [ ] benchmark 360–430 dp i Android 16.


# 2026-10-02 — v0.28.0-NATIVE-ADAPTIVE-PRO-STABILITY

- [x] usunięto nieużywany legacy blok `addModernChartCard` / stary legend renderer z MainActivity,
- [x] odchudzono wysokość standardowego kafelka pomiarowego do 118 dp,
- [x] pozostawiono jeden aktywny model Chart Workspace z bezpośrednimi seriami,
- [x] zachowano natywny Android bez WebView/HTML jako UI,
- [ ] pełny Build Android Studio,
- [ ] test realnego telefonu po Wi-Fi,
- [ ] wizualny audyt 24 H / 7 DNI / 30 DNI,
- [ ] test dotyku i zoomu,
- [ ] test komend i ACK.


# 2026-10-03 — v0.29.0 NATIVE VISUAL CLEANUP

- ✅ przeanalizowano rzeczywiste ekrany telefonu z Dashboard / Wykresy / Więcej.
- ✅ zidentyfikowano zasłanianie treści przez dolną nawigację.
- ✅ zidentyfikowano zbyt duży obszar wykresu.
- ✅ naprawiono brakujący reveal animator wykresu i wymuszono redraw po danych.
- ✅ uproszczono dolną nawigację.
- ✅ zmniejszono glow przebiegów i etykiety końcowe.
- 🟨 pełny Make Project w Android Studio — do potwierdzenia.
- 🟨 test 0.29.0 na telefonie po Wi-Fi.
