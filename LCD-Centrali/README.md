# Panel LCD (wyświetlacz CO) — firmware v1.2.0

Źródła panelu dotykowego (ESP32 + ILI9488 + LVGL), cienkiego klienta HTTP
centrali. Punkt wyjścia: v1.1.0 z `LCD Centrali.zip`.

## Budowanie

1. Do `src/` włóż plik `secrets.h`. Najprościej **skopiować `secrets.h`
   z projektu centrali** — panel używa z niego dwóch wpisów o tych samych
   nazwach: `SECRET_WIFI_AP_PASSWORD` i `SECRET_OTA_HASLO`. Można też skopiować
   `src/secrets.h.example` jako `src/secrets.h` i wpisać te dwie wartości
   ręcznie (w v1.1.0 były w `main.cpp` jako `WIFI_PASS` i `OTA_HASLO_PANEL`).
   Bez tego pliku kompilacja zatrzymuje się z komunikatem, co zrobić.
   `secrets.h` jest w `.gitignore`.
2. Reszta bez zmian: PlatformIO → Upload (`pio run -e esp32dev -t upload`).
   `platformio.ini`, `partitions.csv` i `include/lv_conf.h` są takie same jak
   w v1.1.0.

Katalog `testy_host/` nie jest częścią firmware'u — PlatformIO go pomija.

## Co zmienia v1.2.0

Powód: zgłoszenie „kafelki się nie aktualizują”. **Przyczyna zgłoszenia nie
jest potwierdzona** — log v1.1.0 nie mówi, co panel odczytał ani co pokazał.
v1.2.0 usuwa błędy, które dają taki objaw, i dodaje to, czego w logu brakowało.

| Obszar | v1.1.0 | v1.2.0 |
|---|---|---|
| Odpowiedź nie do odczytania (zły kod HTTP, zły JSON) | cicho pomijana, kafelki ze starymi wartościami | linia `lvl=ERR` z przyczyną; po 10 s kafelki pokazują `--` |
| Utrata danych w trakcie pracy | kafelki i wirnik pompy bez zmian | po 10 s `--`, wirnik staje |
| Symulacja czujników w centrali | nie do zauważenia na LCD | napis `SYMULACJA: zewn` w nagłówku + wpis w logu |
| Wybór centrali | pierwsza znaleziona sieć `PiecCO_AP` | skan przy starcie, połączenie z najsilniejszą; BSSID w logu |
| Zapamiętana centrala zniknęła | panel nie łączy się z żadną inną | po 3 próbach szuka od nowa |
| Brak centrali przy włączeniu | watchdog restartuje panel co ok. 15 s | panel czeka i łączy się, gdy centrala wstanie |
| Nieudany POST | linia błędu w każdym obrocie pętli | jedna linia |
| Log przy zwykłej pracy | 3 linie `[NET-DIAG]` co 2 s | jedna linia `tag=PANEL` co 30 s |
| Wskaźnik baterii bez ogniwa | `0%` w kolorze ładowania | `USB` |
| Hasła | wpisane w `main.cpp` | w `src/secrets.h`, poza gitem |

Kolumna „v1.1.0” pochodzi z czytania kodu i z testu na PC, nie z obserwacji
na płytce — z wyjątkiem wierszy o logu i o baterii, które widać w logach
z 10 i 11.11. Pełny opis: `HISTORIA ZMIAN` na górze `src/main.cpp`.

## Logi z 2026-11-11 (panel + centrala na gołej płytce)

- **Panel na biurku ma starszy build niż te źródła.** Jeden nieudany odczyt
  ustawień daje w jego logu setki linii `[USTAWIENIA] GET nieudany: timeout
  calkowity`; źródła z `LCD Centrali.zip` (i v1.2.0) piszą jedną. W adresie
  zapytań widać też `reset_powod=task_wdt`, czyli wcześniejszy restart przez
  watchdog.
- Łączność działa: po włączeniu 54 z 56 odczytów `/api/status` udanych.
- Centrala podawała: `piec=-1.0C bojler=-1.0C panel=-1.0C pom=-1.0C
  zewn=18.5C cisnienie=0.0hPa wilgotnosc=0.0% klapa=0st syberek=16st dym=2413`
  — brak czujników (−1) i jedna wartość z symulacji (zewn).
- Tak wygląda ekran v1.2.0 dla tych danych (zrzut z testu na PC, nie zdjęcie
  płytki): `ekran_gola_plytka.png`.

## Jak czytać ekran

Środek nagłówka dashboardu (między baterią a ikonami po prawej):

