# LCD: zasięg do centrali i niezależny wskaźnik internetu

Ta poprawka zastępuje wcześniejszy wariant, który pokazywał na LCD zasięg centrali do routera i flagę Firebase. Składa się z **dwóch patchy**: dla panelu z `LCD Centrali.zip` i dla centrali na bazie `firmware-patch/v3.54.0-offline-buffer-toggle/main_centrala.cpp`. Oba urządzenia muszą zostać zbudowane i zaktualizowane. Archiwa nie zostały zmodyfikowane.

## Działanie

- **Kreski** pokazują lokalny pomiar `WiFi.RSSI()` panelu — siłę sygnału AP centrali odbieranego przez LCD, nie sygnał centrali do domowego routera. Skala to 7 kresek: -95, -89, -83, -77, -71, -65 i -55 dBm. Pomiar jest odświeżany co sekundę również wtedy, gdy lokalne Wi-Fi działa, ale zapytanie HTTP do centrali się nie powiedzie. `X` oznacza brak połączenia Wi-Fi LCD; `?` oznacza niepewny pomiar (np. RSSI=0).
- **Globus** pokazuje nowy wynik `internet_wan` przekazywany przez centralę w `/api/status`: kolorowy = próba udana, szary z `X` = próba nieudana lub brak połączenia centrali z routerem, szary z `?` = brak aktualnego/wiarygodnego wyniku. Utrata kontaktu HTTP LCD–centrala powoduje `?` na globusie, ale nie uniemożliwia pomiaru kresek, jeśli Wi-Fi LCD pozostaje połączone.
- Centrala odpytuje niezależnie od Firebase dwa publiczne resolvery DNS (Cloudflare 1.1.1.1, awaryjnie Google 8.8.8.8) o rekord A `example.com`. Sprawdza odpowiedź i jej źródło, nie wysyła haseł ani danych kotła. Jeden cykl zaczyna się co 15 s od zakończenia poprzedniego; timeout to 2 s na serwer. UDP jest nieblokujące, bez osobnych tasków i bez `getaddrinfo`; źródłowy adres należy do interfejsu STA. Przeterminowany wynik (>45 s), rozruch i błąd zasobów lokalnych to `null`/`?`.
- **Ograniczenie:** to rzeczywisty test osiągalności tych zewnętrznych usług DNS, nie gwarancja działania każdej strony, HTTPS czy Firebase. Blokada UDP 53 lub obu resolverów może dać `X` mimo działającego internetu; DNS bez TLS może być przechwycony. Test niczego nie uwierzytelnia i nie służy do decyzji bezpieczeństwa. Istniejące pole `internet` w API pozostaje bez zmian i nadal oznacza gotowość Firebase. Starsza centrala nie ma `internet_wan`, więc nowy LCD pokaże `?`, zamiast mylić Firebase z internetem.

## Przygotowanie panelu

Przykład Linux/macOS z katalogu głównego repozytorium, dla nowego pustego katalogu (wymaga `unzip`, `git`):

```sh
REPO="$PWD"
mkdir ../LCD-panel-poprawiony
unzip "LCD Centrali.zip" -d ../LCD-panel-poprawiony
cd ../LCD-panel-poprawiony
git apply --check "$REPO/firmware-patch/panel-lcd-connectivity/lcd-connectivity.patch"
git apply "$REPO/firmware-patch/panel-lcd-connectivity/lcd-connectivity.patch"
cp "$REPO/firmware-patch/panel-lcd-connectivity/src/SiecIkony.h" src/
```

Windows PowerShell w katalogu głównym repozytorium (wymaga `git`):

```powershell
$repo = (Get-Location).Path
$panel = Join-Path (Split-Path $repo) 'LCD-panel-poprawiony'
# Użyj nowego, pustego katalogu.
Expand-Archive -LiteralPath (Join-Path $repo 'LCD Centrali.zip') -DestinationPath $panel
$patch = Join-Path $repo 'firmware-patch/panel-lcd-connectivity/lcd-connectivity.patch'
git -C $panel apply --check $patch
if ($LASTEXITCODE -ne 0) { throw 'Patch nie pasuje do źródeł LCD' }
git -C $panel apply $patch
if ($LASTEXITCODE -ne 0) { throw 'Nie udało się zastosować patcha LCD' }
Copy-Item (Join-Path $repo 'firmware-patch/panel-lcd-connectivity/src/SiecIkony.h') (Join-Path $panel 'src')
```

Sprawdź ustawienia połączenia i sprzętu, otwórz kopię w PlatformIO i wykonaj build panelu. Domyślne `LVGL_POC_TEST=1`, `LVGL_POC_SYMULACJA=0`; symulacja nie mierzy realnej sieci. Numer wersji panelu nie został zmieniony — nie zakładaj, że automatyczne OTA rozpozna poprawkę jako nową wersję.

## Przygotowanie centrali

Użyj kopii istniejącego projektu PlatformIO centrali przygotowanego **dokładnie** z v3.54.0, w tym jego `platformio.ini`, własnej konfiguracji i plików `secrets.h`. Szczegóły przygotowania bazy oraz wymagane nagłówki są w `firmware-patch/v3.54.0-offline-buffer-toggle/README.md`. Nie zastępuj swoich sekretów wartościami przykładowymi ani nie wysyłaj ich w czacie.

