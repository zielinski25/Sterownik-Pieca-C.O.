# Głęboki audyt statyczny: bezpieczeństwo, pamięć i bateria aplikacji Android

- **Data:** 2026-10-10
- **Zakres:** lokalne źródła `android/` oraz — wyłącznie jako sąsiadujący interfejs sieciowy — bieżące archiwum `Cenatrala Pieca.zip`.
- **Wynik w skrócie:** kod ma sensowne sprzątanie głównych zasobów i kilka jawnych limitów pamięci, ale nie ma podstaw, by uznać aplikację za wolną od wycieków lub zoptymalizowaną energetycznie. Są też poważne problemy bezpieczeństwa w poświadczeniach i lokalnym HTTP. **To audyt statyczny, nie certyfikacja ani test urządzenia.**

> Raport nie zawiera wartości haseł, tokenów ani kluczy. Opis archiwum firmware dotyczy sprawdzonego źródła; **nie potwierdza**, jaki firmware znajduje się obecnie na piecu. Nie łączyłem się z urządzeniem, nie zmieniałem jego stanu, nie symulowałem alarmu i niczego nie wgrywałem.

## 1. Zakres i ograniczenia

Sprawdzono manifest, konfigurację cleartext/backup, źródła uwierzytelniania i preferencji, klienta RTDB/OkHttp, Direct Boot, cykle aktywności i usługi, powiadomienia, widgety, historię wykresów, pogodę, logi, kolekcje w pamięci oraz lokalne endpointy ESP32 widoczne w archiwum.

- Wariant Androida ma `minSdk 26`, `targetSdk 35`; release ma `isMinifyEnabled = false` (`android/app/build.gradle.kts`). Brak minifikacji nie jest ochroną sekretów.
- W repozytorium nie znaleziono reguł Firebase RTDB ani konfiguracji App Check. Nie mam dostępu do konsoli projektu, więc rzeczywistych reguł serwerowych nie da się tu ocenić.
- Nie było APK/AAB do dekompilacji ani środowiska Java/Android SDK/ADB. Nie uruchomiłem Gradle, testów na emulatorze, profilera heap/baterii, przechwytywania ruchu ani testów na piecu. Czas odpowiedzi/zużycie baterii i faktyczna ekstrakcja DEX pozostają niezmierzone.
- Dotychczasowy, zgłoszony przez użytkownika build Windows dotyczył wcześniejszego stanu źródeł; nie stanowi kompilacji nowego zasobu ikony z tej zmiany.
- Wcześniejsze zgłoszenia użytkownika o Direct Boot i wyświetleniu alarmu debug są tylko kontekstem z poprzednich prób, nie powtórzonym testem tego audytu ani potwierdzeniem fizycznego czujnika/wyjść. Nie restartowałem telefonu, nie symulowałem alarmu i nie sprawdzałem ani nie zmieniałem bieżącego stanu urządzenia; ostatni status `ALARM` nie jest przeze mnie potwierdzony jako wyczyszczony.

## 2. Najważniejsze ustalenia bezpieczeństwa

### S1 — Wysokie: wspólne poświadczenia są zapisane w kodzie aplikacji i firmware

`Prefs.kt` zawiera stałe związane z domyślnym hasłem konta Firebase i tokenem komend; domyślny token komend jest używany przez klienta RTDB. Analogiczne wartości występują w sprawdzonym źródle firmware. Stała Firebase API key jest identyfikatorem klienta, a nie hasłem — nie należy mylić jej z sekretem. Natomiast wspólne hasło i token komend trzeba traktować jako możliwe do odzyskania ze źródła, a przy obecnym release bez minifikacji także jako możliwe do odnalezienia w APK. Nie wykonywałem ekstrakcji APK, więc nie twierdzę, że zweryfikowałem binarkę.

**Skutek:** osoba znająca współdzielone poświadczenie może próbować zalogować się do Firebase lub wysłać komendy; faktyczny zasięg zależy od nieznanych reguł RTDB, uprawnień konta i walidacji w firmware. Klucz API sam nie daje autoryzacji, ale jego ograniczenia również nie zastępują reguł bazy.