- **nic** — jedna centrala, żadnej symulacji. Stan normalny.
- **`SYMULACJA: zewn, pokoj`** — centrala podaje te czujniki jako symulowane
  (włączone z jej panelu WWW). Ich wartości na kafelkach nie są pomiarem.
  Napis pojawia się i znika razem z symulacją w centrali, najdalej po kilku
  sekundach — to najprostszy znak, że panel czyta świeże dane.
- **`2 CENTRALE (XX:YY)`** — przy włączeniu panelu w zasięgu były co najmniej
  dwie sieci `PiecCO_AP`. `XX:YY` to końcówka adresu (BSSID) tej, z którą
  panel jest połączony.
- **`SYMULATOR`** — odpowiada `centrala-testowa-c3`.

Kafelki z `--` znaczą: od 10 s panel nie odczytał poprawnego statusu.
Kafelek z `-1°C` znaczy: centrala nie ma odczytu z tego czujnika.

## Jak czytać log

```
tag=WIFI zdarzenie=skan_centrala nr=1/1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=WIFI zdarzenie=polaczono bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=PANEL zdarzenie=centrala_rozpoznana typ=sterownik
tag=PANEL zdarzenie=symulacja_centrali czujniki=zewn
tag=PANEL fw=v1.2.0 wifi=1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45 ap_przy_starcie=1 centrala=sterownik sym=zewn
          status_ok=15 status_blad=0 ust_ok=6 ust_blad=0 ekran=15 wiek_danych_s=1
          t_zewn=18.5 t_pokoj=-1.0 t_ogrz=-1.0 t_ogrz_sr=0.0 t_bojler=-1.0 t_panel=-1.0
          wilg=0 cisn=0 dym=2413 klapa=0 syberka=16 pompa=0 czas=20:41 heap=... heap_min=... blok_max=...
```

(Przykład z testu na PC; linia `tag=PANEL fw=…` jest w logu jedną linią.)

- `tag=PANEL fw=…` — 5 s po starcie i potem co 30 s:
  - `t_zewn`, `t_pokoj`, `t_ogrz`… `dym`, `klapa`, `syberka` — wartości, które
    panel ma w pamięci i podaje kafelkom. **Porównaj je z linią `tag=STATUS`
    w logu centrali** (`zewn=`, `pom=`, `piec=`…): mają być takie same;
  - `status_ok` / `status_blad` — ile odpowiedzi `/api/status` udało się
    odczytać, a ile odrzucono od poprzedniej takiej linii (zdrowo: 15 i 0);
  - `ekran` — ile razy w tym czasie dane poszły na kafelki;
  - `sym` — czujniki, które centrala podaje jako symulowane;
  - `heap`, `heap_min`, `blok_max` — wolna pamięć, jej minimum od startu
    i największy ciągły wolny blok.
- `tag=PANEL zdarzenie=symulacja_centrali czujniki=…` — centrala włączyła
  albo wyłączyła symulację czujnika (`brak` = żadna).
- `tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=json|kod_http|nie_http|brak_ciala …`
  — odpowiedź przyszła, ale nie dało się jej użyć; dalej w linii kod HTTP,
  błąd JSON, pamięć, początek i koniec treści. Najwyżej jedna na 10 s.
- `tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne` / `dane_wrocily` — chwila,
  w której kafelki przeszły na `--` i w której wróciły.
- `tag=WIFI zdarzenie=skan_centrala` — każda sieć `PiecCO_AP` widziana przy
  starcie. Tylko przy zimnym starcie, więc log warto łapać od włączenia.

Stary, gęsty log `[NET-DIAG] start/wyslano/sukces` można przywrócić, ustawiając
`SiecCfg::POZIOM_LOGU = 2` w `src/siec_nieblokujaca_LCD.h`.

## Co zostało sprawdzone, a co nie

- **Na PC, z atrapami sprzętu:** prawdziwy `main.cpp` i dashboard uruchomione
  zwykłym `g++` przeciwko udawanym centralom — 10 scenariuszy, 102
  sprawdzenia, plus 136 sprawdzeń jednostkowych; także pod AddressSanitizer
  i UBSan. Udawana centrala wysyła ten sam JSON co firmware 3.58.3.
- **Składnia i typy, prawdziwymi nagłówkami ESP32:** wszystkie trzy pliki
  `.cpp` bez błędów z arduino-esp32 3.0.7 i 3.3.8, ESP-IDF 5.1, TFT_eSPI,
  LVGL i ArduinoJson (`testy_host/sprawdz_naglowki_esp32.sh`; kompilator clang,
  bez linkowania).
- **Nie sprawdzone:** build PlatformIO (GCC, linkowanie, rozmiar binarki)
  i działanie na płytce. Opis ograniczeń: `testy_host/README.md`.
- Zimny start trwa o czas skanu dłużej (szacunkowo 1,5–2 s; niezmierzone).
