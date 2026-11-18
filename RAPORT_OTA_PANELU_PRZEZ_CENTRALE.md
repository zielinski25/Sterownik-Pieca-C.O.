# Czy aktualizacja panelu LCD przez centralę i Telegram zadziała

Data: 2026-11-12. Dotyczy panelu LCD (na płytce v1.2.2, w repozytorium
v1.3.0) i centrali 3.58.5. Kod aktualizacji panelu jest identyczny we
wszystkich wersjach panelu od v1.1.0 do v1.3.0, a w centrali nie zmienił się
między 3.58.3 a 3.58.5. Firmware'u nie zmieniałem — to jest sprawdzenie.

> **Dopisane 2026-11-12, po odpowiedzi „Działaj”.** Ten raport opisuje stan
> sprzed zmian: panel do v1.3.0 i centralę do 3.58.5. Zmiany są zrobione
> w panelu **v1.4.0** i centrali **3.58.6**. Co z czego wynikło: rozdział
> „Co zostało zrobione” na końcu. Rozdział „Jak zrobić, żeby zadziałało”
> dotyczy starego kodu.

## Wynik

**Dziś nie zadziała, ale też niczego nie zepsuje.** Komenda `/update_panel`
skończy się odpowiedzią centrali „Pobieranie firmware panelu nieudane: …
kod 404”. W żadnym z 39 wydań na GitHubie nie ma pliku `firmware_panel.bin`,
którego centrala szuka.

**Mechanizm jest kompletny i obie strony do siebie pasują.** Adres, hasło,
pola w odpowiedzi centrali i zgłaszanie wersji zgadzają się po obu stronach.
Po spełnieniu warunków z rozdziału „Jak zrobić, żeby zadziałało”
aktualizacja powinna przejść. Na tych urządzeniach nie była jeszcze
wykonana, więc pierwsza próba będzie testem.

**Jest jedna groźna pułapka.** Centrala uznaje aktualizację za wykonaną
tylko wtedy, gdy panel zgłosi wersję identyczną co do znaku z tagiem wydania
na GitHubie. Panel zgłasza `v1.3.0`, a wydania mają tagi takie jak `3.57.9`.
Gdy teksty się różnią, panel wgrywa ten sam plik przy każdym starcie i nie
dochodzi do ekranu z kafelkami.

| Co zrobisz | Co się stanie |
|---|---|
| Wyślesz `/update_panel` dziś | Centrala: „Nowa wersja panelu: 3.57.9 (…). Pobieram na centralę...”, potem „Pobieranie firmware panelu nieudane: … kod 404”. Nic się nie zmienia. |
| Dołączysz `firmware_panel.bin` do wydania z tagiem centrali, np. `3.58.5` | Po restarcie panel wgrywa plik, zgłasza `v1.3.0`, centrala tego nie uznaje i panel wgrywa plik od nowa — w kółko, z ciemnym ekranem. Centrala nie wysyła żadnej wiadomości i odmawia `/update` oraz `/update_panel`. |
| Zrobisz osobne wydanie z tagiem `v1.3.0` i samym `firmware_panel.bin` | Po wyłączeniu i włączeniu panelu aktualizacja powinna przejść, a centrala napisać „Panel zaktualizowany do v1.3.0.”. Nie sprawdzone na płytkach. |

## Jak ta droga działa

1. Wysyłasz `/update_panel` w Telegramie (albo przycisk „Aktualizuj Panel
   LCD”, albo komenda `update_panel` z panelu WWW).
2. Centrala pyta GitHuba o najnowsze wydanie i bierze jego tag.
3. Pobiera z tego wydania plik `firmware_panel.bin` na swój dysk.
4. W odpowiedziach dla panelu (`/api/status`) zaczyna podawać
   `"panel_ota_dostepna":true`.
5. Panel przy starcie widzi tę flagę, pobiera plik z centrali
   (`/panel-ota.bin`), zapisuje go do drugiej partycji i restartuje się.
6. Nowy firmware zgłasza centrali swoją wersję (`panel_fw=…`). Gdy jest
   równa tagowi, centrala zdejmuje flagę i pisze „Panel zaktualizowany
   do …”.

## Co sprawdziłem