**Zalecenie:** przed uznaniem wydania za bezpieczne zaplanować skoordynowaną rotację/usunięcie współdzielonych haseł i tokenu, z uwzględnieniem firmware i provisioningu urządzeń. Nie zmieniać wyłącznie stałej po stronie Androida — mogłoby to zerwać działanie. Reguły RTDB powinny ograniczać odczyt i zapis do minimalnych ścieżek/operacji, a polecenia powinny mieć autoryzację per użytkownik lub krótko żyjący mechanizm, nie wspólny sekret w kliencie.

W szerszym, wcześniejszym przeglądzie źródeł firmware/LCD odnotowano też statyczne dane dostępowe SoftAP i OTA w materiałach projektu. Wartości pomijam; należy je uwzględnić w tym samym planie rotacji/provisioningu. To nadal ustalenie źródłowe, nie potwierdzenie bieżącej konfiguracji urządzenia.

### S2 — Wysokie, warunkowe: lokalne endpointy firmware nie mają widocznej autoryzacji

W lokalnym `Cenatrala Pieca.zip`, `src/main_centrala.cpp`, handlery m.in. `/api/wifi/add`, `/api/wifi/delete`, `/api/telegram/save`, `/api/ustawienia-set`, `/api/servo-manual`, `/api/pompa-manual`, `/api/mieszadlo-manual` i `/api/symulacja-czujnik` walidują część parametrów, ale w samych handlerach nie znalazłem kontroli uwierzytelnienia. W tym samym pliku sprawdzenie hasła występuje dla osobnego endpointu pobrania OTA, nie dla wymienionych API. Część endpointów zmienia konfigurację lub zleca sterowanie wyjściami. To **analiza źródła**, nie test żądania sieciowego ani dowód, że ten kod jest wgrany na pracującym sterowniku.

Dla części `/api/wifi/*` i `/api/telegram/*` są także odpowiedzi `Access-Control-Allow-Origin: *` oraz `Access-Control-Allow-Private-Network: true`. Część ścieżek ma obsługę OPTIONS, ale nie wszystkie; możliwość ataku z obcej strony WWW zależy od zachowania konkretnej przeglądarki i preflightu, więc nie uznaję jej za potwierdzoną runtime. Brak autoryzacji dla klienta w tej samej sieci nie zależy jednak od CORS.

**Zalecenie:** potraktować lokalne API jako osiągalne przez niezaufanego klienta LAN, dopóki nie zostanie dodana autoryzacja po stronie ESP32. Ograniczenie CORS i rezygnacja z wildcard/PNA, gdzie nie są niezbędne, są dodatkową warstwą — same nie zastępują uwierzytelnienia. Dla endpointów zmieniających pracę urządzenia wymagany jest także przegląd bezpieczeństwa i próby na odizolowanym stanowisku; nie wykonywałem ich na żywym piecu.

### S3 — Wysokie: HTTP cleartext jest globalnie dozwolony, a adres ESP32 nie jest ograniczany

`AndroidManifest.xml` ustawia `usesCleartextTraffic="true"`; `network_security_config.xml` zezwala na cleartext w `base-config`. Jest to szersze niż pojedynczy lokalny endpoint. `RtdbClient.espUrl()` odrzuca tylko pusty adres i konkatenacją tworzy `http://<ip>…`. Pole `S.ip` pochodzi ze statusu RTDB i nie jest walidowane jako prywatny literalny adres. Dwa wywołania Telegrama w `AppModel.kt` składają adres bezpośrednio, z pominięciem helpera.

**Skutek:** WPA/hasło Wi-Fi przy `/api/wifi/add` oraz token bota/chat ID przy `/api/telegram/save` mogą być przesyłane po niezaszyfrowanym HTTP. Osoba z możliwością podsłuchu w sieci może je odczytać. Gdy pole `ip` zostanie podmienione w źródle statusu, późniejsza, zainicjowana przez użytkownika akcja lokalna może wysłać dane pod inny host. Nie jest to dowód na automatyczne wysyłanie sekretów w tle.

