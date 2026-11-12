# Panel LCD (wyświetlacz CO) — firmware v1.2.2

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

## Co zmienia v1.2.2

**Przyczyna znaleziona: panelowi brakowało pamięci RAM.** Pokazał to log
panelu z 2026-11-12 (v1.2.0):

- po starcie prawie każda odpowiedź centrali kończyła się linią
  `tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna … json=NoMemory bajtow=1811`.
  Odpowiedź przyszła cała, ale panel nie miał gdzie jej rozpakować. Kafelki
  pokazywały `--` przez ok. 18 minut;
- linie `tag=PANEL` podawały `heap=47032 heap_min=39624 blok_max=38900`,
  z `blok_max` identycznym przez godzinę. Te liczby doliczały ok. 39 kB
  obszaru, z którego program nie może brać pamięci (opis w `src/main.cpp`
  przy `ramWolny()`). Naprawdę wolne było ok. 8 kB, a w najgorszej chwili
  poniżej 1 kB;
- wcześniejszy restart w tym logu (`task_wdt … loopTask`) to najprawdopodobniej
  ta sama przyczyna: biblioteka ekranu (LVGL) po nieudanym przydziale pamięci
  staje w miejscu, a po 15 s panel restartuje watchdog. Pewności nie ma —
  adresów ze śladu wywołań nie da się tu zamienić na nazwy funkcji.

v1.1.0 zajmowała tyle samo pamięci, tylko o jej braku nie pisała: nieudane
rozpakowanie odpowiedzi kończyło się po cichu, a kafelki zostawały ze starymi
wartościami. To najpewniej tłumaczy pierwsze zgłoszenie („kafelki się nie
aktualizują”) — logu panelu z tamtej chwili nie ma, więc to wniosek z kodu
i z pomiaru, nie obserwacja.

| | v1.2.0 | v1.2.2 |
|---|---|---|
| Pamięć zajęta przez firmware po starcie (pomiar na PC) | 171 kB | 129 kB |
| Wolny RAM na płytce po starcie | ok. 8 kB (z logu) | 49,7 kB (z logu; szacunek z PC: 48,6 kB) |
| Bufor rysowania ekranu | 38,4 kB | 15,4 kB |
| Bufory ekranu Wykresy (18,5 kB) | trzymane zawsze | tylko gdy ekran jest otwarty |
| Pamięć potrzebna chwilowo na jedno odpytanie centrali | 7,4 kB | 5,6 kB (odpowiedź nie jest już kopiowana) |
| Wolna pamięć w logu | `heap=` — zawyżone o ok. 39 kB | `ram=` — prawdziwa |
| Brak pamięci na odczytanie odpowiedzi | kafelki `--`, w logu `przyczyna=json` (wygląda na winę centrali), bez restartu | nagłówek `BRAK PAMIECI`, `przyczyna=pam`, restart po 45 s |
| Brak pamięci na odebranie odpowiedzi | odpowiedź po cichu ucięta, w logu wygląda jak błąd centrali | `GET nieudany: brak pamieci na odpowiedz`, `przyczyna=pam` |
| Linie logu przy braku pamięci | linie od 64 znaków mogą przepaść | najważniejsze linie wypisują się zawsze |
| LVGL nie dostaje pamięci | ekran zamiera na 15 s, restart przez watchdog | napis `BRAK PAMIECI - RESTART`, restart po 2,5 s, powód w logu centrali |

Obraz na ekranie jest taki sam (20 zrzutów z testów na PC, piksel w piksel).
Mniejszy bufor rysowania oznacza więcej, niższych pasków przy przerysowaniu
całego ekranu; czy to widać na płytce, nie zostało sprawdzone.

Od ok. 19. minuty tego logu każda próba kończyła się limitem czasu, przy
połączonym WiFi. Z logu nie da się rozstrzygnąć, czy panel gubił odpowiedzi
z braku pamięci, czy centrala przestała mu odpowiadać. W obu przypadkach
panel wychodzi z tego sam, mechanizmami z v1.2.1 (niżej).

**Po wgraniu** w logu panelu pojawi się raz linia
`tag=PAMIEC zdarzenie=po_starcie ram=… ram_blok=… ram_min=…`. `ram` powinno
wynosić kilkadziesiąt tysięcy (bajtów). To jest sprawdzian tej wersji: jeśli
będzie poniżej 20 000, przyślij tę linię.