| Co | Wynik | Jak |
|---|---|---|
| Wydania na GitHubie | 39 wydań, najnowsze `3.57.9`, w żadnym nie ma `firmware_panel.bin`. Adres tego pliku zwraca 404. | zapytania do GitHuba, takie same jak wysyła centrala |
| Odpowiedź GitHuba | Ma postać, której szuka centrala (`"tag_name":"3.57.9"`). | ta sama odpowiedź, na żywo |
| Kiedy panel zaczyna aktualizację | Tylko przy starcie, raz. Pracujący panel dostawał flagę przez 2 minuty i niczego nie pobrał. | symulator panelu, prawdziwy `main.cpp` |
| O co panel pyta centralę | `http://192.168.4.1:80/panel-ota.bin?token=<hasło OTA>` — adres, który centrala obsługuje. | symulator + kod centrali |
| Co panel robi po pobraniu | Restartuje się; po restarcie potwierdza nową partycję i zgłasza wersję. | symulator |
| Wersja równa tagowi | Centrala zdejmuje flagę, panel nie pobiera drugi raz i pracuje. | symulator |
| Wersja inna niż tag | Centrala nie zdejmuje flagi, panel pobiera i restartuje się znowu. | symulator; warunek w kodzie centrali |
| Nieudane pobieranie | Panel wraca do zwykłej pracy; następna próba po kolejnym restarcie. | symulator |
| Watchdog w czasie pobierania | Zadanie jest wypisane z watchdoga; 40 s pobierania nie kończy się restartem. | symulator |
| Hasło OTA | W archiwach panelu i centrali z repozytorium jest identyczne i nie ma znaków, które psułyby adres. | porównanie bez wypisywania hasła |
| Miejsce na dysku centrali | Centrala pobierze plik 1,5 MB tylko wtedy, gdy logi i pozostałe pliki zajmują najwyżej 2 907 951 B. | kod centrali wycięty i uruchomiony na PC |
| Wymagania biblioteki po stronie panelu | Kod 200, podany rozmiar pliku, pierwszy bajt `0xE9`, plik nie większy niż partycja. Centrala to wszystko spełnia. | źródła arduino-esp32 3.3.8 i ESPAsyncWebServer 3.10.3 |
| Zabezpieczenia przed złym plikiem | Obraz jest sprawdzany przed uruchomieniem (suma, układ). Gdy nowy firmware nie dojdzie do końca startu, bootloader wraca do poprzedniego. | źródła bibliotek, konfiguracja bootloadera |
| RAM panelu | W chwili startu aktualizacji wolne jest 49,7 kB (log v1.2.2). Potrzeba ok. 15 kB (policzone, nie zmierzone). | log płytki, źródła bibliotek |

Te same scenariusze uruchomione na źródłach v1.2.2 — czyli na tym, co jest
teraz na płytce — pokazują to samo zachowanie.

## Jak zrobić, żeby zadziałało

Bez zmian w firmware, sześć kroków:

1. **Zbuduj panel** w PlatformIO. Plik to
   `LCD-Centrali/.pio/build/esp32dev/firmware.bin`; do wydania ma trafić pod
   nazwą `firmware_panel.bin`.
2. **Sprawdź plik i tag**, zanim cokolwiek opublikujesz:

   ```
   python3 narzedzia/ota_panelu_sprawdz.py --tag v1.3.0 --bin LCD-Centrali/.pio/build/esp32dev/firmware.bin
   ```

   Tag musi być tym samym tekstem co `FIRMWARE_VERSION_PANEL` w `main.cpp`
   panelu (dziś `v1.3.0` — stąd przykład wyżej).
3. **Opublikuj osobne wydanie** z tym tagiem i tym jednym plikiem. Nie
   dołączaj `firmware_panel.bin` do wydania centrali z innym tagiem.
   Wspólne wydanie dla obu urządzeń jest możliwe tylko wtedy, gdy panel
   został zbudowany z wersją równą tagowi tego wydania.
4. **Zwolnij miejsce na centrali**: logi poniżej ok. 2,9 MB (przycisk
   „Wyczyść logi”). Inaczej centrala odpowie „Brak miejsca na bezpieczny
   staging panelu”.
5. **Wyślij `/update_panel`**, gdy nie ma alarmu. Poczekaj na „Firmware
   panelu (…) gotowy.”.
