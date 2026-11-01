# Symulacje limitów — model czasu sieci (3.57.8 → 3.57.9)

Data: 2026-11-01 · baza: **3.57.8** (`src/main_centrala.cpp`, SHA-256
`db3da8854cd8408a31978d8391e8290a38b474ef31c03e4d7550e14cf38bc525` — zgodne z
`SHA256SUMS.txt` paczki i z gałęzią PR #12)
Katalog: `narzedzia/czas-sieci-3.57.9/` · **22/22 asercji modelu, wszystko zielone**
Firmware **nie był zmieniony** — to krok 2 z ustalonej kolejności.

> **Decyzje użytkownika (2026-11-01):** limit handshake TLS = **8 s** · zakres =
> **wszystkie 22 miejsca** `setHandshakeTimeout` **oraz** `setTimeout` liczące
> w sekundach · **`.rebuild` → osobno w 3.57.10**, 3.57.9 to tylko limity sieci.

---

## 0. Odpowiedź w jednym zdaniu

Hipoteza numer jeden (pętla ponowień) **odpada** — w funkcji jest jedno `GET` i zero
pętli sieciowych. Przyczyną jest **jednostka czasu**: `setHandshakeTimeout(4000)`
znaczy 4000 **sekund**, czyli limit TLS **66 minut**, a nie 4 s.

---

## 1. Skala zjawiska — sprostowanie (ważne)

Model pokazuje „**5 restartów w 2 h**”, ale to **scenariusz „TLS wisi ZAWSZE”**
(bramka 15-minutowa + TLS, który nigdy nie odpowiada), **a nie stan pieca**.
W prawdziwym logu restarty dzieliły **~4 godziny**:

| kiedy | znacznik | co to znaczy |
|---|---|---|
| 31.10, 23:49 | `DESZCZ / BEFORE` | zawieszenie w TLS w `pobierzPrognozeDeszczu()` |
| 01.11, 03:35 | `FB_LOOP / BEFORE` | zawieszenie w TLS przed `fbApp.loop()` |

Czyli **zawieszenie jest rzadkie** — 2 razy na dobę, nie co 20 minut. Tabela
„5 restartów / 2 h” pokazuje **górną granicę** (co by było, gdyby TLS padł na stałe,
np. przy zmianie routera, blokadzie po stronie operatora albo awarii serwera pośredniczącego),
a nie to, co dzieje się teraz. Sens tej liczby jest taki: **gdy warunek się utrzyma,
sytuacja eskaluje sama** — a wtedy piec traci Telegram i Firebase na ok. 5 min przy
każdym restarcie.

**Koszt pojedynczego zdarzenia (niezależnie od częstości):** 300 s ciszy w zadaniu
sieciowym + restart centrali. Przez te 5 minut nie ma Telegrama ani Firebase
(sterowanie piecem działa — to inne zadanie, na drugim rdzeniu).

---

## 2. Hipoteza nr 1 — sprawdzona i odrzucona

`pobierzPrognozeDeszczu()` (linia 44118):

| sprawdzenie | wynik |
|---|---|
| ile razy wychodzi do sieci | **1** (`http.GET()`) |
| czy `GET` jest w pętli | **nie** (0) |
| trzy pętle w funkcji | wszystkie po `String` — skanowanie JSON-a, bez sieci |
| `setConnectTimeout` | **nie jest wołane** → domyślne 5 s z biblioteki |

Czyli „3 × 125 s = 375 s” nie ma miejsca. Ale jedno pobranie też nie ma limitu 125 s —
i to jest sedno.

## 3. Prawdziwa przyczyna (dowód źródłowy)

Z arduino-esp32 **2.0.17** (pełne snippety i SHA plików w `ZRODLA-BIBLIOTEKI.md`):

```cpp
// WiFiClientSecure.cpp
void WiFiClientSecure::setHandshakeTimeout(unsigned long handshake_timeout) {
    sslclient->handshake_timeout = handshake_timeout * 1000;   // SEKUNDY * 1000
}
```

`setHandshakeTimeout(4000)` → `handshake_timeout = 4 000 000 ms` = **66 min 40 s**.
Żeby dostać 8 sekund (decyzja), trzeba podać **`8`**.

Pętla, która tego używa (`ssl_client.cpp`):

```cpp
while ((ret = mbedtls_ssl_handshake(...)) != 0) {
    if ((millis() - handshake_start_time) > ssl_client->handshake_timeout) return -1;
    vTaskDelay(2);
}
```

`applyFbSockTimeouts(fbSSLClient, 8)` z 3.57.3 tego **nie ratuje**: `SO_RCVTIMEO`
ogranicza pojedynczy `recv()`, a mbedtls wtedy zwraca `WANT_READ` i pętla kręci się
dalej do `handshake_timeout`.

**To tłumaczy oba znaczniki jednym mechanizmem:**

| znacznik | gdzie w kodzie |
|---|---|
| `DESZCZ / BEFORE` | checkpoint stoi **przed** `http.GET()` → zawieszenie w TLS w środku `GET()` |
| `FB_LOOP / BEFORE` | checkpoint stoi **przed** pompą `fbApp.loop()`, poprzedzoną `setHandshakeTimeout` w linii 45662 |

## 4. Model czasu — przed / po (limit 8 s)

| # | scenariusz | PRZED `(4000)` | PO `(8)` | nadzór |
|---|---|---|---|---|
| S1 | normalny serwer | 610 ms | 610 ms | brak restartu |
| S2 | **TLS stoi** (TCP przyjęty, cisza na ClientHello) | **66,7 min → restart po 300 s** | **8,1 s** | brak restartu |
| S3 | brak trasy (SYN bez odpowiedzi) | 5,0 s | 5,0 s | brak restartu |
| S4 | wolny TLS 3,2 s | 3,4 s | 3,4 s | brak restartu |
| S5 | wolny TLS 4,5 s | 4,7 s | 4,7 s | brak restartu |
| S6 | wolny TLS 9 s | 9,2 s | 8,1 s → błąd | brak restartu |
| S7 | odpowiedź „kapie” 1 B / 3 s przez 400 s | 400 s → **restart** | 400 s → **restart** | **nie naprawia** |

**Dlaczego 8, a nie 4 s:** przy 4 s S5 (wolny TLS 4,5 s) kończy się **błędem**
— czyli tracimy pobranie pogody na całe 15 min. Przy 8 s przechodzi, a najgorszy
czas jednego `GET` (S1–S6) to nadal **17 s** (TCP 5 + TLS 8 + odczyt 4) — **17× mniej**
niż próg nadzoru.

Seria 2 h, pobranie co 15 min, TLS stoi (S2) — **górna granica, patrz §1**:

| | pobrań | restartów |
|---|---|---|
| PRZED | 5 | **5** |
| PO (8 s) | 7 | **0** |

## 5. Zakres — ustalony na podstawie audytu źródła

Audyt (`audyt_timeoutow.py`, wynik w `WYNIKI-audyt-timeoutow.txt`) policzył **wszystkie**
wywołania timeoutów i ustalił typ zmiennej dla każdego:

| grupa | miejsc | co dziś | po 3.57.9 |
|---|---|---|---|
| `setHandshakeTimeout(4000)` | **21** | 4 000 s | **8** |
| `setHandshakeTimeout(7000)` | **1** (linia 24056, `solarArchiveFetchGhi`) | 7 000 s | **8** |
| `client.setTimeout(...)` w sekundach, **ŹLE** | **6** | 4 000–15 000 s | 4 / 7 / 8 / 15 s |
| `client.setTimeout(4)` w sekundach, **dobrze** | 9 | 4 s | bez zmian |
| `http.setTimeout(...)` w milisekundach | 5 | 4–15 s | bez zmian |

**Razem do zmiany: 28 miejsc** (22 + 6).

Sześć miejsc `setTimeout`, które trzeba poprawić:

| linia | wywołanie | dziś | ma być | funkcja |
|---|---|---|---|---|
| 24055 | `client.setTimeout(7000)` | 7 000 s | 7 | `solarArchiveFetchGhi` |
| 39299 | `client.setTimeout(8000)` | 8 000 s | 8 | `sprawdzNajnowszaWersjeGithub` |
| 39388 | `client.setTimeout(15000)` | 15 000 s | 15 | `wykonajAktualizacjeOTA` |
| 39439 | `client.setTimeout(15000)` | 15 000 s | 15 | `pobierzFirmwarePanelaZGithub` |
| 44121 | `client.setTimeout(4000)` | 4 000 s | 4 | `pobierzPrognozeDeszczu` |
| 44382 | `client.setTimeout(4000)` | 4 000 s | 4 | `pobierzPelnaPrognoze` |

**Uczciwa uwaga o ryzyku:** te sześć miejsc jest dziś **przykrytych** przez
`HTTPClient::connect()`, które po połączeniu woła `_client->setTimeout((_tcpTimeout+500)/1000)`
i nadpisuje wartość z miejsca wywołania (sprawdzone w źródle 2.0.17; dotyczy też OTA,
bo `httpUpdate.update(client, url)` tworzy własny `HTTPClient`). Czyli **nie są
dziś źródłem zawieszania** — poprawiamy je, bo to ten sam błąd jednostek, są tanie,
a po jakimkolwiek uproszczeniu warstwy HTTP wróciłyby jako realna dziura.

## 6. Co ten fix naprawia, a czego nie

**Naprawia:** zawieszanie w TLS (S2, S6) — oba znaczniki z 1.11, serie restartów przy
trwałej awarii, 300 s ciszy przy każdym takim zdarzeniu.

**Nie naprawia:**
- **S7** — odpowiedź „kapie” (limit odczytu resetuje się przy każdym bajcie).
  Potrzebny limit **całego** pobrania, nie bezczynności — osobny temat, nie w 3.57.9.
- **błędu `.rebuild`** z kroku 1 (rotacja historii) — **decyzja: osobno w 3.57.10**.

## 7. Plan kroku 3 (gotowy, czeka na „tak”)

1. **28 zmian** w `src/main_centrala.cpp` — wyłącznie wartości w wywołaniach timeoutów.
2. `FIRMWARE_VERSION` → `v3.57.9` + wpis w changelogu na górze pliku (Co / Problem /
   Przyczyna / Naprawa / Czego nie zmieniono).
3. Testy przed wydaniem: ten model (22/22), brama `sprawdz_po.py` (diff **tylko**
   w timeoutach, `FIRMWARE_VERSION` i changelogu, `http.setTimeout` nietknięte),
   symulacje limitów z kroku 1 (38/38).
4. Paczka: **jeden kompletny ZIP**, podmienia **tylko** `src/main_centrala.cpp`
   (+ README / CZYTAJ-NAJPIERW / SHA256SUMS). Plików `.h` nie ruszamy.
5. **Nie flashuję** — build i OTA robisz Ty.

## 8. Czego model nie sprawdza (uczciwie)

- **kompilacji** — sandbox nie ma toolchainu ESP32; to robi `pio run`,
- rzeczywistego zachowania routera/NAT (model zakłada ciszę do końca limitu),
- `hostByName()` — biblioteka nie ma na to limitu,
- wpływu 8 s na Telegram/Firebase — realny handshake to setki ms (S1: 610 ms).

## 9. Pliki

| plik | rola |
|---|---|
| `zbierz_parametry.py` | czyta progi i timeouty **z firmware** |
| `audyt_timeoutow.py` | wszystkie `setTimeout`/`setHandshakeTimeout` + typ zmiennej + realna wartość |
| `model_czasu.py` | model czasu + 22 asercje (PRZED/PO/wariant 4 s, seria 2 h) |
| `sprawdz_po.py` | brama kroku 3: czy poprawki są zrobione dokładnie tak i czy diff jest czysty |
| `ZRODLA-BIBLIOTEKI.md` | snippety z arduino-esp32 2.0.17 + SHA plików |
| `WYNIKI-*.txt` | zapisane wyjście wszystkich czterech narzędzi |
| `run.sh` | jedno polecenie |

```
bash narzedzia/czas-sieci-3.57.9/run.sh          # źródło 3.57.8 jest już w tej gałęzi
```