### Wynik na płytce (2026-11-12)

Log panelu z pierwszych ok. 40 s po wgraniu v1.2.2 (zbudowanej w PlatformIO
przez użytkownika):

```
[LVGL] start lvgl_port_init(), wolny RAM=234196 B
[LVGL] bufor rysowania zaalokowany (15360 B), wolny RAM=218148 B
tag=PAMIEC zdarzenie=po_ekranach ram=105568 ram_blok=102388
tag=PAMIEC zdarzenie=po_starcie ram=49676 ram_blok=42996 ram_min=43836
tag=PANEL fw=v1.2.2 … status_ok=1 status_blad=0 ust_ok=1 ust_blad=0 … ram=50516 ram_min=43836 ram_blok=42996 … autorestartow=0
tag=PANEL fw=v1.2.2 … status_ok=15 status_blad=0 ust_ok=6 ust_blad=0 … ram=50076 ram_min=41644 ram_blok=42996 … autorestartow=0
```

- Po starcie wolne jest 49,7 kB (v1.2.0: ok. 8 kB). Najmniej w tym czasie:
  41,6 kB.
- Wszystkie odpowiedzi zostały odczytane: 16 ze statusem i 7 z ustawieniami,
  zero błędów, żadnego `json=NoMemory`, żadnego ponownego łączenia ani
  restartu.
- Panel zauważył zmianę symulacji w centrali (`czujniki=zewn` →
  `czujniki=zewn,piec`), czyli dostaje świeże dane.
- Według zgłoszenia ekran działa prawidłowo.

To krótki log. Pracy przez wiele godzin jeszcze nie pokazuje.

## Co zmienia v1.2.1

Powód: zgłoszenie z 2026-11-12 — panel z v1.2.0 nie pokazywał świeżych
danych, aż został zresetowany. Gdy powstawała v1.2.1, był tylko log centrali
i przyczyna nie była znana; pokazał ją dopiero log panelu (v1.2.2 wyżej).
Wszystko, co poniżej, jest także w v1.2.2.

Gdy panel nie ma danych z centrali, sam robi to, co dotąd pomagało:

| Czas bez poprawnego statusu | Co robi panel |
|---|---|
| 10 s | kafelki `--` (jak w v1.2.0) i **powód w nagłówku**: `BRAK WIFI`, `CENTRALA MILCZY` albo `ZLA ODPOWIEDZ`, z licznikiem sekund |
| 12 s, gdy WiFi jest „połączone”, a centrala milczy | rozłącza się i łączy od nowa; po kolejnych 12 s jeszcze raz, szukając sieci centrali od zera. v1.1.0 i v1.2.0 w tym stanie nie robiły nic |
| 45 s | **restartuje się sam**; ostatnie 10 s nagłówek odlicza `RESTART ZA 7 s: …` |
| nadal bez danych | kolejne restarty po 90 s, 3 min, potem co 5 min (centrala wyłączona na dłużej nie powoduje restartów w kółko); licznik wraca do 45 s po 2 minutach poprawnych danych |

Gdy centrala odpowiada, ale nieczytelnie (`ZLA ODPOWIEDZ`), restarty są
najwyżej dwa z rzędu — trzeci nic by nie dał.

Ten sam napis co w nagłówku stoi teraz także **na ekranie alarmu**, nad
przyciskami WYCISZ / ZAMKNIJ (`ekran_alarm_bez_danych.png`). W logu centrali
z 2026-11-12 alarm przegrzania był aktywny (`piec=134.0C` na płytce bez
czujników). Przy takim statusie panel pokazuje pełnoekranowy alarm, który
zasłania kafelki i nagłówek — nie było na nim widać, czy dane jeszcze
przychodzą, ani że temperatura pochodzi z symulacji. Teraz stoi tam
`BRAK WIFI 14 s` albo `SYMULACJA: piec`.

Po takim restarcie panel **mówi centrali, dlaczego go zrobił**, a centrala
wpisuje to do swojego logu (i wysyła wiadomość na Telegram, tak jak o każdym
nietypowym restarcie panelu):