6. **Wyłącz i włącz zasilanie panelu.** Ekran będzie ciemny przez około
   minutę (start, pobieranie, restart, start). Potem centrala napisze „Panel
   zaktualizowany do ….”. Gdy tej wiadomości nie ma po kilku minutach,
   a ekran jest dalej ciemny, to pułapka 1 — niżej jest wyjście.

Dwie uwagi do tej procedury:

- **v1.3.0 wgraj kablem, a tę drogę wypróbuj przy następnej wersji.** v1.3.0
  nie była jeszcze uruchomiona na płytce. Gdyby pierwsza aktualizacja przez
  centralę niosła od razu nie sprawdzony firmware, przy kłopotach nie byłoby
  wiadomo, co zawiniło.
- **Po takim wydaniu `/update` centrali odmówi.** Najnowszym wydaniem będzie
  `v1.3.0`, które centrala uzna za starsze od własnej wersji. Wraca do normy
  po następnym wydaniu centrali.

## Pułapki

### 1. Wersja inna niż tag: panel wgrywa plik bez końca

Centrala porównuje tekst zgłoszony przez panel z tagiem wydania znak po
znaku (`zgloszona == g_panelOtaWersja`). `v3.58.6` i `3.58.6` to dla niej
dwie różne wersje. W dzienniku zmian centrali przy wersji 2.2.3 jest
adnotacja, że porównywanie wersji panelu zwykłą równością tekstów to ta sama
klasa błędu, którą wtedy naprawiono w `/update`, i że dla panelu nie została
poprawiona.

Co widać, gdy to się stanie:

- panel ma ciemny ekran i nie dochodzi do kafelków;
- po „Firmware panelu … gotowy” centrala milczy — nie ma ani „Panel
  zaktualizowany”, ani ostrzeżenia;
- `/update` i `/update_panel` odpowiadają „Inna aktualizacja (centrali lub
  panelu) jest w trakcie / czeka na potwierdzenie”;
- nadzór zadań sieciowych centrali (restart, gdy zadania Telegrama milczą
  5 minut) jest przez cały ten czas wyłączony;
- stan przetrwa restart centrali — jest zapisany w jej pamięci ustawień.

**Jak z tego wyjść bez kabla.** Otwórz w przeglądarce, z sieci centrali:

```
http://<adres centrali>/api/status?panel_fw=<tag z wiadomości „Firmware panelu (...) gotowy”>
```

Centrala potraktuje to jak zgłoszenie panelu: zdejmie flagę, odblokuje
`/update` i napisze „Panel zaktualizowany do …”. Panel przy następnym
starcie zobaczy brak flagi i uruchomi się normalnie. To wynika z kodu
obsługi `/api/status`; nie sprawdziłem tego na płytce. Tym samym adresem
można odwołać aktualizację, która czeka na restart panelu.

### 2. Panel nie zaczyna sam — trzeba go wyłączyć i włączyć

Centrala pisze „Zainstaluje się automatycznie przy najbliższym wybudzeniu
panelu (przycisk)”. To dotyczy starego ekranu, który usypiał. W obecnej
wersji z LVGL panel nie usypia i flagę aktualizacji sprawdza tylko przy
starcie. Dopóki panel nie zostanie zrestartowany, centrala czeka — ze
skutkami z punktu 1 (blokada `/update`, wyłączony nadzór), choćby przez
wiele dni.

### 3. Miejsce na dysku centrali

Przed pobraniem centrala wymaga, żeby na dysku zostało miejsce na wszystkie
swoje rezerwy i na nowy plik. W praktyce: logi i pozostałe pliki najwyżej
4 407 951 B minus rozmiar firmware'u panelu, czyli ok. 2,9 MB dla pliku
1,5 MB. Logi rosną do 8,2 MB, zanim centrala je wyśle i skasuje, więc przez
większą część tego cyklu `/update_panel` dostanie odmowę. Komunikat podaje
dokładne liczby. Wyczyszczenie logów wystarcza.

### 4. Ciemny ekran przez cały czas pobierania

Panel zaczyna pobierać, zanim cokolwiek narysuje, i nie pokazuje żadnego
napisu. Przez około minutę wygląda jak wyłączony. Wyłączenie go w tym
czasie nie szkodzi: stary firmware zostaje, a próba powtórzy się przy
następnym starcie.