**Zalecenie:** docelowo TLS lub inny uwierzytelniony i integralny protokół lokalny; do czasu jego wdrożenia scentralizować budowanie adresów, odrzucać hosty publiczne/hostname’y/nieoczekiwane porty i dopuścić wyłącznie zatwierdzony zakres LAN. Sama walidacja prywatnego IP nie szyfruje ruchu i nie chroni przed innym urządzeniem w tej samej sieci.

### S4 — Wysokie: token Telegrama i token komend są przechowywane oraz logowane jak zwykłe dane

Hasło logowania ma osobną ścieżkę AES-GCM/Android Keystore (`setSecret`), ale `K_TG_TOKEN` i `K_CMD_TOKEN` korzystają ze zwykłego `Prefs.set()` w CE `SharedPreferences`. Dodatkowo, gdy Telegram konfigurowany jest przy aktywnym połączeniu, `System.kt` składa `tg_config <token> <chatId> …` i wysyła go przez `m.send()`. `sendAwait()` wpisuje całą treść komendy do toastu i do listy logu aplikacji przy wysyłce oraz ACK/NACK. Ta lista jest ograniczona do 250 wpisów i jest pamięciowa, nie Logcat, ale sekret pozostaje w UI/pamięci do usunięcia wpisu lub zniszczenia modelu. Sama komenda trafia też do węzła RTDB `/piec/cmd`; jego reguły, retencja i historia nie są znane. Gdy Firebase jest rozłączony, alternatywna ścieżka `tgSave()` przesyła token do ESP32 po HTTP i zapisuje go w preferencjach.

**Zalecenie:** nie przesyłać sekretu bota jako tekstu polecenia; oddzielić kanał konfiguracji i zabezpieczyć go po stronie urządzenia. Redagować sekrety przed toastem/logiem/ACK i nie zapisywać ich do bufora diagnostycznego. Migrować token bota i własny token komend do `setSecret` z bezpieczną migracją starszych wartości.

### S5 — Średnie: tokeny RTDB trafiają do query URL; kopie CE nie są szyfrowane przez aplikację

`Rtdb.authQuery()` dodaje ID token do `?auth=…` przy GET/SSE i żądaniach zapisu. Nie znalazłem `HttpLoggingInterceptor` ani jawnego logowania URL/tokenu, ale token w query jest bardziej narażony na zapis w diagnostyce proxy/infrastruktury niż nagłówek autoryzacji. ID/refresh token w CE `SharedPreferences` są przechowywane jako zwykłe wpisy aplikacji; kopia Direct Boot w DE jest osobno zabezpieczona AES-GCM kluczem Android Keystore z AAD nazwą pakietu.

Manifest ma `allowBackup="true"`, natomiast oba pliki reguł wskazują `piec_co_prefs.xml`; rzeczywista nazwa pliku z `Prefs` to `piec_hmi`. To niespójność, nie potwierdzony wyciek — nie sprawdzono backupu na urządzeniu. Jeśli ścieżka zostanie poprawiona bez separacji/wykluczenia sekretów, ten sam plik zawiera także wrażliwe preferencje.

**Zalecenie:** nie zamieniać mechanicznie `auth=<Firebase ID token>` na nagłówek Bearer — dla Firebase ID tokenów query jest standardową ścieżką REST, a Bearer dotyczy innego typu poświadczenia; każdą zmianę transportu testować na bazie testowej. Nie logować ani nie udostępniać pełnych URL-i. Rozdzielić sekrety do osobnego pliku preferencji, by reguły backupu mogły go wykluczyć; jawnie zdecydować, czy aplikacja w ogóle ma wspierać backup. Zweryfikować cloud backup i transfer urządzenie–urządzenie, zamiast wnioskować tylko z XML.

### Pozytywne zabezpieczenia zaobserwowane statycznie

