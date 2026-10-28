# Skąd wziąć 5 brakujących wartości do `secrets.h`

Stan na 2026-10-28 po przeszukaniu komputera użytkownika:

```
C:\Users\Rafcio\Documents\PlatformIO\sterownik-co-esp32\src\secrets.h   52 B   <- tylko HISTORIA
C:\Users\Rafcio\.platformio\...\WebServer\examples\WebServer\secrets.h  531 B  <- przykład z biblioteki, NIE nasz
```

**Wniosek:** na tym komputerze nie ma starego `secrets.h` z prawdziwymi hasłami.
Nie da się go „znaleźć" — trzeba odczytać wartości z urządzeń, które ich używają.
Na szczęście **wszystkie 5 wartości są odczytywalne albo wymienne**.

## Co robi każda wartość (z kodu `main_centrala.cpp`)

| Klucz | Gdzie używany | Co się stanie przy złej wartości |
|---|---|---|
| `SECRET_WIFI_AP_SSID` | `WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)` — linia 29791 | inna nazwa hotspota; trzeba się połączyć od nowa |
| `SECRET_WIFI_AP_PASSWORD` | to samo; **min. 8 znaków** | < 8 znaków → `softAP` w ogóle nie wystartuje |
| `SECRET_OTA_HASLO` | `ArduinoOTA.setPassword()` (29944), `/ota-upload` (28702), upload z przeglądarki (29315) | OTA odrzuci wgrywanie; USB działa normalnie |
| `SECRET_FIREBASE_PASS` | tylko awaryjne konto boot `piec_co_boot@akwarium.local` (39610) | **nic**, dopóki NVS ma komplet danych logowania (kod najpierw czyta NVS) |
| `SECRET_CMD_TOKEN` | `/api/lan/cmd` nagłówek `X-Piec-Token` (28533) **oraz** token w JSON komendy z Firebase (43282) | aplikacja i panel przestaną sterować piecem — po cichu, tylko wpis `odrzucono: bledny token` w logu |

## Skąd odczytać

### 1. `SECRET_CMD_TOKEN` — z aplikacji albo z panelu (dokładna wartość)

* Aplikacja Android → **Ustawienia → token komend**.
* Panel pieca w przeglądarce → **Ustawienia → „Połączenie z bazą Firebase"**
  (karta `data-menu="firebase"`) → pole tokenu jest wypełnione z `localStorage`.
* Albo w przeglądarce: F12 → Application → Local Storage → `piec_cmd_token`.

Ten token jest też w firmware działającym na piecu, więc musi zostać ten sam —
inaczej po wgraniu v3.56.5 aplikacja przestanie sterować do czasu zmiany w ustawieniach.

### 2. `SECRET_WIFI_AP_PASSWORD` — z telefonu

Telefon → WiFi → sieć `PiecCO_AP` (albo podobna) → **Udostępnij / QR** → Android
pokazuje hasło pod kodem QR. Jeśli piec nigdy nie rozgłaszał hotspota, hasło
i tak trzeba ustawić — wystarczy nowe, byle ≥ 8 znaków.

### 3. `SECRET_OTA_HASLO` — Twoje własne

Używane tylko przy wgrywaniu bez kabla. Jeśli go nie pamiętasz: wpisz nowe,
zapisz je, i od tej pory używaj tego. Wgrywanie po USB (`pio run -t upload`)
tego hasła nie używa.

### 4. `SECRET_FIREBASE_PASS` — na teraz dowolne ≥ 8 znaków

Kod (linia 39618 `loadFirebaseConfig`) najpierw czyta `api_key`/`auth_email`/
`auth_pass` z NVS i **tylko gdy brakuje któregokolwiek** sięga po wartość
z `secrets.h`. Piec działa i loguje do Firebase, więc NVS jest kompletne —
ta wartość nie zostanie użyta.

Gdyby kiedyś NVS wyczyszczono: Firebase nie wstanie, ale naprawia się to
z Telegrama: `/fbprov|KLUCZ_API|piec_co_boot@akwarium.local|HASLO`
(patrz `BEZPIECZENSTWO_HASLA.md`, krok 3).

### 5. `SECRET_WIFI_AP_SSID` — `PiecCO_AP`

Taka jest wartość wzorcowa w `secrets.h.example`. Zmiana = inna nazwa sieci
awaryjnej; nie wpływa na połączenie z domowym WiFi.

## Czego NIE robić

* **Nie wpisywać pustego `SECRET_CMD_TOKEN`.** `token != CMD_TOKEN` przy pustym
  tokenie przepuszcza każde żądanie — piec byłby sterowalny przez kogoś w sieci.
  Dlatego `SEKRETY.ps1` nie przyjmuje wartości krótszej niż 8 znaków.
* **Nie wgrywać firmware z wartościami `ZMIEN-MNIE-...`.** Kompiluje się (to
  wystarcza do sprawdzenia błędów C++), ale aplikacja straci sterowanie.
  `SPRAWDZ.ps1` celowo oznacza takie wartości jako `WZOR`.

## Kolejność

1. Testowa kompilacja z wartościami zastępczymi → odpowiedź: czy poprawka
   etapu 223 usunęła błąd `function-definition`.
2. Odczytanie prawdziwych wartości (punkty 1–5 wyżej).
3. `SEKRETY.ps1` → `SPRAWDZ.ps1` (bez `WZOR`) → `pio run -t upload`.