### 5. Zerwane pobieranie może zatrzymać panel na długo

Gdy połączenie zamilknie w połowie pliku bez błędu, biblioteka próbuje dalej
300 razy po 8 sekund — do ok. 40 minut. Watchdog jest na ten czas wyłączony,
więc panel sam się nie zrestartuje. Przy błędzie połączenia kończy się to po
ok. 30 sekundach. Limit 20 sekund ustawiony w kodzie panelu nie działa:
biblioteka nadpisuje go swoim. To wynika ze źródeł biblioteki; nie
sprawdziłem tego na płytce.

### 6. Aktualizacja zaczyna się także w czasie alarmu

Centrala nie przyjmie `/update_panel` w trakcie alarmu. Ale gdy plik już
czeka, panel zacznie pobierać przy każdym starcie, także wtedy, gdy centrala
zgłasza alarm. Przez czas pobierania ekran jest ciemny, a brzęczyk nie gra
wzorca alarmu — zostaje w stanie, w jakim był w chwili rozpoczęcia.

### 7. Centrala sprawdza w pliku tylko pierwszy bajt

Plik innego projektu na ESP32 przeszedłby przez centralę i zostałby wgrany
do panelu. Firmware centrali (ESP32-S3) nie przejdzie: jest większy niż
limit 2 MiB, a panel odrzuciłby go przy sprawdzaniu obrazu.

## Czego nie sprawdziłem

- **Niczego na płytkach.** Pobierania pliku z GitHuba do pliku na centrali,
  wysyłania go panelowi, zapisu do flash, restartu i cofania wersji nie da
  się uruchomić na PC. Log panelu pokazuje partycję wgraną kablem
  (`partycja=app0 stan=-1`), a w żadnym wydaniu nie ma pliku panelu — nie
  widzę śladu, żeby ta droga była tu kiedykolwiek użyta.
- **Rozmiar firmware'u panelu.** Musi być najwyżej 2 031 616 B. Poda go
  PlatformIO przy budowaniu i narzędzie z opcją `--bin`.
- **Ile zajmują logi na Twojej centrali** — pokaże adres `/api/fs-list`.
- **Czy hasło OTA w obu dzisiejszych plikach `secrets.h` jest takie samo.**
  W archiwach z repozytorium jest; Twoich obecnych plików nie mam.
- **Stos i czas pobierania na płytce.**
- **Narzędzie `ota_panelu_sprawdz.py` z opcją `--bin`** sprawdziłem tylko na
  sztucznych plikach; prawdziwego pliku z PlatformIO tu nie mam.

## Jak powtórzyć

Sześć scenariuszy, na których opiera się ten raport (`ota_start`,
`ota_po_restarcie_zgodna`, `ota_po_restarcie_inna_wersja`,
`ota_flaga_w_trakcie_pracy`, `ota_nieudane`, `ota_przy_alarmie`), opisywało
stary kod. Po zmianach zastąpiło je dwanaście nowych. Stary stan da się
odtworzyć z commita `1c5818d` tego repozytorium:

```
git checkout 1c5818d
cd LCD-Centrali/testy_host
./uruchom_testy.sh ota_po_restarcie_inna_wersja     # i pozostałe pięć
python3 ../../narzedzia/ota_panelu_sprawdz.py
```

Tam cały zestaw to 41 scenariuszy i 420 sprawdzeń, także z AddressSanitizer
i w modelu 32-bitowym. W symulatorze z tamtego commita samo pobieranie jest
udawane: sprawdzane jest, kiedy panel zaczyna, o co pyta i co robi potem.

## Co proponowałem zmienić

1. **Panel zaczyna aktualizację sam**, w trakcie pracy, bez wyłączania
   zasilania. Tylko gdy nie ma alarmu, i z napisem „Aktualizacja…” na
   ekranie.
2. **Potwierdzenie niezależne od numeru wersji.** Panel po wgraniu zgłasza
   centrali, który plik wgrał, zamiast liczyć na to, że numer wersji będzie
   równy tagowi. Pętla z pułapki 1 przestaje być możliwa.
3. **Ten sam plik nie jest wgrywany drugi raz.** Panel pamięta przez
   restart, co właśnie wgrał.
4. **Limit czasu na zerwane pobieranie**: kilkanaście sekund ciszy kończy
   próbę.