- Direct Boot zapisuje kopię sesji jako AES-GCM w device-protected storage; logout najpierw ustawia tombstone, potem czyści kopie tokenów i klucz/ciphertext oraz zatrzymuje usługę i widget. To dobry porządek operacji.
- Aktywność alarmowa, receiver i serwis nie są eksportowane; PendingIntenty są immutable, a akcje alarmowe sprawdzają nonce. Broadcasty ekranów są rejestrowane jako `RECEIVER_NOT_EXPORTED`.
- Nie znalazłem loggera całych żądań HTTP ani jawnego logowania haseł/tokenów do Logcat. Wyjątkiem jest opisany wyżej log **w pamięci/UI** z treścią komendy.
- Firebase API key jest publicznym identyfikatorem projektu, nie sekretem autoryzującym. Najważniejsze są reguły bazy i autoryzacja operacji.

## 3. Bateria i aktywność sieciowa

**Wniosek statyczny:** obecny projekt stawia niską latencję alarmu i pracę przed odblokowaniem ponad minimalny ruch sieciowy. W trybie całodobowego czuwania nie nazwałbym go „optymalnym bateriowo”. Rzeczywiste zużycie zależy od telefonu, Wi-Fi/LTE, Doze, ROM-u i siły sygnału; nie było pomiaru mAh.

### Ruch w tle

- `AlarmMonitorService` utrzymuje SSE, ale równolegle wykonuje fallback GET co `30 s` (`FALLBACK_POLL_MS`) i wymusza ponowne zestawienie SSE co `3 min` (`STREAM_MAX_MS`). W stabilnej pracy oznacza to około **120 fallback GET/h + 20 początkowych GET/h po reconnect = ~140 GET/h**, plus ruch samego SSE/keepalive. Przy błędach i backoffie liczba może się zmienić. To wyliczenie z interwałów źródłowych, nie pomiar.
- Gdy aplikacja jest widoczna, `AppModel` odpytuje status co `4 s` — do **~900 żądań/h** przy ciągłej pracy ekranu — równolegle do SSE i fallbacku serwisu. `pollMutex` zapobiega nakładaniu się części odczytów, ale nie usuwa ich przy normalnej szybkiej odpowiedzi.
- Część tych żądań jest celowym kompromisem dla szybkiego alarmu i świeżego dashboardu. Nie należy usuwać fallbacku ani wydłużać interwału w ciemno, zanim niezawodność nie zostanie zweryfikowana na odizolowanym stanowisku.

### Zbędna praca możliwa do ograniczenia

- `AppModel.onResume()` wywołuje `loadRange(rangeSec)` bez sprawdzenia, czy aktualnie pokazana jest strona wykresu. Po wznowieniu aplikacji może więc pobierać historię RTDB, gdy użytkownik jest na dashboardzie. Zakres może być zapisanym zakresem aż do 30 dni. Przy próbkach co minutę 30 dni to do ok. 43 200 rekordów i około 150 stron po 300 rekordów w zwykłej ścieżce indeksowanej; bez indeksu kod przechodzi na pobieranie całych shardów dziennych. To potencjalnie istotny transfer, czas i peak heap przy każdym wznowieniu.
- `AppModel.init` i `onResume` oba uruchamiają `refreshWeather(false)`. Funkcja nie ma własnej blokady ani cache wieku danych; pierwsze wznowienie może zrobić dwa równoległe pobrania. `false` steruje głównie wskaźnikiem busy, nie pomija żądania. Dodatkowo jest timer co 15 min podczas widoczności.
- Fallback usługi nadal działa, gdy aktywność wykonuje polling statusu — ruch jest częściowo zduplikowany.
- Widget pomija niezmienioną sygnaturę przez 60 s, ale **każda zmiana** temperatury/statusu omija ten limit. Jeśli sterownik często publikuje zmienne wartości, `RemoteViews` i SharedPreferences mogą być aktualizowane często. Zmiana flagi alarmowej powinna pozostać natychmiastowa; można osobno ograniczyć częste zmiany telemetryczne.

### Co ogranicza drenaż