```
tag=RESET-DIAG lvl=WARN zdarzenie=nietypowy_restart_panelu powod=auto_wifi_r201_n1_t45_h61_b31_en118 poprzedni=poweron
```

Jeśli problem wróci, **ta linia z logu centrali powie, czego panelowi
brakowało** — rozpisanie pól jest niżej, w rozdziale „Jak czytać log”.

Każdy taki restart centrala ogłasza raz (po jej pierwszej odpowiedzi panel
wraca do zwykłego `reset_powod=sw`). Gdy restarty idą seriami, zgłaszane są
pierwsze trzy, a potem 6., 12., 24.… z serii — żeby łączność, która rwie się
raz za razem, nie dawała wiadomości co kilka minut. Serię kończy godzina
poprawnych danych; numer w serii jest w zgłoszeniu (`n6`).

Alarm nie ginie przy takim restarcie: stan alarmu (dym, przegrzanie pieca
lub panelu, WYCISZ, ZAMKNIJ) jest zapamiętywany przed restartem, a brzęczyk
i ekran alarmu wracają po nim, zanim przyjdą dane.

Przy okazji naprawione trzy błędy brzęczyka (były też w v1.1.0 i v1.2.0):

- WYCISZ działał dopiero po następnym poprawnym statusie — czyli bez danych
  z centrali wcale; teraz cisza zapada od razu;
- drugie naciśnięcie WYCISZ włączało dźwięk z powrotem;
- raz wyciszony rodzaj alarmu zostawał niemy przy każdym następnym
  wystąpieniu, aż do restartu panelu; teraz wyciszenie kończy się razem
  z alarmem.

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
W tym samym miejscu nagłówka stoi wtedy powód (v1.2.1):

- **`BRAK WIFI 14 s`** — panel nie jest połączony z siecią centrali
  (ikona WiFi po prawej jest przekreślona).
- **`CENTRALA MILCZY 14 s`** — WiFi jest połączone, ale centrala nie
  odpowiada na zapytania.
- **`ZLA ODPOWIEDZ 14 s`** — centrala odpowiada, ale treści nie da się
  odczytać (szczegóły w logu: `tag=STATUS lvl=ERR`).
- **`BRAK PAMIECI 14 s`** (v1.2.2) — odpowiedź centrali przyszła, ale
  panelowi zabrakło pamięci RAM, żeby ją odczytać. Wina leży w panelu.
- **`RESTART ZA 7 s: BRAK WIFI`** — za tyle sekund panel zrestartuje się sam.

Czerwony pasek **`BRAK PAMIECI - RESTART`** na górze ekranu (v1.2.2):
bibliotece ekranu zabrakło pamięci w trakcie rysowania; panel restartuje się
po 2,5 s. `BLAD EKRANU - RESTART` to ten sam restart przy innym błędzie
wewnętrznym tej biblioteki.

Tak to wygląda (zrzuty z testu na PC, nie zdjęcia płytki):
`ekran_centrala_milczy.png` — dashboard, `ekran_alarm_bez_danych.png` — ekran
alarmu (ten sam napis nad przyciskami; od v1.2.1).

Kafelek z `-1°C` znaczy: centrala nie ma odczytu z tego czujnika.

## Jak czytać log