5. **Centrala nie czeka bez końca.** Oczekiwanie na panel ma limit czasu
   i komendę odwołania, nie wyłącza nadzoru i nie blokuje `/update`.
   Komunikat mówi „zrestartuj panel”, a nie „przycisk”.

## Co zostało zrobione

Po odpowiedzi „Działaj” (2026-11-12): firmware panelu **v1.4.0**
(`LCD-Centrali/`) i centrali **3.58.6** (`firmware-patch/v3.58.6-panel-ota/`).
Wszystkie pięć punktów powyżej jest zrobionych.

| Pułapka z tego raportu | Po zmianach |
|---|---|
| 1. Wersja inna niż tag: panel wgrywa plik bez końca | Centrala potwierdza po **pliku**: z sumy SHA-256 na jego końcu bierze identyfikator, a panel zgłasza identyfikator programu, z którego pracuje. Tag wydania i numer wersji nie muszą być równe. Panel pamięta, co wgrał, i tego samego pliku nie wgrywa drugi raz — także ze starszą centralą. |
| 2. Panel nie zaczyna sam | Panel v1.4.0 zaczyna w trakcie pracy, zwykle w ciągu minuty. Czekająca oferta nie blokuje `/update` i nie wyłącza nadzoru. Kończy się sama: potwierdzenie, odmowa panelu, 24 h albo nowa komenda `/update_panel_anuluj`. O każdym zakończeniu przychodzi wiadomość. |
| 3. Miejsce na dysku centrali | Bez zmian. Limit został; komunikat centrali i narzędzie podają liczby. |
| 4. Ciemny ekran przez cały czas pobierania | Napis `AKTUALIZACJA PANELU`, pasek postępu, procenty. |
| 5. Zerwane pobieranie może zatrzymać panel na długo | Panel pobiera plik własnym kodem, po kawałku na obrót pętli. 15 s ciszy kończy próbę, cała próba trwa najwyżej 5 minut, po trzech nieudanych panel zgłasza odmowę z przyczyną. Watchdog pilnuje pętli przez cały czas. |
| 6. Aktualizacja zaczyna się także w czasie alarmu | W czasie alarmu centrala nie podaje ani oferty, ani pliku, a panel nie zaczyna. Alarm w trakcie przerywa pobieranie. |
| 7. Centrala sprawdza w pliku tylko pierwszy bajt | Centrala odrzuca plik, który nie jest programem dla ESP32 z sumą na końcu (firmware centrali nie przejdzie). Panel sprawdza nagłówek, rozmiar i to, czy plik jest tym z oferty; cały obraz sprawdza przed uruchomieniem ESP-IDF. |

Co się nie zmieniło:

- **Dziś `/update_panel` nadal kończy się brakiem pliku** — żadne wydanie na
  GitHubie nie ma `firmware_panel.bin`. Centrala 3.58.6 pisze to wprost.
- **Panel trzeba raz wgrać kablem.** Na płytce jest v1.2.2 ze starym kodem
  aktualizacji; nowy zaczyna działać dopiero od v1.4.0.
- **Nic z tego nie było budowane w PlatformIO ani uruchamiane na płytkach.**
  Sprawdzenia są na PC: prawdziwy firmware panelu uruchomiony przeciwko
  kodowi decyzji wyciętemu z firmware'u centrali. Pobranie pliku z GitHuba,
  zapis do pamięci programu, bootloader i powrót do poprzedniego programu
  sprawdzi dopiero pierwsza próba.

Jak teraz zrobić aktualizację: `LCD-Centrali/README.md`, rozdział
„Aktualizacja przez centralę i Telegram (OTA)”. Wgranie centrali:
`firmware-patch/v3.58.6-panel-ota/CZYTAJ-NAJPIERW.txt`. Sprawdzenie pliku
przed publikacją wydania:

```
python3 narzedzia/ota_panelu_sprawdz.py --tag <tag wydania> --bin LCD-Centrali/.pio/build/esp32dev/firmware.bin
```

Testy nowego kodu:

```
cd LCD-Centrali/testy_host && TYLKO='^ota_' ./uruchom_testy.sh      # panel: 12 scenariuszy
cd firmware-patch/v3.58.6-panel-ota/tests && ./uruchom_testy.sh     # centrala
```
