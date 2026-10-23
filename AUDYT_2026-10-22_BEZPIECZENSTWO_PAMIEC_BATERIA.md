# Audyt 2026-10-22 — bezpieczeństwo, pamięć, bateria

Drugi audyt, po `AUDYT_ANDROID_BEZPIECZENSTWO_PAMIEC_BATERIA.md` z 2026-10-10.
Tamten opisywał stan źródeł **przed** poprawkami. Ten sprawdza, **co faktycznie
jest w kodzie dzisiaj**, i dokłada research zewnętrzny.

Zakres: Android v0.31.28 (baza v0.31.22 + nakładki 0.31.23–0.31.28),
firmware v3.54.0.

**To nadal audyt statyczny.** Nie ma tu pomiaru `mAh`, profilera heap, testu
penetracyjnego ani deasemblacji APK — w sandboxie nie ma JDK, Androida ani
urządzenia.

> Dokument **nie zawiera wartości haseł ani tokenów**. Podaje miejsce
> (plik + linia) i typ sekretu. Wartości są w repozytorium — i to jest problem,
> opisany w A1.

---

## A. Bezpieczeństwo

### A1 — KRYTYCZNE: repozytorium jest publiczne, a w nim stałe sekrety

**Sprawdzone empirycznie, nie z dokumentacji:** anonimowe `GET` na
`api.github.com/repos/zielinski25/Sterownik-Pieca-C.O.` zwraca **HTTP 200**,
a pole `private` ma wartość **`false`**. Każdy, bez logowania, może przeczytać
każdy plik.

Co z tego wynika — sekrety możliwe do odczytania przez dowolną osobę:

| sekret | plik | linia |
|---|---|---|
| hasło OTA (`OTA_HASLO`) | `firmware-patch/v3.54.0-offline-buffer-toggle/main_centrala.cpp` | 5391 |
| hasło hotspota centrali (`WIFI_AP_PASSWORD`) | ten sam plik | 5386 |
| token komend (`DEFAULT_CMD_TOKEN`) | `app/.../core/Prefs.kt` (w źródłach v0.31.22) | 316 |
| Firebase API key (`DEFAULT_FB_API_KEY`) | ten sam plik | 314 |

Sama **Firebase API key nie jest sekretem** — to publiczny identyfikator
projektu, tak samo jak w każdej aplikacji Firebase. Sekretami są pozostałe trzy.

**Skutek łańcuchowy:**

1. Ktoś w Twojej sieci Wi-Fi czyta hasło OTA → może wgrać **własny firmware**
   przez `/ota-upload`. Firmware nie jest podpisany (brak Secure Boot),
   więc urządzenie przyjmie dowolny obraz.
2. Ma wtedy pełną kontrolę nad piecem — łącznie z wyłączeniem zabezpieczeń.
3. Nawet bez wgrywania firmware: `/api/*` **nie ma autoryzacji** (patrz A2),
   więc sterowanie serwami i pompą działa od razu, bez żadnego hasła.
4. Token komend z repo pozwala wysyłać polecenia do `/piec/cmd` w Firebase.

**To nie jest hipoteza.** Tak wyglądają dokładnie wektory z OWASP IoT Top 10
(I1 — słabe/zaszyte hasła, I3 — niezabezpieczone interfejsy ekosystemu,
I4 — brak bezpiecznego mechanizmu aktualizacji).

**Co zrobić, w tej kolejności:**

| krok | działanie | efekt |
|---|---|---|
| 1 | **Ustaw repozytorium jako prywatne** (Settings → General → Danger Zone → Change visibility) | zamyka dostęp do haseł w jednym kliknięciu, nic nie psuje |
| 2 | Zmień hasło OTA i hasło hotspota na nowe, mocne | nawet po wycieku starych, nowe nie są znane |
| 3 | Zmień token komend | j.w. |
| 4 | Rozważ „wyczyść historię" (`git filter-repo`) albo pogódź się z tym, że stare wartości są w historii Gita na zawsze | bez tego krok 2 i 3 są kosmetyką wobec historii |