Linux/macOS, po ustawieniu zmiennych na katalog główny repo i kopię projektu centrali:

```sh
REPO="/sciezka/do/Sterownik-Pieca-C.O."
CENTRALA="/sciezka/do/kopii-projektu-centrali"
git -C "$CENTRALA" apply --check "$REPO/firmware-patch/panel-lcd-connectivity/centrala-connectivity.patch"
git -C "$CENTRALA" apply "$REPO/firmware-patch/panel-lcd-connectivity/centrala-connectivity.patch"
cp "$REPO/firmware-patch/panel-lcd-connectivity/centrala/InternetProbe.h" "$CENTRALA/src/"
```

PowerShell, po ustawieniu tych samych ścieżek:

```powershell
$repo = 'C:\sciezka\do\Sterownik-Pieca-C.O.'
$centrala = 'C:\sciezka\do\kopii-projektu-centrali'
$patch = Join-Path $repo 'firmware-patch/panel-lcd-connectivity/centrala-connectivity.patch'
git -C $centrala apply --check $patch
if ($LASTEXITCODE -ne 0) { throw 'Patch wymaga dokładnej bazy v3.54.0; nie wgrywaj' }
git -C $centrala apply $patch
if ($LASTEXITCODE -ne 0) { throw 'Nie udało się zastosować patcha centrali' }
Copy-Item (Join-Path $repo 'firmware-patch/panel-lcd-connectivity/centrala/InternetProbe.h') (Join-Path $centrala 'src')
```

Zbuduj oba firmware przed ich wgrywaniem. Wgrywaj centralę tylko w bezpiecznych warunkach, zachowując sprawdzoną możliwość powrotu do poprzedniej wersji. Sama zmiana w repozytorium **nie aktualizuje urządzeń**.

## Testy i ograniczenia

Hostowy test stosuje oba patche do odpowiednich baz, używa ArduinoJson i kompiluje faktyczną funkcję rysowania z atrapami LVGL. Sprawdza progi, osobne działanie zasięgu i internetu, brak/niepoprawny `internet_wan`, stary status Firebase, utratę i odzyskanie łączności. Test centrali obejmuje prawdziwy parser pakietu DNS i automat prób przez atrapę transportu, timeouty obu serwerów, zmianę IP, błędy lokalne i zawinięcie `millis()`.

```sh
# Potrzebne python3, g++, patch i ArduinoJson 7.
curl -fsSL https://codeload.github.com/bblanchon/ArduinoJson/tar.gz/refs/tags/v7.2.1 | tar xz -C /tmp
ARDUINOJSON_SRC=/tmp/ArduinoJson-7.2.1/src python3 firmware-patch/panel-lcd-connectivity/tests/run_tests.py
```

Testy hostowe przeszły. Nie potwierdzają poprawnej kompilacji i działania firmware na ESP32, Wi-Fi ani faktycznego połączenia z resolverami. Wcześniejsza próba pełnego builda LCD w tym środowisku zakończyła się błędem pobrania platformy `espressif32`; tej wersji nie zbudowano na PlatformIO ani nie przetestowano sprzętowo.

### Próby po wgraniu

1. LCD blisko centrali i dalej od niej: liczba kresek powinna odpowiadać lokalnemu RSSI LCD, niezależnie od siły sygnału centrali do routera.
2. Pozostaw LCD połączone z AP centrali, odłącz jej router od WAN: po czasie dwóch timeoutów i cyklu pomiaru globus powinien wskazać `X`; kreski LCD mają pozostać. Po przywróceniu internetu poczekaj na kolejny cykl i potwierdź kolorowy globus.
3. Sprawdź `/api/status`: nowe `internet_wan` zmienia się między `true`, `false` i `null`; stare `internet` jest nadal flagą Firebase. Testuj też z niedostępnym Firebase, ale działającymi resolverami.
4. Odłącz LCD od AP centrali: kreski `X`, globus `?`. Po odzyskaniu połączenia stan powinien wrócić po odczycie statusu. Sprawdź też uruchomienie LCD bez centrali i brak odpowiedzi HTTP przy nadal połączonym Wi-Fi.

## Źródła

- [Espressif — Wi-Fi API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_wifi.html): RSSI stacji dotyczy połączonego AP, a wyjątkowo może być dodatnie.
- [Espressif — implementacja `WiFiSTA::RSSI`](https://github.com/espressif/arduino-esp32/blob/2479efb10d2709246a41562b8a626c7ed14ee164/libraries/WiFi/src/WiFiSTA.cpp): zero także przy błędzie odczytu AP.
- [Espressif — nieblokujące gniazda i timeouty](https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/protocols/lwip.html).
- [FirebaseClient — przykład `FirebaseApp::ready()`](https://github.com/mobizt/FirebaseClient/blob/main/examples/BareMinimum/RealtimeDatabase/RealtimeDatabase.ino): gotowość uwierzytelnienia nie jest niezależnym testem internetu.