```
tag=WIFI zdarzenie=skan_centrala nr=1/1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=WIFI zdarzenie=polaczono bssid=24:6F:28:11:22:33 kanal=1 rssi=-45
tag=PANEL zdarzenie=centrala_rozpoznana typ=sterownik
tag=PANEL zdarzenie=symulacja_centrali czujniki=zewn
tag=PAMIEC zdarzenie=po_ekranach ram=... ram_blok=...
tag=PAMIEC zdarzenie=po_starcie ram=... ram_blok=... ram_min=...
tag=PANEL fw=v1.2.2 wifi=1 bssid=24:6F:28:11:22:33 kanal=1 rssi=-45 ap_przy_starcie=1 centrala=sterownik sym=zewn
          status_ok=15 status_blad=0 ust_ok=6 ust_blad=0 ekran=15 wiek_danych_s=1
          t_zewn=18.5 t_pokoj=-1.0 t_ogrz=-1.0 t_ogrz_sr=0.0 t_bojler=-1.0 t_panel=-1.0
          wilg=0 cisn=0 dym=2413 klapa=0 syberka=16 pompa=0 czas=20:41 ram=... ram_min=... ram_blok=...
          bez_danych_s=1 przyczyna=- wymuszen=0 rozl_kod=0 rozl_n=0 autorestartow=0
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
  - `ram`, `ram_min`, `ram_blok` (v1.2.2) — wolny RAM w bajtach, jego
    minimum od startu i największy ciągły wolny blok. Zdrowy panel ma tu
    kilkadziesiąt tysięcy; poniżej 16 000 zaczynają się kłopoty. W v1.2.0
    i v1.2.1 te pola nazywały się `heap`, `heap_min`, `blok_max` i były
    zawyżone o ok. 39 000.
- `tag=PAMIEC zdarzenie=po_ekranach` / `po_starcie` (v1.2.2) — raz, przy
  starcie: ile RAM-u zostało po zbudowaniu ekranów (przed WiFi) i po
  odebraniu pierwszych danych.
- `tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci ram=… ram_blok=… ram_min=…`
  (v1.2.2) — wolnego RAM-u jest mniej niż 16 000; najwyżej raz na minutę.
- `tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=… ram_blok=… ram_min=…`
  (v1.2.2) — bibliotece ekranu zabrakło pamięci (albo wykryła inny błąd
  wewnętrzny); panel restartuje się od razu.
- `[STATUS] GET nieudany: brak pamieci na odpowiedz errno=12 ENOMEM` (v1.2.2)
  — panelowi zabrakło pamięci już przy odbieraniu odpowiedzi.
- `tag=PANEL zdarzenie=symulacja_centrali czujniki=…` — centrala włączyła
  albo wyłączyła symulację czujnika (`brak` = żadna).
- `tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=json|kod_http|nie_http|brak_ciala …`
  — odpowiedź przyszła, ale nie dało się jej użyć; dalej w linii kod HTTP,
  błąd JSON, pamięć, początek i koniec treści. Najwyżej jedna na 10 s.
  **`json=NoMemory` znaczy: odpowiedź jest dobra, a panelowi zabrakło
  pamięci** (patrz `ram=` w tej samej linii).
- `tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne` / `dane_wrocily` — chwila,
  w której kafelki przeszły na `--` i w której wróciły.
- `tag=WIFI zdarzenie=skan_centrala` — każda sieć `PiecCO_AP` widziana przy
  starcie. Tylko przy zimnym starcie, więc log warto łapać od włączenia.

Linie dodane w v1.2.1 (panel bez danych):

```
tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne wiek_s=10 prog_s=10 wifi=1 przyczyna=milczy restart_za_s=34
tag=WIFI lvl=WARN zdarzenie=wymuszam_polaczenie nr=1 szukam=ten_sam_bssid bssid=24:6F:28:11:22:33 kanal=6 rssi=-45 bez_danych_s=12 msg="..."
tag=WIFI zdarzenie=rozlaczono kod=201 nazwa=NO_AP_FOUND nowych=1 lacznie=3
[STATUS] GET nieudany: connect() natychmiastowy blad errno=118 EHOSTUNREACH
tag=PANEL lvl=ERR zdarzenie=autorestart przyczyna=wifi bez_danych_s=45 nr_z_rzedu=1 w_serii=1 lacznie=1 wifi=0 rssi=0 rozl_kod=201 rozl_nazwa=NO_AP_FOUND rozl_n=14 ostatni_blad="..." ram=... ram_blok=... ram_min=... alarmy=0x00 msg="..."
tag=PANEL lvl=WARN zdarzenie=start_po_autorestarcie nr_z_rzedu=1 w_serii=1 zgloszony_nr=0 ... zglaszam_centrali=auto_wifi_r201_n1_t45_h61_b31_en118
tag=PANEL zdarzenie=autorestart_zgloszony powod=auto_wifi_r201_n1_t45_h61_b31_en118 msg="..."
```

- `przyczyna=wifi | milczy | odp | pam` — to samo, co napis w nagłówku
  (`pam` = brak pamięci w panelu, od v1.2.2).
  `restart_za_s=-1` — przy tej przyczynie restartu już nie będzie.
- `tag=PANEL lvl=WARN zdarzenie=autorestart_pominiety` — odpowiedzi centrali
  są nieczytelne mimo dwóch restartów z rzędu; panel czeka bez restartu.
- `tag=PANEL zdarzenie=autorestart_licznik_wyzerowany z_rzedu=1->0 w_serii=1->1`
  — dane poprawne od 2 minut: następny samoczynny restart znów po 45 s.
  Po godzinie taka sama linia z `w_serii=…->0`: koniec serii.
- `nr_z_rzedu` — restarty bez 2 minut poprawnych danych pomiędzy (od tego
  zależy, po ilu sekundach będzie następny); `w_serii` — restarty bez godziny
  poprawnych danych pomiędzy (ten numer idzie do centrali jako `n…`).
- `zglaszam_centrali=sw` w linii `start_po_autorestarcie` — ten restart
  z serii nie jest zgłaszany centrali (zgłoszony był nr `zgloszony_nr`).
- `autorestart_zgloszony` — centrala odpowiedziała na zapytanie z powodem
  restartu, czyli go dostała.
- `tag=WIFI zdarzenie=rozlaczono kod=…` — każde rozłączenie WiFi z kodem
  przyczyny podanym przez radio. Najczęstsze: `201 NO_AP_FOUND` (sieci
  centrali nie ma w eterze), `200 BEACON_TIMEOUT` (centrala przestała
  nadawać), `8 ASSOC_LEAVE` (panel rozłączył się sam przed kolejną próbą),
  `15 4WAY_HANDSHAKE_TIMEOUT` (typowo złe hasło).
- `errno=…` w liniach `GET nieudany` — dlaczego nie udało się nawet zacząć
  połączenia: `EHOSTUNREACH`/`ENETUNREACH` = panel nie ma połączenia WiFi
  albo adresu, `ENOMEM`/`ENOBUFS` = w panelu zabrakło pamięci.
- Linia `tag=PANEL fw=…` ma na końcu nowe pola: `bez_danych_s`, `przyczyna`
  (`-` = nic nie brakuje), `wymuszen`, `rozl_kod`, `rozl_n` (rozłączenia WiFi
  od startu), `autorestartow` (od włączenia zasilania).

**Powód samoczynnego restartu w logu centrali** (`tag=RESET-DIAG … powod=…`),
na przykładzie `auto_wifi_r201_n1_t45_h61_b31_en118`:

| Pole | Znaczenie |
|---|---|
| `auto` | panel zrestartował się sam, bo nie miał danych |
| `wifi` | czego brakowało: `wifi` (brak połączenia z siecią centrali), `milczy` (WiFi jest, centrala nie odpowiada), `odp` (odpowiedź nieczytelna), `pam` (panelowi zabrakło pamięci; v1.2.2), `lvgl` (błąd wewnętrzny biblioteki ekranu, zwykle też brak pamięci — restart od razu; v1.2.2) |
| `r201` | ostatni kod rozłączenia WiFi (`r0` = nie było żadnego) |
| `n1` | który to taki restart w serii (serię kończy godzina poprawnych danych); przeskok numeru, np. z `n3` na `n6`, to restarty, których panel nie zgłaszał |
| `t45` | ile sekund panel był bez danych |
| `h61`, `b31` | wolny RAM i największy wolny blok, w kB. Od v1.2.2 to prawdziwe liczby; zdrowy panel ma tu kilkadziesiąt, `h1_b0` znaczy brak pamięci |
| `en118` | ostatnia nieudana próba statusu — etap i numer błędu: `el0` centrala nie odpowiada na próbę połączenia, `ec0` połączono, brak odpowiedzi, `en118` połączenie odrzucone od razu (118 = brak drogi do centrali, 12 = brak pamięci), `eo500` odpowiedź nieczytelna (kod HTTP), `em200` odpowiedź przyszła, zabrakło pamięci na jej odczytanie, `em12` zabrakło pamięci już na jej odebranie, `ev0` błąd biblioteki ekranu |

Którą wersję panel ma wgraną, też widać w logu centrali: gdy panel zgłosi
się z inną wersją niż poprzednio (albo pierwszy raz po restarcie centrali),
centrala pisze `tag=OTA-PANEL lvl=INFO zdarzenie=wersja_zgloszona wersja=v1.2.2`.

Stary, gęsty log `[NET-DIAG] start/wyslano/sukces` można przywrócić, ustawiając
`SiecCfg::POZIOM_LOGU = 2` w `src/siec_nieblokujaca_LCD.h`.

## Co zostało sprawdzone, a co nie

- **Na PC, z atrapami sprzętu:** prawdziwy `main.cpp` i dashboard uruchomione
  zwykłym `g++` przeciwko udawanym centralom — 26 scenariuszy, 252
  sprawdzenia, plus 262 sprawdzenia jednostkowe; także pod AddressSanitizer
  i UBSan. Udawana centrala wysyła ten sam JSON co firmware 3.58.3. W czterech
  nowych scenariuszach panel nie dostaje pamięci na żądanie testu — tak, jak
  na płytce z logu z 2026-11-12.
- **Pamięć, na PC** (`testy_host/pomiar_pamieci.sh`): ten sam firmware
  zbudowany jako program 32-bitowy, z licznikiem każdego przydziału pamięci
  i z narzutem sterty ESP32. Po starcie trzyma 129 244 B (v1.1.0: 170 324 B,
  v1.2.0: 170 572 B), jedno odpytanie centrali bierze chwilowo 5 588 B
  (v1.2.0: 7 412 B), przez 10 minut pracy nie przybywa nic. Zmiennych
  statycznych przybyło 864 B. Skrypt pilnuje tych liczb limitami. Wszystkie
  scenariusze przechodzą także w tym modelu.
- **Składnia i typy, prawdziwymi nagłówkami ESP32:** wszystkie trzy pliki
  `.cpp` bez błędów z arduino-esp32 3.0.7 i 3.3.8, ESP-IDF 5.1, TFT_eSPI,
  LVGL i ArduinoJson (`testy_host/sprawdz_naglowki_esp32.sh`; kompilator clang,
  bez linkowania).
- **Wersje bibliotek:** `platformio.ini` dopuszcza każde LVGL 8.x od 8.3.11
  i każdy ArduinoJson 7.x od 7.2.0 — którą wersję ma dany komputer, zależy od
  tego, kiedy PlatformIO je pobrał. Wszystko powyższe przechodzi z LVGL 8.3.11
  i ArduinoJson 7.2.1 oraz z najnowszymi wydaniami (LVGL 8.4.0, ArduinoJson
  7.4.3). Pamięć po starcie wychodzi identyczna, odpytanie centrali o 140 B
  mniejsze.
- **Na płytce** (2026-11-12): firmware zbudowany w PlatformIO i wgrany przez
  użytkownika. Panel startuje, zgłasza centrali `v1.2.2`, ma 49,7 kB wolnego
  RAM-u po starcie i w ok. 40 s logu odczytał wszystkie odpowiedzi. Liczby:
  „Wynik na płytce” wyżej.
- **Nadal nie sprawdzone na płytce:** praca przez wiele godzin; zachowanie
  przy prawdziwym braku pamięci i samoczynne restarty (w tym logu nie było do
  nich powodu); ekran Wykresy, który bierze dodatkowe 18,6 kB; szybkość
  rysowania (nie była mierzona). Ograniczenia testów na PC opisuje
  `testy_host/README.md`.
- **Szacunek a pomiar.** Pomiar na PC nie obejmuje pamięci, którą na płytce
  biorą WiFi i system; skrypt przyjmuje dla nich 55,9 kB, wyliczone z logu
  v1.2.0. Szacował 48,6 kB wolnego RAM-u po starcie, płytka pokazała 49,7 kB.
  Firmware zajmuje o ok. 40 kB mniej niż v1.2.0: 41,3 kB mniej pamięci
  przydzielanej w trakcie pracy, 0,9 kB więcej zmiennych statycznych (bufor
  logu).
- **Zachowanie radia jest w testach założeniem.** Scenariusz
  `martwe_polaczenie` (centrala restartuje się tak szybko, że panel dalej
  uważa się za połączony) pokazuje, że od v1.2.1 panel wychodzi z takiego
  stanu sam po ok. 13 s, a v1.2.0 nie wychodzi wcale — ale czy właśnie to
  dzieje się na płytkach, nie wiadomo. Dlatego jest też restart po 45 s,
  który nie zależy od tego założenia.
- Zimny start trwa o czas skanu dłużej (szacunkowo 1,5–2 s; niezmierzone).
