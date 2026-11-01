# Skąd wziąć wartości do `secrets.h` — ZNALEZIONE w archiwach

Stan na 2026-10-28. Wcześniejsza wersja tego dokumentu zakładała, że haseł
nie ma na dysku i trzeba je odtwarzać z aplikacji/telefonu. **To było błędne:**
hasła są w archiwach ZIP, które leżą w repozytorium — tylko nie w pliku
`secrets.h`, a **wpisane na sztywno w starym `main_centrala.cpp`**, z czasów
przed wprowadzeniem `secrets.h`.

Uwaga na przyszłość: `grep` po plikach roboczych nic nie znajdzie, bo wartości
siedzą **wewnątrz ZIP-ów**. Trzeba zajrzeć do archiwów (`zipfile`/`Expand-Archive`).

## Gdzie dokładnie

### 1. `Cenatrala Pieca.zip` → `src/main_centrala.cpp` (wersja v3.36.2, 31 561 linii)

| linia | co tam jest |
|---|---|
| 4927 | `WIFI_AP_SSID` — wartość wpisana na sztywno |
| 4928 | `WIFI_AP_PASSWORD` — wartość wpisana na sztywno |
| 4933 | `OTA_HASLO` — wartość wpisana na sztywno |
| 5390 | `#define CMD_TOKEN "…"` — token komend wpisany na sztywno |
| 25002 | `FIREBASE_BOOT_PASS` — hasło konta boot Firebase |

### 2. `firmware-patch/v3.54.0-offline-buffer-toggle/SterownikCO-v3.54.0-OFFLINE-BUFFER.zip` → `main_centrala.cpp`

To samo, ale w wersji **v3.54.0** — czyli **jednej wersji przed** przejściem
na `secrets.h` (refaktor w v3.55.0). Wszystkie cztery wartości identyczne jak
w v3.36.2. To jest najmocniejszy dowód, że refaktor **przeniósł** te hasła do
`secrets.h`, a nie zmienił ich.

### 3. `LCD Centrali.zip` → `src/main.cpp` (wyświetlacz)

Niezależne potwierdzenie SSID hotspota, hasła hotspota i hasła OTA. To musi się
zgadzać, bo wyświetlacz łączy się z tą samą siecią awaryjną — komentarz
w `main_centrala.cpp` (linie 3564–3583) wprost mówi, że obie strony mają używać
tych samych wartości.

### 4. `core.zip` i `android-sync/…-PELNE-ZRODLA.zip` → `Prefs.kt`

`Prefs.DEFAULT_CMD_TOKEN` w aplikacji ma **tę samą** wartość co `CMD_TOKEN`
w starej centrali. Czyli token komend aplikacji i pieca to jeden i ten sam
ciąg — dlatego po wgraniu v3.56.5 aplikacja ma dalej sterować.

## Wniosek

Wszystkie 5 wartości `secrets.h` jest odtwarzalnych bez pytania użytkownika
i bez zgadywania:

| klucz | źródło | potwierdzone przez |
|---|---|---|
| `SECRET_WIFI_AP_SSID` | Cenatrala v3.36.2 linia 4927 | v3.54.0, `LCD Centrali.zip` |
| `SECRET_WIFI_AP_PASSWORD` | linia 4928 | v3.54.0, `LCD Centrali.zip` |
| `SECRET_OTA_HASLO` | linia 4933 | v3.54.0, `LCD Centrali.zip` |
| `SECRET_FIREBASE_PASS` | linia 25002 | v3.54.0 |
| `SECRET_CMD_TOKEN` | linia 5390 | v3.54.0, `Prefs.kt` w aplikacji |

Szósty — `SECRET_HISTORIA_TOKEN` — jest nowy (funkcja z v3.56.x), użytkownik
ma go już w `src\secrets.h`.

## Skutek uboczny: te hasła są w historii gita

`Cenatrala Pieca.zip`, `LCD Centrali.zip` i `core.zip` są **trackowane**
w repozytorium, a zawierają token komend, hasło hotspota, hasło OTA i hasło
konta boot Firebase. Kto ma dostęp do repozytorium, ten ma te wartości.

Nie zmieniać ich teraz — rotacja wymaga jednoczesnej zmiany w trzech miejscach
(centrala, aplikacja `Prefs.DEFAULT_CMD_TOKEN`, wyświetlacz) i wgrania obu
urządzeń. Do zaplanowania **po** uruchomieniu pobierania historii;
procedura jest w `BEZPIECZENSTWO_HASLA.md`.

## Czego ten dokument nie zawiera

Samych wartości — celowo. `secrets.h` nie idzie do gita, a ten plik jest
w gicie. Wartości są w ZIP-ach wskazanych wyżej (i w wiadomości do użytkownika).
