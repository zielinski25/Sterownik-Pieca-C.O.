# Model czasu sieci — 3.57.8 (krok 2 przed 3.57.9)

Data: 2026-11-01 · baza: **3.57.8** · **19/19 asercji zielonych**

Krok 2 z ustalonej kolejności: **research (zrobiony) → symulacje limitów (to) →
zmiany w kodzie (czeka na akceptację)**. Firmware nietknięty.

## Po co

Raport nadzoru po 300 s ciszy pokazywał `DESZCZ / BEFORE` i `FB_LOOP / BEFORE`.
Pierwsza hipoteza — „kod ponawia próbę w pętli, 3 × 125 s = 375 s” — **odpadła**
(patrz `WYNIKI-parametry.txt`: jedno `GET`, zero pętli sieciowych).

Prawdziwa przyczyna jest w jednostkach czasu: `setHandshakeTimeout()` w
arduino-esp32 2.0.x przyjmuje **sekundy**, a firmware podaje `4000` — czyli
limit TLS to **4 000 s (66 min)**, nie 4 s. Jedno pobranie może wisieć dłużej
niż próg nadzoru. Dowód: `ZRODLA-BIBLIOTEKI.md`.

## Jak liczy model

Każdy krok ma swój limit i swoje źródło:

| krok | limit | skąd |
|---|---|---|
| DNS | brak w bibliotece | — |
| TCP connect | 5 000 ms (domyślny, `setConnectTimeout` nieużywane) | `HTTPClient.h` |
| TLS handshake | `parametr × 1000` ms | `WiFiClientSecure.cpp` |
| odczyt | 4 s **bezczynności** (`http.setTimeout(4000)`) | `HTTPClient.cpp` |

Wartości liczbowe nie są wpisane na sztywno w model — `zbierz_parametry.py` czyta je
z `main_centrala.cpp` (`--src`), więc model liczy na tym, co naprawdę stoi w firmware.

## Uruchomienie

```
FIRMWARE_SRC=/sciezka/src/main_centrala.cpp bash narzedzia/czas-sieci-3.57.9/run.sh
bash narzedzia/czas-sieci-3.57.9/run.sh     # bez źródła: wartości domyślne = 3.57.8
```

## Scenariusze

| # | scenariusz |
|---|---|
| S1 | normalny serwer |
| S2 | TLS stoi (TCP przyjęty, cisza na ClientHello) |
| S3 | brak trasy (SYN bez odpowiedzi) |
| S4/S5/S6 | wolny TLS: 3,2 s / 4,5 s / 9 s |
| S7 | odpowiedź „kapie” 1 B co 3 s przez 400 s |

Plus seria **N pobrań w 2 h** (pobranie co 15 min, licznik restartów nadzoru
z firmware: 3 restarty / 30 min, zerowanie po 30 min pracy).

## Pliki

| plik | rola |
|---|---|
| `zbierz_parametry.py` | odczyt timeoutów i progów ze źródła firmware |
| `model_czasu.py` | model + asercje |
| `ZRODLA-BIBLIOTEKI.md` | snippety z arduino-esp32 2.0.17 i sumy SHA |
| `WYNIKI-model-czasu.txt`, `WYNIKI-parametry.txt` | zapisane wyjście |
| `run.sh` | jedno polecenie |
| `RAPORT-CZAS-SIECI-3.57.9.md` | wnioski dla 3.57.9 |

## Czego nie sprawdza

Kompilacji (brak toolchainu ESP32), realnego zachowania routera/NAT, `hostByName()`
(brak limitu w bibliotece). Model zakłada najgorszy przypadek: cisza trwa do końca limitu.
