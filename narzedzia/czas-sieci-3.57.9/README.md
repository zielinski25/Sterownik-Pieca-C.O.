# Model czasu sieci — 3.57.8 (krok 2 przed 3.57.9)

Data: 2026-11-01 · baza: **3.57.8** · **22/22 asercji modelu zielonych**

Krok 2 z ustalonej kolejności: **research (zrobiony) → symulacje limitów (to) →
zmiany w kodzie (czeka na „tak”)**. Firmware nietknięty.

**Decyzje (2026-11-01):** handshake TLS = **8 s** · zakres = **22 miejsca
`setHandshakeTimeout` + 6 miejsc `setTimeout` w sekundach** (razem 28) ·
`rebuild` → 3.57.10.

## Skąd bierze się źródło

`main_centrala.cpp` 3.57.8 leży **w tej samej gałęzi**:
`firmware-patch/v3.57.8-raport-i-wersja/src/main_centrala.cpp`
(SHA-256 `db3da885…`, zgodne z `SHA256SUMS.txt` paczki). Domyślna ścieżka w `run.sh`
wskazuje właśnie tam — nie trzeba nic podawać.

## Po co

Raport nadzoru po 300 s ciszy pokazywał `DESZCZ / BEFORE` i `FB_LOOP / BEFORE`.
Hipoteza „kod ponawia próbę w pętli, 3 × 125 s = 375 s” **odpadła**
(`WYNIKI-parametry.txt`: jedno `GET`, zero pętli sieciowych).

Prawdziwa przyczyna jest w jednostkach: `setHandshakeTimeout()` w arduino-esp32 2.0.x
przyjmuje **sekundy**, a firmware podaje `4000` — limit TLS to **4 000 s (66 min)**.
Dowód: `ZRODLA-BIBLIOTEKI.md`.

## Jak liczy model

| krok | limit | skąd |
|---|---|---|
| DNS | brak w bibliotece | — |
| TCP connect | 5 000 ms (domyślny) | `HTTPClient.h` |
| TLS handshake | `parametr × 1000` ms | `WiFiClientSecure.cpp` |
| odczyt | 4 s **bezczynności** | `HTTPClient.cpp` |

Liczby nie są wpisane na sztywno — `zbierz_parametry.py` czyta je z firmware.

## Uruchomienie

```
bash narzedzia/czas-sieci-3.57.9/run.sh
FIRMWARE_SRC=/sciezka/main_centrala.cpp bash narzedzia/czas-sieci-3.57.9/run.sh
```

Kolejność: parametry → audyt timeoutów → model (22 asercje) → brama kroku 3.

## Scenariusze

S1 normalny · S2 TLS stoi · S3 brak trasy · S4/S5/S6 wolny TLS (3,2 / 4,5 / 9 s) ·
S7 odpowiedź „kapie” 400 s. Plus seria **N pobrań w 2 h** z licznikiem restartów
nadzoru z firmware (3 restarty / 30 min, zerowanie po 30 min pracy).

**Uwaga:** „5 restartów / 2 h” to scenariusz **„TLS wisi zawsze”**, a nie stan pieca —
w logu restarty dzieliły ~4 h (31.10 23:49 `DESZCZ`, 1.11 03:35 `FB_LOOP`).

## Pliki

| plik | rola |
|---|---|
| `zbierz_parametry.py` | odczyt timeoutów i progów ze źródła |
| `audyt_timeoutow.py` | wszystkie timeouty + typ zmiennej + realna wartość |
| `model_czasu.py` | model + 22 asercje |
| `sprawdz_po.py` | brama kroku 3 (po zmianie ma być 4/4) |
| `ZRODLA-BIBLIOTEKI.md` | snippety z arduino-esp32 2.0.17 + SHA |
| `WYNIKI-*.txt` | zapisane wyjście |
| `RAPORT-CZAS-SIECI-3.57.9.md` | wnioski i plan kroku 3 |

## Czego nie sprawdza

Kompilacji (brak toolchainu ESP32), realnego routera/NAT, `hostByName()`.
Model zakłada najgorszy przypadek: cisza trwa do końca limitu.