- Cykle `AppModel` są anulowane w `onPause`; pogoda jest timerowana, a nie odpytywana w tle przez osobny worker.
- Widgety mają `updatePeriodMillis="0"`; nie wykonują własnego okresowego pollingu.
- Nie ma stałego `WakeLock` na czas bezczynności. `PARTIAL_WAKE_LOCK` MediaPlayera i `FLAG_KEEP_SCREEN_ON` są powiązane z aktywnym, głośnym alarmem i są intencjonalne.
- Sama usługa foreground/SSE nadal zużywa CPU/sieć, a system/OEM może ograniczyć jej działanie. Status ostatniego odczytu nie gwarantuje przyszłego dostarczenia alarmu.

## 4. Pamięć i sprzątanie zasobów

**Nie znalazłem statycznego dowodu na klasyczny, bezterminowy wyciek Activity/Service ani na stale zatrzymany odtwarzacz. To nie jest dowód braku wycieków w runtime.**

| Obszar | Wniosek z kodu |
|---|---|
| Zakres serwisu | `AlarmMonitorService` ma własny `SupervisorJob` i wywołuje `scope.cancel()` w `onDestroy()`. Długotrwały SSE w `RtdbClient.streamEvents()` rejestruje `invokeOnCompletion { call.cancel() }` i zamyka `Response` przez `use`, więc anulowanie coroutine jest powiązane z tym strumieniem. |
| Audio alarmu | `MediaPlayer` jest zatrzymywany/zwalniany przy wyciszeniu, błędzie, końcu i `onDestroy()`. Opóźnione callbacki Handlera sprawdzają aktywną instancję i wygasają po maks. kilku sekundach — krótkie, nie stałe referencje. |
| Aktywności/odbiorniki | Model korzysta z `lifecycleScope`; aktywności wyrejestrowują dynamiczne receivery przy końcu cyklu życia. |
| Zbiory ograniczone | Log aplikacji ma limit 250 wpisów, terminalowy raw buffer 3500 linii, wykres live 240 próbek, mini-historia PiecState 60 punktów, archiwum solarne 5000 próbek. Trwały zapis solarnego archiwum obejmuje ostatnie 500. |
| Potencjalnie nieograniczona lista | `terminalLines` jest dopisywane przez `terminalSend()` bez limitu; nie znalazłem obecnie czytnika ani wywołania tej metody w UI. To niskie, latentne ryzyko wzrostu w czasie życia modelu, nie potwierdzony wyciek aplikacji. |
| Bufor terminala | `terminalRaw` ogranicza liczbę wpisów, ale nie długość bajtową pojedynczej linii; `terminalAppend()` nie ma w tym checkoutcie call-site. Warto dodać limit bajtów, jeśli zostanie podłączony strumień logów. |
| Historia wykresu | Wyniki dla 30 dni są materializowane w całości i nie są downsamplowane przed przekazaniem do wykresu. To ograniczone czasowo żądanie użytkownika, nie wyciek, ale może zwiększyć peak heap/CPU; dokładny koszt wymaga profilera. |
| Zwykłe żądania HTTP | Helper `RtdbClient.execute()` i bezpośrednie żądania w `Weather.fetch()`/`AppModel` wykonują synchroniczne `OkHttp.execute()` w `Dispatchers.IO`; w tych miejscach nie widać jawnego hooka `invokeOnCompletion { call.cancel() }`. Nie należy tego mylić z długotrwałym SSE, który ma taki hook opisany wyżej. `callTimeout` wynosi 15 s. Anulowanie coroutine może więc nie przerwać natychmiast synchronicznego I/O; operacja jest jednak ograniczona timeoutem, a `Response.use` zwalnia odpowiedź po zakończeniu. To możliwe opóźnienie sprzątania, nie dowód permanentnego wycieku. |

**Priorytet pamięci:** ograniczyć `terminalLines` bajtowo/liczbowo, dodać pojedynczy cancellable job dla historii i downsampling wykresu, a potem sprawdzić Android Studio Memory Profiler/heap dump. Nie ma podstaw do obietnicy „brak wycieków” bez takiego testu.

## 5. Zalecenia przed kolejnym wydaniem

