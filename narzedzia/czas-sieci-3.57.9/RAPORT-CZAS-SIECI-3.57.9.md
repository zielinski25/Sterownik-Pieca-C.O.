# Symulacje limitów — model czasu sieci (3.57.8 → 3.57.9)

Data: 2026-11-01 · baza: **3.57.8** (`src/main_centrala.cpp`, SHA-256 `db3da8854cd8408a31978d8391e8290a38b474ef31c03e4d7550e14cf38bc525`,
zgodne z `SHA256SUMS.txt` paczki i z gałęzią PR #12)
Katalog: `narzedzia/czas-sieci-3.57.9/` · **19/19 asercji, wszystko zielone**
Firmware **nie był zmieniony** — to krok 2 z ustalonej kolejności.

---

## 0. Odpowiedź w jednym zdaniu

Hipoteza numer jeden (pętla ponowień) **odpada** — w funkcji jest jedno `GET` i zero
pętli sieciowych. Przyczyną jest **jednostka czasu**: `setHandshakeTimeout(4000)`
znaczy 4000 **sekund**, czyli limit TLS **66 minut**, a nie 4 s — jedno pobranie
może więc wisieć ~66,7 min, dużo dłużej niż próg nadzoru 300 s.

---

## 1. Co pokazał kod (hipoteza nr 1 — sprawdzona i odrzucona)

`pobierzPrognozeDeszczu()` (linia 44118):

| sprawdzenie | wynik |
|---|---|
| ile razy wychodzi do sieci | **1** (`http.GET()`) |
| czy `GET` jest w pętli | **nie** (0) |
| trzy pętle w funkcji | wszystkie po `String` — skanowanie JSON-a, bez sieci |
| `setConnectTimeout` | **nie jest wołane** → domyślne 5 s z biblioteki |
| `setHandshakeTimeout` | **jest**, z wartością `4000` (linia 44122) |
| `client.setTimeout(4000)` / `http.setTimeout(4000)` | są, oba w milisekundach — poprawne |

Czyli „3 × 125 s = 375 s” nie ma miejsca: nie ma trzech prób. Ale jedno pobranie
też nie ma limitu 125 s — i to jest sedno.

## 2. Prawdziwa przyczyna (dowód źródłowy, nie hipoteza)

Z arduino-esp32 **2.0.17** (pełne snippety i sumy SHA w `ZRODLA-BIBLIOTEKI.md`):

```cpp
// WiFiClientSecure.cpp
void WiFiClientSecure::setHandshakeTimeout(unsigned long handshake_timeout) {
    sslclient->handshake_timeout = handshake_timeout * 1000;   // <-- SEKUNDY * 1000
}
```

`setHandshakeTimeout(4000)` → `handshake_timeout = 4 000 000 ms` = **66 min 40 s**.
Żeby dostać 4 sekundy, trzeba podać **`4`**.

Efekt w pętli handshake (`ssl_client.cpp`):

```cpp
while ((ret = mbedtls_ssl_handshake(...)) != 0) {
    ...
    if ((millis() - handshake_start_time) > ssl_client->handshake_timeout) return -1;
    vTaskDelay(2);
}
```

Ta sama pomyłka jest **we wszystkich 22 miejscach** w pliku (linie 23965…45662),
m.in. w `tgEnsureConnected()`, `checkFirebaseCommands()`, `sendStatusToFirebase()`,
`fbInitialize()`, `pobierzPelnaPrognoze()`, `telemetrySendPendingBatch()`
oraz w pompie `tgTaskFn()` (linia 45662, tuż przed `fbApp.loop()`).

**To tłumaczy oba restarty z 1.11 jednym mechanizmem:**

| znacznik w raporcie nadzoru | gdzie w kodzie |
|---|---|
| `DESZCZ / BEFORE` | `diagCheckpoint(...DESZCZ, BEFORE)` stoi **przed** `http.GET()` — zawieszenie w TLS w środku `GET()` |
| `FB_LOOP / BEFORE` | `diagCheckpoint(...FB_LOOP, BEFORE)` stoi **przed** pompą `fbApp.loop()`, a ta ma przed sobą `setHandshakeTimeout(4000)` |

Dodatkowo: `applyFbSockTimeouts(fbSSLClient, 8)` z 3.57.3 **nie ratuje** tego miejsca —
ustawia `SO_RCVTIMEO` na 8 s, co ogranicza pojedynczy `recv()`, ale pętla handshake
kręci się dalej aż do `handshake_timeout`.

## 3. Model czasu — tabela przed / po

Parametry liczbowe wzięte **z firmware** (skrypt `zbierz_parametry.py` czyta źródło),
semantyka **z biblioteki**. Czas = DNS + TCP + TLS + odpowiedź, każdy krok ze swoim limitem.

| # | scenariusz | PRZED `setHandshakeTimeout(4000)` | PO `setHandshakeTimeout(4)` | nadzór |
|---|---|---|---|---|
| S1 | normalny serwer | 610 ms | 610 ms | brak restartu |
| S2 | **TLS stoi** (TCP przyjęty, cisza na ClientHello) | **66,7 min → restart po 300 s** | **4,1 s** | brak restartu |
| S3 | brak trasy (SYN bez odpowiedzi) | 5,0 s | 5,0 s | brak restartu |
| S4 | wolny TLS 3,2 s | 3,4 s | 3,4 s | brak restartu |
| S5 | wolny TLS 4,5 s | 4,7 s | **4,1 s → błąd** | brak restartu |
| S6 | wolny TLS 9 s | 9,2 s | 4,1 s → błąd | brak restartu |
| S7 | odpowiedź „kapie” 1 B / 3 s przez 400 s | 400 s → **restart** | 400 s → **restart** | **nie naprawia** |

Wariant **8 s** (`setHandshakeTimeout(8)`): S5 i S6 przechodzą (4,7 s / 8,1 s),
S2 nadal kończy się po 8,1 s — czyli też bez restartu.

### Seria 2 godzin, pobranie co 15 min, TLS stoi (S2)

| | pobrań | restartów |
|---|---|---|
| PRZED | 5 | **5** |
| PO (4 s) | 7 | **0** |

Oś czasu PRZED (z modelu):

```
t= 900 s  pobranie WISI (tls_timeout) — TLS handshake
t=1200 s  RESTART (licznik=1)
t=2100 s  pobranie WISI …
t=2400 s  RESTART (licznik=2)
t=3300 s  pobranie WISI …
t=3600 s  RESTART (licznik=3)
t=4500 s  pobranie WISI …
t=4800 s  NADZOR_LIMIT — bez restartu („poddalem sie”)
t=5400 s  licznik wyzerowany po 30 min pracy
t=5401 s  RESTART
```

Czyli przy trwałej awarii TLS-u centrala wpada w **serię restartów co ~20 min**,
a piec przez większość czasu nie ma Telegrama ani Firebase — dokładnie to, co widać
w logach z 1.11.

## 4. Wniosek: poprawka jest jednolinijkowa, ale nie w jednym miejscu

`setHandshakeTimeout(4000)` → **`setHandshakeTimeout(4)`** (albo `8`), w **22 miejscach**
w `main_centrala.cpp`. Żadnych innych zmian: żadnych progów nadzoru, żadnej logiki,
żadnych formatów JSON, żadnego WDT.

**Który wariant?** Model pokazuje różnicę tylko w S5/S6 (wolny TLS 4–9 s):

| wariant | limit TLS | S5 (4,5 s) | S6 (9 s) | uwaga |
|---|---|---|---|---|
| **4 s** | 4 000 ms | ✗ błąd | ✗ błąd | pobranie „nie wyszło” = okno bez deszczu do 15 min, **bez restartu** |
| **8 s** | 8 000 ms | ✓ | ✗ błąd | zostaje margines na wolny STARTTLS/retransmisje |

Osobiście **skłaniałbym się do 8 s**: 8 s to wciąż 37× mniej niż próg nadzoru
(300 s), a nie tracimy pobrań przy wolnym łączu. Wtedy limit jednego `GET`
to 5 s (TCP) + 8 s (TLS) + 4 s (odczyt) = **17 s** — z zapasem poniżej 300 s.

## 5. Co ten fix naprawia, a czego nie

**Naprawia:** zawieszanie w TLS (S2, S6) — oba znaczniki z 1.11 (`DESZCZ / BEFORE`,
`FB_LOOP / BEFORE`), serie restartów co 20 min, ciszę w Telegramie/Firebase.

**Nie naprawia:** S7 — odpowiedź, która „kapie” (1 bajt co 3 s), bo limit odczytu
jest resetowany przy każdym bajcie. Wtedy nadal jest restart po 300 s. To osobny
temat (i osobna decyzja): potrzebny limit **całego** pobrania, nie tylko bezczynności.
Nie proponuję go w 3.57.9, żeby nie mnożyć zmian — ale warto zapisać jako następny krok.

## 6. Czego model nie sprawdza (uczciwie)

- **kompilacji** — sandbox nie ma toolchainu ESP32; to robi `pio run`,
- rzeczywistego zachowania routera/operatorskiego NAT (model zakłada najgorszy
  przypadek: cisza do końca limitu),
- `hostByName()` — biblioteka nie ma na to limitu i model tego nie zmienia,
- wpływu `setHandshakeTimeout(4)` na Telegram/Firebase (te same gniazda, inne
  serwery) — 4–8 s to i tak wielokrotnie więcej niż realny handshake z tych adresów
  (S1: 610 ms na Open-Meteo, Firebase/Telegram są bliżej).

## 7. Pliki

| plik | rola |
|---|---|
| `zbierz_parametry.py` | czyta progi i timeouty **z firmware** (bez oceniania) |
| `model_czasu.py` | model czasu + 19 asercji (PRZED/PO, seria 2 h) |
| `ZRODLA-BIBLIOTEKI.md` | dosłowne snippety z arduino-esp32 2.0.17 + SHA plików |
| `WYNIKI-model-czasu.txt`, `WYNIKI-parametry.txt` | zapisane wyjście |
| `run.sh` | jedno polecenie: parametry + model |

```
FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/czas-sieci-3.57.9/run.sh
```

## 8. Czego czekam przed krokiem 3

Akceptacji **wariantu** (4 s czy 8 s) i potwierdzenia zakresu: **wszystkie 22 miejsca**
czy tylko te, które realnie wisiały (`pobierzPrognozeDeszczu`, `pobierzPelnaPrognoze`,
pompa `fbApp.loop()`, `fbInitialize`, `checkFirebaseCommands`, `sendStatusToFirebase`,
`tgEnsureConnected`)?

Po akceptacji: zmiana w `main_centrala.cpp`, `FIRMWARE_VERSION` → `v3.57.9`, wpis
w changelogu na górze pliku, ponowne uruchomienie obu symulacji (limity + czas),
test zgodności, paczka ZIP z **jednym** podmienionym plikiem i README/CZYTAJ.