**Uwaga o kolejności:** repozytorium jest publiczne **od początku**, więc
hasła mogły już zostać zaindeksowane (Google, serwisy typu GitGuardian,
archiwa Githuba). Zmiana haseł jest konieczna niezależnie od tego, czy
ustawisz repo na prywatne.

**Alternatywa, jeśli chcesz zachować publiczne repo:** przenieś sekrety do
`secrets.h` w `.gitignore` i wstawiaj je tylko przy budowaniu. Wtedy w repo
zostają nazwy zmiennych, nie wartości. To wymaga zmiany układu plików
firmware — czyli kolejnego wgrania, więc nie robiłbym tego teraz, tylko przy
następnej okazji.

### A2 — WYSOKIE: endpointy `/api/*` w firmware nie mają autoryzacji

**Potwierdzone w kodzie**, nie z pamięci. Przykład — `/api/servo-manual`,
`main_centrala.cpp:29137`:

```cpp
wifiWebServer.on("/api/servo-manual", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("os", true) || !request->hasParam("procent", true)) { ... 400 ... }
    if (TrybSerwa != 2) { ... 400 ... }          // sprawdza TRYB, nie uprawnienia
    ...
    docelowaKlapaProcent = procent;               // rusza serwem
```

Nie ma sprawdzenia hasła, sesji ani tokenu. To samo dotyczy innych endpointów
konfiguracji i sterowania. Hasło (`OTA_HASLO`) jest używane **wyłącznie**
przy uploadzie firmware i plików, nie przy `/api/*`.

**Skutek:** każde urządzenie w tej samej sieci LAN — laptop gościa, telefon
dziecka, zainfekowany PC — może przestawić serwa, pompę i mieszadło bez
żadnego poświadczenia.

**Ograniczenie, które już istnieje:** `LocalControllerAddress.kt` w aplikacji
dopuszcza tylko literalne adresy RFC1918 i waliduje ścieżkę. To chroni
**aplikację** przed wysłaniem danych pod obcy adres, ale **nie chroni
centrali** przed innymi klientami w LAN.

**Zalecenie:** dodać do `/api/*` wspólne hasło (Basic Auth albo pole `token`
w body) sprawdzane po stronie ESP32. To zmiana firmware, więc do zaplanowania,
nie na teraz.

### A3 — WYSOKIE: Telegram bez weryfikacji certyfikatu (`setInsecure`)

`main_centrala.cpp:5119` — połączenie z `api.telegram.org` używa
`setInsecure()`, czyli **nie sprawdza certyfikatu serwera**.

**Skutek:** ktoś w tej samej sieci Wi-Fi (kawiarnia, hotel, gościnna sieć)
może podstawić własny serwer i przechwycić **token bota** oraz **treść
wiadomości** (temperatury, alarmy, ewentualne komendy).