1. **Bloker bezpieczeństwa:** ustalić i skoordynować rotację wspólnego hasła i tokenu komend; przejrzeć reguły RTDB w konsoli. Nie udostępniać nowego APK z tym samym współdzielonym sekretem jako jedynym zabezpieczeniem.
2. **Bloker sieci lokalnej:** dodać uwierzytelnianie po stronie firmware do endpointów konfiguracji i sterowania; dla sesji/komend używać nonce/ochrony przed replay. Uporządkować CORS i PNA. Nie testować tych endpointów na pracującym piecu.
3. W Androidzie scentralizować i walidować adres ESP32, ograniczyć cleartext do niezbędnego celu albo zastąpić go uwierzytelnionym TLS/protokołem; nie wysyłać Wi-Fi/Telegramu do dowolnego `S.ip`.
4. Przenieść sekrety bota i token komend do magazynu sekretów oraz redagować treści `tg_config` w toastach, logach i potwierdzeniach. Zweryfikować też ścieżkę `/piec/cmd` po stronie RTDB.
5. Rozdzielić pliki preferencji wrażliwe/niewrażliwe i naprawić politykę backupu; wybrać jawnie `allowBackup=false` albo sprawdzony zestaw wykluczeń. Wykonać rzeczywisty test cloud/device transfer.
6. Ograniczyć ruch bez naruszania wymagań alarmu: `loadRange` tylko na ekranie wykresu i pojedynczy cancellable job; deduplikować weather fetch; zmierzyć SSE/fallback przed zmianą interwałów; throttle’ować niealarmowe update’y widgetu.
7. Wydzielić **bezpieczne, odizolowane** testy runtime: długi idle z włączonym monitoringiem, profil heap po wielokrotnych wejściach/wyjściach, Android Battery Historian/`dumpsys batterystats` i licznik żądań. Nie przeprowadzać prób wyjść/alarmów na żywym piecu.

## 6. Zmiana ikony debug w tej pracy

Wybrana opcja 2 została przygotowana **wyłącznie dla wariantu debug** jako adaptacyjna ikona: `drawable-nodpi/ic_launcher_debug_foreground.png` (432×432, ok. 184 KB) oraz `mipmap-anydpi-v26/ic_launcher.xml`. Produkcyjne zasoby `src/main` nie zostały zmienione. Nazwa aplikacji debug już zawiera `(TEST)`, a alarmowe powiadomienie debug ma prefiks `TEST`; nie zmieniałem logiki powiadomień ani alarmów.

Pakiet przenoszący zawiera dwa alternatywne warianty: `Android-DirectBoot-Debug-Icon.diff` dodaje zasoby, jeśli debugowa ikona adaptive jeszcze nie istnieje; `Android-DirectBoot-Debug-Icon-Overlay.diff` podmienia wyłącznie grafikę w istniejącym `ic_launcher_debug_art.png` po wcześniejszym patchu bezpieczeństwa/ikony. Instrukcja `android-sync/Android-DirectBoot-Debug-Icon-README.md` wskazuje, który wariant zastosować; nie należy nakładać obu. Nowe zasoby nie zostały zbudowane przez Gradle w tym środowisku.

## 7. Ocena końcowa

- **Bezpieczeństwo:** nie uznałbym obecnego źródła za gotowe do deklaracji „bezpieczne” przed rozwiązaniem współdzielonych poświadczeń, lokalnego API bez widocznej autoryzacji i cleartext z sekretami. Największa niewiadoma to rzeczywiste reguły RTDB oraz firmware działający na urządzeniu.
- **Pamięć:** brak dowodu na stały wyciek głównych komponentów, ale jest latentna nieograniczona lista i niezmierzony peak pamięci historii. Nie można uczciwie zagwarantować braku wycieków statycznym przeglądem.
- **Bateria:** projekt jest nastawiony na ciągłe, szybkie czuwanie, nie minimalny pobór. Najbardziej oczywiste zbędne koszty to 4-sekundowy polling na wierzchu, równoległy fallback SSE oraz pobieranie historii przy każdym `onResume`, także poza stroną wykresu. Rzeczywistego drenażu nie zmierzono.