**Zalecenie:** `setCACert()` z korzeniem dla `api.telegram.org`. Kod już to
robi dla Firebase (`main_centrala.cpp:801` — „jawna lista CA, bez
`setInsecure()`"), więc wzorzec jest w pliku. To zmiana firmware.

### A4 — ŚREDNIE: OTA z GitHuba bez weryfikacji autentyczności obrazu

`httpUpdate.update(client, otaGithubDownloadUrl(tag))` — transport jest
HTTPS, ale:

- samo `Update.end()` sprawdza **integralność** (czy obraz nie jest
  uszkodzony), nie **autentyczność** (kto go podpisał),
- brak Secure Boot → urządzenie przyjmie dowolny poprawny obraz,
- `sprawdzNajnowszaWersjeGithub()` używa `setInsecure()`
  (`main_centrala.cpp:2466`).

**Skutek:** przy kontroli nad ruchem sieciowym można podstawić własny firmware.
Wymaga to aktywnego ataku w sieci (ARP spoofing / DNS), nie jest trywialne —
ale przy A1 (publiczne hasło OTA) atak jest znacznie łatwiejszy drugą drogą.

### A5 — ŚREDNIE: tokeny w query URL

`RtdbClient.kt:114` — `authQuery()` dokleja token do `?auth=...`. To
**standardowa ścieżka Firebase REST**, nie błąd — ale token widnieje w URL,
więc trafia do logów proxy, historii przeglądarki i diagnostyki sieciowej
łatwiej niż nagłówek `Authorization`.

**Nie zamieniać tego mechanicznie** na nagłówek `Bearer` — Firebase ID token
w query to udokumentowany sposób, a `Bearer` dotyczy innego typu
poświadczenia. Zmiana wymaga testu na bazie testowej.

### A6 — NIEWERYFIKOWALNE z sandboxa: reguły Firebase RTDB

Nie mam dostępu do konsoli projektu, więc **nie wiem**, czy reguły bazy są
zamknięte. To największa niewiadoma całego audytu.

Dlaczego to ważne: w marcu 2024 zespół trzech badaczy przeskanował 5 mln
domen i znalazł **916 instancji Firebase z błędnymi regułami**, co wystawiło
**125 mln rekordów**, w tym **19,8 mln haseł w czystym tekście**
([The Register](https://www.theregister.com/2024/03/18/google_firebase_cloud_security/),
[Bleeping Computer](https://www.bleepingcomputer.com/news/security/misconfigured-firebase-instances-leaked-19-million-plaintext-passwords/)).
Osobny skan z 2025 r. znalazł **35 publicznych baz RTDB na 937 projektów**
([ice0.blog](https://ice0.blog/docs/openfirebase)).

Jeśli Twoja baza ma `.read: true` / `.write: true` (domyślne „tryb testowy",
który wygasa po 30 dniach), to przy **publicznym API key** w repo każdy może
odczytać całe drzewo `/piec` — temperatury, historię, IP centrali, stan alarmów.

**Jak to sprawdzić w 30 sekund, bez konsoli:** spróbuj odczytać bazę
anonimowo. Jeśli wróci `{"error":"Permission denied"}` — jest OK. Jeśli
wróci JSON z danymi — baza jest otwarta.

### A7 — POZYTYWNE (sprawdzone, nie z dokumentacji)

| mechanizm | stan |
|---|---|
| `allowBackup="false"` | jest — sekrety nie idą do chmury Google |
| Full-screen intent na Androidzie 14+ | **jest sprawdzane** `canUseFullScreenIntent()` + fallback do heads-up (`AlarmNotify.kt:97,102,211`) |
| hasło do Firebase | `setSecret()` → AES-GCM + Android Keystore, z migracją starych wartości |
| walidacja adresu centrali | `LocalControllerAddress.kt` — ścisłe RFC1918, odrzuca hostname’y, porty i dziwne znaki w ścieżce |
| logowanie sekretów do Logcat | nie znalazłem |
| `android:exported` | tylko `MainActivity`, konfigurator widgetu i 3 providery widgetów — reszta `false` |
| potwierdzenie OTA w `loop()` | jest, 10 min / 60 min + wykrywanie cichego rollbacku |

---

## B. Pamięć

### B1 — Latentny wyciek: `terminalLines` bez limitu

`AppModel.kt:2286`:

```kotlin
val terminalLines = mutableStateListOf<String>()
fun terminalSend(line: String) = scope.launch {
    terminalLines.add("» " + line)
    ...
    terminalLines.add(response ?: "⚠ sterownik nie zwrócił odpowiedzi")
}
```

Lista rośnie z każdą komendą i **nigdy nie jest czyszczona** (brak
`terminalLines.clear()` w całym drzewie). Każdy wpis żyje do zniszczenia modelu.

**Waga: niska.** `terminalSend()` nie ma wywołania z UI — jedyne trafienie to
definicja. Więc dziś lista jest pusta. Ryzyko zmaterializuje się, jeśli ktoś
podłączy terminal do ekranu. Poprawka: limit 200 wpisów, analogicznie do `logs`.

### B2 — Reszta kolekcji jest ograniczona

Sprawdzone w kodzie, nie z pamięci:

| kolekcja | limit | plik |
|---|---|---|
| `logs` | 250 | `AppModel.kt:218` |
| `terminalRaw` | 3500 | `AppModel.kt:1688` |
| `chartLive` | 240 | `AppModel.kt:1116` |
| archiwum solarne | 5000 | `SolarAnalytics.kt:122` |
| zdarzenia poboru wody | 100 | `SolarAnalytics.kt:107` |
| zapis archiwum solarnego | 500 | `SolarAnalytics.kt:75` |

### B3 — Brak typowych źródeł wycieku

Sprawdzone i **nie znalazłem**: `GlobalScope`, statycznych referencji do
`Context`, rejestrowanych listenerów systemowych (`SensorManager`,
`LocationManager`). Model dostaje `CoroutineScope` z zewnątrz i jest anulowany
w `onPause` — to poprawny wzorzec.

### B4 — Nowy koszt z v0.31.27, pomijalny

`recomputeToday()` przechodzi całe archiwum (do 5000 próbek) przy **każdej**
nowej próbce, czyli co 25 s. To ~5000 iteracji prostych działań na `Double`
— rząd mikrosekund, bez wpływu na baterię. Warto o tym wiedzieć, gdyby
archiwum kiedyś urosło do setek tysięcy wpisów.

### B5 — Czego nie zmierzono

Nie ma profilera heap ani `dumpsys meminfo`. **Nie mogę uczciwie napisać
„brak wycieków"** — mogę napisać „nie znalazłem statycznego dowodu na wyciek".
To różnica.

---

## C. Bateria

### C1 — Co się zmieniło od audytu z 10.10 (na plus)

W październikowym audycie usługa alarmowa trzymała **SSE + fallback GET co 30 s
+ reconnect co 3 min** ≈ 140 żądań/h. W v0.31.22 tego nie ma:

```kotlin
/** Background alarm checks use one RTDB read per minute ... */
private suspend fun monitorLoop() {
    ...
    val status = Rtdb.pollStatus()
    ...
    val waitMs = (BACKGROUND_STATUS_POLL_MS - elapsedMs).coerceAtLeast(1_000L)
}
```

`BACKGROUND_STATUS_POLL_MS = 60_000` (`AlarmMonitorService.kt:398`). **Jedno
żądanie na minutę, bez SSE, bez reconnectów.** To największa pojedyncza
poprawa energetyczna w całym projekcie — około **2,3× mniej ruchu w tle**.

### C2 — Aktualny profil zużycia

| kontekst | interwał | żądania/h | żądania/dobę (szacunek) |
|---|---|---|---|
| aplikacja na ekranie | 8 s | 450 | ~450 (przy 1 h ekranu) |
| usługa alarmowa w tle | 60 s | 60 | ~1 380 |
| pogoda | 15 min | 4 | ~96 |
| widget | bez pollingu (`updatePeriodMillis="0"`) | 0 | 0 |

Razem **~1 900 żądań/dobę**. Dla porównania: jedna strona WWW to często
50–100 żądań.

### C3 — Gdzie realnie ucieka energia

Największy koszt to nie liczba żądań, a **czas pracy radia**. Po każdym
żądaniu HTTPS modem pozostaje aktywny kilka sekund („tail time"). Przy
60 żądaniach/h tło trzyma radio w stanie aktywnym przez znaczną część doby.

**Szacunek rzędu wielkości (nie pomiar):** 60 żądań/h × ~3 s aktywnego radia
to ~5% czasu w stanie wysokiego poboru. Dla typowego telefonu daje to
**kilka procent baterii na dobę** — odczuwalne, ale nie alarmujące.

To jest cena decyzji „alarmy muszą działać przy telefonie w kieszeni",
którą podjąłeś 2026-10-22 (wybór „A"). **Nie proponuję jej zmiany** — bez
tego alarm przestanie być alarmem.

### C4 — Co ogranicza drenaż (sprawdzone)

- brak stałego `WakeLock` — `PARTIAL_WAKE_LOCK` jest tylko przy aktywnym,
  głośnym alarmie (`AlarmMonitorService.kt:154`),
- brak `AlarmManager`, brak `WorkManager` — widget nie budzi urządzenia,
- cykle `AppModel` anulowane w `onResume`/`onPause`,
- `foregroundServiceType="specialUse"` zadeklarowany poprawnie (wymóg Androida 14+).

### C5 — Ryzyko systemowe, nie związane z kodem

Doze i App Standby mogą ograniczyć pracę usługi. Aplikacja prosi
o wyłączenie optymalizacji baterii (`Alarms.kt:370`,
`ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS`) — i to jest właściwe podejście
dla aplikacji alarmowej.

**Uwaga na przyszłość:** gdybyś kiedyś chciał opublikować aplikację w Google
Play, `REQUEST_IGNORE_BATTERY_OPTIMIZATIONS` jest uprawnieniem
reglamentowanym — wymaga uzasadnienia, a Google może odrzucić aplikację,
jeśli nie należy do dozwolonej kategorii. Przy instalacji przez `adb`
problem nie istnieje.

Podobnie `USE_FULL_SCREEN_INTENT`: od Androida 14 to uprawnienie specjalne.
Play odbiera je aplikacjom, których główną funkcją nie jest alarm ani
połączenia ([Play Console Help](https://support.google.com/googleplay/android-developer/answer/13392821?hl=en),
[AOSP](https://source.android.com/docs/core/permissions/fsi-limits)).
Twoja aplikacja **ma** sprawdzenie `canUseFullScreenIntent()`, więc w razie
odmowy pokaże heads-up zamiast pełnego ekranu — to poprawne zachowanie.

---

## D. Podsumowanie ryzyk

| # | ryzyko | waga | stan | wymaga |
|---|---|---|---|---|
| A1 | publiczne repo + stałe hasła | **krytyczne** | do naprawy | zmiana ustawień GitHuba (**bez wgrywania**) |
| A2 | `/api/*` bez autoryzacji | **wysokie** | do naprawy | zmiana firmware |
| A3 | Telegram `setInsecure` | wysokie | do naprawy | zmiana firmware |
| A4 | OTA bez podpisu | średnie | akceptowalne przy prywatnym repo | zmiana firmware (Secure Boot) |
| A5 | token w query URL | średnie | standard Firebase | nic |
| A6 | reguły RTDB | **nieznane** | do sprawdzenia | 30 sekund, test anonimowy |
| B1 | `terminalLines` bez limitu | niskie | latentne | zmiana APK (2 linie) |
| C | drenaż baterii | akceptowalny | zgodny z decyzją „A" | nic |

**Najważniejsze zdanie tego audytu:** jedyne ryzyko krytyczne **nie wymaga
wgrywania czegokolwiek**. Repozytorium jest publiczne, a hasło OTA leży
w pliku źródłowym. Zmiana widoczności repozytorium na prywatne zajmuje
kilkanaście sekund i zamyka największą dziurę.

---

## E. Ograniczenia

- brak JDK/Android SDK → nie zbudowałem APK, nie zdeasemblowałem DEX,
- brak urządzenia → brak `adb`, profilera heap, Battery Historian,
- brak dostępu do konsoli Firebase → **A6 pozostaje niewiadomą**,
- brak dostępu do routera → nie sprawdziłem, czy port 80 centrali jest
  wystawiony na internet (jeśli tak, A2 staje się krytyczne),
- audyt nie zawiera wartości żadnego sekretu.

## F. Źródła

- [The Register — 900+ stron Firebase z błędnymi regułami, 125 mln rekordów](https://www.theregister.com/2024/03/18/google_firebase_cloud_security/)
- [Bleeping Computer — 19 mln haseł jawnym tekstem w Firebase](https://www.bleepingcomputer.com/news/security/misconfigured-firebase-instances-leaked-19-million-plaintext-passwords/)
- [ice0.blog — skan 937 projektów, 35 publicznych baz RTDB](https://ice0.blog/docs/openfirebase)
- [OWASP IoT Top 10 — I1, I3, I4](https://github.com/OWASP/IoTGoat)
- [espressif/arduino-esp32 — CSRF w endpointach OTA (GHSA-9vfw-wx65-c872)](https://github.com/espressif/arduino-esp32/security/advisories/GHSA-9vfw-wx65-c872)
- [Play Console Help — wymagania foreground service i full-screen intent](https://support.google.com/googleplay/android-developer/answer/13392821?hl=en)
- [AOSP — limity full-screen intent](https://source.android.com/docs/core/permissions/fsi-limits)
- [Android Developers — Doze i App Standby](https://developer.android.com/training/monitoring-device-state/doze-standby)
