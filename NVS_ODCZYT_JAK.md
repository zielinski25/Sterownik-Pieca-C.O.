# Jak odczytać wszystko, co jest w NVS na ESP (centrala i panel LCD)

Data: 2026-11-23. Dotyczy: centrala **v3.58.10** (`firmware-patch/v3.58.10-zew2-ponowienie/`)
i panel LCD **v1.4.7** (`LCD-Centrali/`).

NVS to pamięć ustawień w ESP (ta sama, do której pisze `Preferences`). Ten plik
mówi, **co dokładnie w niej leży** w tym projekcie, **jak ją odczytać w całości**
(zrzut z płytki + `narzedzia/nvs_odczyt.py`) i **czym się różni „odczyt z kodu”**
od „odczytu z pamięci”.

---

## 1. Najkrócej

1. **Wszystko da się odczytać jednym zrzutem partycji `nvs`** — bez ESP-IDF,
   bez PlatformIO, bez podłączania się do firmware'u:

   ```
   python -m esptool --port COM16 read-flash 0x8000 0xC00 tablica.bin
   python narzedzia/nvs_odczyt.py --tablica-partycji tablica.bin
   python -m esptool --port COM16 read-flash 0x9000 0x5000 nvs_centrala.bin
   python narzedzia/nvs_odczyt.py nvs_centrala.bin
   ```

   (Te dwie linie z `0x9000 0x5000` są dla **panelu**, bo tylko jego plik
   partycji jest w repozytorium. Dla centrali offset i rozmiar bierze się
   z pliku `tablica.bin` — pierwsze dwa polecenia. Windows: `python`, nie
   `python3`; `esptool`, nie `esptool.py` — szczegóły w 2.1.)

   Dostajesz wszystkie przestrzenie, wszystkie klucze, wartości, typy, stan
   stron, wpisy usunięte i ostrzeżenia o złych CRC. `--json`, `--csv`,
   `--maskuj`, `--pokaz-usuniete` — patrz rozdział 4.

2. **Offset i rozmiar bierz z `partitions.csv` tego projektu, którego płytkę
   czytasz.** W tym repozytorium jest tylko `LCD-Centrali/partitions.csv`
   (panel): `nvs, data, nvs, 0x9000, 0x5000` — 20 480 B, 5 stron. Projekt
   centrali ma własny plik partycji (w nim jest też system plików 10 354 688 B,
   patrz `RAPORT_PAMIEC_RAM_LCD_I_CENTRALA.md`), **ale tego pliku nie ma
   w tym repozytorium** — więc wartości dla centrali nie da się tu potwierdzić.
   Weź wiersz `nvs` z pliku partycji centrali; jeśli to nie 0x9000/0x5000,
   popraw polecenie. Gdy nie wiesz, gdzie NVS leży: zrzuć tablicę partycji
   (`read-flash 0x8000 0xC00 tablica.bin`) i podaj ją narzędziu
   (`--tablica-partycji`) — wypisze cały układ pamięci i gotowe polecenie
   zrzutu. Awaryjnie działa też `--wykryj` (szuka po nagłówkach stron NVS),
   a jeśli podasz narzędziu cały zrzut flashu, samo znajdzie w nim partycję.

3. **Nie każda rzecz „z NVS” jest w NVS tego projektu.** Część ustawień
   w ogóle nie jest zapisywana (np. `tryb_reczny`/`tryb_auto` z Telegrama
   zmieniają tylko `TrybSerwa` w RAM — do NVS trafia on dopiero przy innej
   okazji; „bieżący stan” pompy, alarmów i ruchu serw też jest w RAM), a część
   leży w innych miejscach: kopia sieci WiFi
   w przestrzeni WiFi rdzenia (`nvs.net80211`), historia i archiwum solarne
   w LittleFS (pliki), zrzuty awarii na partycji `coredump`. Rozdział 3 mówi,
   co jest czym.

4. **Bezpieczeństwo:** zrzut NVS to **wszystkie hasła w czystym tekście**
   (WiFi `pass0..passN`, token bota Telegram, hasło konta Firebase do
   sterowania piecem). Traktuj plik jak hasła — nie wrzucaj do repo
   (`.gitignore` już blokuje `secrets.h`), a raporty rób z `--maskuj`.
   Sam zrzut (`*.bin` z flasha) też trzymaj poza repo.
   `BEZPIECZENSTWO_HASLA.md` §5 mówi, dlaczego zmiana `secrets.h` nie zmienia
   hasła w piecu, który ma je już w NVS — dokładnie dlatego warto umieć to
   odczytać.

---

## 2. Skąd wziąć zrzut (trzy drogi + jedna obejściowa)

### 2.1 Kablem USB — najprościej, czyta wszystko

**Najpierw: gdzie jest esptool.** W tym projekcie nie trzeba nic instalować.
PlatformIO ma własne środowisko Pythona z esptool w środku:

```
C:\Users\<Ty>\.platformio\penv\Scripts\esptool.exe        # Windows
~/.platformio/penv/bin/esptool                              # Linux
```

Jeśli `esptool` nie jest w `PATH` (typowe na Windows — `Scripts` nie jest
dopisywany), wywołaj Pythona z tego samego `penv` i użyj `-m esptool`:

```powershell
C:\Users\Rafcio\.platformio\penv\Scripts\python.exe -m esptool --version
```

**Trzy zasady, na których ludzie się wywracają (esptool 5.x, a taki jest
w PlatformIO — 5.2.0):**

1. **Nie ma `esptool.py`.** W wersji 5 polecenie nazywa się `esptool`;
   plik `esptool.py` jest instalowany tylko poza Windows, i to jako
   przestarzały alias. Na Windows `esptool.py` po prostu nie istnieje —
   „nie jest rozpoznawane jako nazwa polecenia” to nie brak instalacji.
2. **Nie ma `python3`.** Na Windows to `python` (albo `py`). `python3`
   to nazwa z Linuksa i macOS.
3. **Opcje globalne PRZED podkomendą.** `--port`, `--baud`, `--chip` muszą
   stać **przed** `read-flash`. Po podkomendzie `-p` znaczy
   `--no-progress`, nie „port” — stąd mylący komunikat
   `Got unexpected extra argument (COM16)`.

**Zrzut — krok 1: tablica partycji** (0x8000, 0xC00 B; jest na każdej płytce):

```powershell
python -m esptool --port COM16 --baud 921600 read-flash 0x8000 0xC00 tablica.bin
python narzedzia/nvs_odczyt.py --tablica-partycji tablica.bin
```

Drugie polecenie wypisze cały układ pamięci (co jest aplikacją, gdzie NVS,
gdzie system plików) i **gotowe polecenie zrzutu NVS** z prawdziwym offsetem:

```
Tablica partycji z tablica.bin:
  etykieta   typ/podtyp           offset     rozmiar    koniec
  nvs        dane / NVS           0x009000   0x005000   0x00e000
  phy_init   dane / phy           0x00e000   0x002000   0x010000
  ota_0      app / ota_0          0x010000   0x1f0000   0x200000
  ...
Partycja NVS: "nvs", offset 0x9000, rozmiar 0x5000 (20480 B, 5 stron po 4096 B).
Zrzut i odczyt:
  python -m esptool --port COM16 read_flash 0x9000 0x5000 nvs.bin
  python narzedzia/nvs_odczyt.py nvs.bin
```

(Uwaga na podtypy: NVS to `0x02`. `0x01` to `phy_init`, a `0x04` to
`nvs_keys` — partycja z **kluczami szyfrowania** NVS, nie z danymi.
Także etykieta bywa myląca, więc narzędzie patrzy na typ/podtyp, nie na nazwę.)
To jest właściwa droga dla centrali: jej pliku partycji nie ma w repozytorium,
więc offsetu nie da się tu podejrzeć — trzeba go odczytać z urządzenia.

**Zrzut — krok 2: sama partycja NVS.** Panel LCD ma
`nvs, data, nvs, 0x9000, 0x5000` (`LCD-Centrali/partitions.csv`), czyli:

```powershell
python -m esptool --port COM16 --baud 921600 read-flash 0x9000 0x5000 nvs_panel.bin
python narzedzia/nvs_odczyt.py nvs_panel.bin
```

Dla centrali wstaw offset i rozmiar z kroku 1, np.
`read-flash 0x9000 0x6000 nvs_centrala.bin`.

**Gdy nie wiesz, gdzie NVS leży, i nie chcesz czytać tablicy partycji:**
zrzuć cały flash i daj go narzędziu — ono samo znajdzie partycję NVS
(najpierw z tablicy partycji spod 0x8000, potem po nagłówkach stron):

```powershell
python -m esptool --port COM16 --baud 921600 read-flash 0 0x1000000 caly.bin
python narzedzia/nvs_odczyt.py caly.bin
```

16 MB na N16R8 idzie kilka minut, a plik jest duży — do codziennego użytku
lepsze są kroki 1–2. Narzędzie powie wprost, co znalazło:

```
To nie jest sama partycja NVS - to wiekszy obraz.
Znalazlem NVS na 0x9000, 20480 B (tablica partycji z 0x8000, etykieta "nvs"). Czytam ta czesc.
(na przyszlosc: python nvs_odczyt.py caly.bin --offset 0x9000 --rozmiar 0x5000)
```

**Port.** Windows: Menedżer urządzeń → „Porty (COM i LPT)”; na płytkach
z mostkiem CH340 to `COM` inny niż przy USB-CDC, więc przy dwóch płytkach
na raz łatwo pomylić. Linux: `ls /dev/ttyUSB* /dev/ttyACM*`.

**Gdy `python` też nie działa:** sprawdź, czy w ogóle jest Python
(`py --version`, `where python`). PlatformIO instaluje własny interpreter
tylko dla siebie — `python` z `PATH` może go nie widzieć. Wtedy pełna
ścieżka: `C:\Users\Rafcio\.platformio\penv\Scripts\python.exe -m esptool ...`.
Z tego samego `penv` działa też `pip`, więc `pip show esptool` powie,
którą wersję masz (`5.2.0` = ta opisywana wyżej).

**Co się dzieje po drodze:** `read-flash` łączy się z bootloaderem ROM
(reset + `Connecting...`), potem `Writing at 0x00009000... (100 %)`.
Jeśli widzisz `Failed to connect`: zły port, kabel bez linii danych albo
płytka nie w trybie pobierania (przytrzymaj BOOT przy resecie).
Zrzut jest **tylko do odczytu** — nic nie zmienia w pamięci pieca.

### 2.2 Gdy esptool nie da się uruchomić (ścieżki, `pip`, brak internetu)

Kolejność, która zawsze działa na Windows z PlatformIO:

```powershell
& "C:\Users\Rafcio\.platformio\penv\Scripts\python.exe" -m esptool --version
```

(`&` jest potrzebne, gdy ścieżka ma spacje — tutaj nie ma, ale w
`C:\Users\Jan Kowalski\...` już ma.)

- **`esptool: command not found` / „nie jest rozpoznawane...”** — `Scripts`
  nie jest w `PATH`. Albo pełna ścieżka jak wyżej, albo `python -m esptool`.
- **`No module named esptool`** — wywołujesz `python` z innego Pythona niż
  PlatformIO. Sprawdź `where python` i `pip show esptool` w tej samej konsoli.
- **`pip install esptool`** — niepotrzebne (penv już go ma) i na Windows
  częściej szkodzi, bo instaluje drugą, czasem starszą wersję gdzie indziej.
  Jeśli już musisz: `C:\Users\Rafcio\.platformio\penv\Scripts\python.exe -m pip install -U esptool`.
- **Version 5 vs 4** — jeśli `esptool --version` pokaże `4.x`, wtedy działa
  jeszcze `esptool.py` i `-p` po podkomendzie też, ale polecenia z tego
  dokumentu (opcje przed podkomendą, `read-flash` z myślnikiem) działają
  w **obu** wersjach. Nie trzeba się cofać do 4.x.

Na Linuksie/macOS: `python3 -m esptool ...` albo
`~/.platformio/penv/bin/python -m esptool ...`.

### 2.3 Z firmware'u, bez kabla — tylko to, co firmware sam pokaże

Firmware **nie ma** endpointu typu „pokaż całe NVS”. Ma endpointy pokazujące
**to, co z NVS wczytał** (czyli nie pokażą kluczy usuniętych, osieroconych ani
tych, których dana wersja już nie czyta):

| Co | Adres / komenda | Co pokazuje |
| --- | --- | --- |
| ustawienia pieca | `GET /api/ustawienia-get` | wartości z **RAM** (to, czego kod używa teraz), nie z NVS |
| zmiana ustawienia | `POST /api/ustawienia-set` (`pole`, `wartosc`) | waliduje i zapisuje do NVS `piec` |
| zapisane sieci WiFi | `GET /api/wifi/list` | SSID-y i `hasPass`, **bez haseł** (NVS `wifi`) |
| Telegram | `GET /api/telegram/status` | `enabled`, `chatId`, `tokenPrefix` (6 znaków) — NVS `telegram` |
| wersja, stan | `GET /api/status` | `fw`, błędy, liczniki |
| pliki na flashu | `GET /api/fs-list` | LittleFS (historia, logi), **nie NVS** |
| menu komend | Telegram `/help` | lista komend |

Przykład z PC w tej samej sieci:

```
curl http://192.168.4.1/api/ustawienia-get
curl http://192.168.4.1/api/wifi/list
curl http://192.168.4.1/api/telegram/status
```

To dobre do szybkiego sprawdzenia „czy piec ma to, co myślę”, ale **nie
zastąpi zrzutu**: pokaże tylko wybrane pola i nigdy nie pokaże haseł.

### 2.4 Z firmware'u, bez kabla — wszystkie klucze, gdy dopiszesz 40 linii

Gdy nie ma dostępu do USB, a chcesz **wszystko** (nie tylko wybrane pola),
ESP-IDF ma iterator po NVS: `nvs_entry_find()` / `nvs_entry_next()` /
`nvs_entry_info()`. W tym firmware **jeszcze go nie ma** — jest tylko
`Preferences` z ręcznie wypisanymi kluczami. Minimalny odczyt w całości
wygląda tak (wersja do wklejenia, w stylu reszty pliku, z `AsyncWebServer`):

```cpp
#include <nvs.h>
// GET /api/nvs-dump  ->  {"przestrzenie":[{"nazwa":"piec","klucze":[
//                          {"klucz":"tempOn","typ":"u8","wartosc":60}]}]}
wifiWebServer.on("/api/nvs-dump", HTTP_GET, [](AsyncWebServerRequest *request) {
  // [UWAGA] Podglad hasel - tylko w sieci lokalnej, najlepiej za tokenem
  // X-Piec-Token jak /api/lan/cmd. Bez tego kazdy w LAN widzi token Telegram.
  String out = "{\"przestrzenie\":[";
  nvs_iterator_t it = nullptr;
  esp_err_t err = nvs_entry_find("nvs", nullptr, NVS_TYPE_ANY, &it);
  bool pierwsza = true;
  while (err == ESP_OK) {
    nvs_entry_info_t info;
    nvs_entry_info(it, &info);
    if (info.type == NVS_TYPE_U8) {           // wpis przestrzeni nazw
      if (!pierwsza) out += "]}";
      pierwsza = false;
      out += String("{\"nazwa\":\"") + info.namespace_name + "\",\"klucze\":[";
    }
    err = nvs_entry_next(&it);
  }
  nvs_release_iterator(it);
  out += "]}]}";
  request->send(200, "application/json", out);
});
```

Zasady, których nie warto łamać: (1) cały iterator pod jednym zadaniem —
`nvs_entry_find` trzyma blokadę NVS, a `wifiWebServer` chodzi w osobnym
zadaniu AsyncTCP; (2) wątek serwera WWW nie może czekać na `logMutex` ani
na LittleFS (patrz komentarze `[v2.0.1.x]` w centrali); (3) wartości tekstowe
czytaj `nvs_get_str(handle, klucz, NULL, &len)` — najpierw długość, potem bufor
z PSRAM, żeby nie zjeść stosu; (4) **hasła nie idą do JSON-a** — zamiast
wartości podawaj `"***"` i długość, a pełny odczyt zostaw zrzutowi z kabla.

Ta droga ma jedną zaletę: pokazuje to, co **naprawdę** jest zapisane (łącznie
z kluczami, których kod już nie czyta), i jedną wadę: nadal nie pokaże wpisów
usuniętych ani stanu stron — to tylko zrzut.

---

## 3. Co naprawdę leży w pamięciach (stan na v3.58.10 / panel v1.4.7)

### 3.1 Centrala — przestrzenie `Preferences` w partycji `nvs`

Wszystkie nazwy i klucze poniżej są **z kodu**, nie z dokumentacji:
`main_centrala.cpp` (skan `config.get*/put*`, `prefs.begin("...")`, stałe
`*_NVS_NAMESPACE`). Skan daje 8 przestrzeni z `begin("...")` plus `spoolg`
i `coredump` (nazwy w stałych) i 64 klucze w samej przestrzeni `piec`.

| Przestrzeń (NVS) | Klucze | Skąd w kodzie |
| --- | --- | --- |
| `piec` | `wybor` (1 = cykl czasowy/trociny, 2 = cykl temperaturowy), `trybSerwa` (1 auto / 2 ręczny / 3 bezpieczny), `czasOn`, `czasOff`, `tempOn`, `tempOff`, `antystopWlacz`, `antystopDni`, `mieszWlaczony`, `mTrocinyOff`, `mieszadloCzasOn`, `mieszCzasOff`, `tempZadServo`, `histerServo`, `skokKlapy`, `skokSyberka`, `odchylTemp`, `mnoznik`, `progAlarmTemp`, `progAlarmDym`, `dymCzasOn`, `dymCzasOff`, `dymCzasStab`, `dymProgTemp`, `dymTrybPracy`, `pozKlapa`, `pozSyberek`, `rSerwoAct`, `rSerwoWin`, `rSerwoBlk`, `resetCount`, `debugMode`, `tempSrednia`, `solarOffSeason`, `srOstPompa`, `aSndDym`, `aSndOgrz`, `aSndBoth`, `dSerMask`, `dSerLvl`, `dArcMask`, `dArcLvl`, `dRemMask`, `dRemLvl`, `dRemOn`, `otaSelfTag`, `otaSelfTrwa`, `panOtaGotowa`, `panOtaTrwa`, `panOtaTag`, `panOtaObraz`, `panOtaPrzed`, `panOtaRozm`, `panOtaNr`, `nadzorRestart`, `nadzorKto`, `nadzorMs`, `nadzorCnt`, `nadzorKrokZnany`, `nadzorOp`, `nadzorStage`, `nadzorKrokS`, `nadzorWiekS`, `nadzorHeap` | `config` (`Preferences config`, `begin("piec")` w `setup()`) |
| `wifi` | `count`, `ssid0..ssidN`, `pass0..passN` (N ≤ 7, `WIFI_MAX_SIECI = 8`) — **hasła czystym tekstem** | `wifiConfig`, `wifiWczytajSieci()` / `wifiZapiszSieci()` |
| `telegram` | `token` (token bota), `chatId`, `enabled` | `tgConfig`, `loadTelegramConfig()` / `saveTelegramConfig()` |
| `firebase` | `api_key`, `auth_email`, `auth_pass` — **hasło konta** | `fbConfig`, `loadFirebaseConfig()` |
| `hist` | `fs_on` (przełącznik „zapisuj historię na flash”, brak klucza = włączone; przełącznik jest na stronie WWW centrali) | `historiaFsInicjalizujPrzyStarcie()`, `historiaFsZapiszNvs()` |
| `spoolrt` | `lastseq`, `oldest` (numery stron bufora telemetrii do Firebase) | `spoolRuntime*` |
| `spoolg` | `lastseq` (numer strony bufora G), `rt_on` (bufor włączony) | `spoolGAllocatePageSeq()`, `spoolRuntimeSetArmed()` |
| `coredump` | `seq` (numer kolejnego zrzutu awarii w LittleFS) | `coredumpAlokujSeq()` |
| `stb_mega` | `last` (ostatni `update_id` testu stabilności) | `stabilityMegaOznaczUpdateZuzyty()` |
| `tc_test` | `last` (ostatni `update_id` testu `/testcrash TAK`) | `testCrashOznaczUpdateZuzyty()` |

Uwaga o `piec`: komentarz `[FIX NVS-15]` w kodzie — klucz dłuższy niż 15
znaków NVS **po cichu odrzuca**. Dlatego w pliku są skróty (`antystopWlacz`
zamiast `antystopWlaczony`, `dymCzasStab`, `mieszCzasOff`). Jeśli dopisujesz
własny klucz: najwyżej 15 znaków, `nvsPut*`/`nvsGet*` (mają `static_assert`),
nie `config.putX`.

### 3.2 Panel LCD — przestrzenie `Preferences`

| Przestrzeń | Klucze | Znaczenie |
| --- | --- | --- |
| `wyswietlacz` | `jasnDzien`, `jasnNoc`, `godzStart`, `godzKoniec`, `harmonWlacz`, `cpuIdx`, `txIdx` | jasność dzień/noc, harmonogram przyciemniania, taktowanie CPU i moc nadajnika WiFi (indeksy w tabelach, v1.4.7) |
| `dotyk` | `rawXA`, `rawYA`, `rawXB`, `rawYB` | kalibracja dotyku (surowe wartości ADC) |
| `otapanel` | `zapis` (jeden wpis typu blob) | stan aktualizacji panelu: klucz oferty, `stan`, `prob`, `powod`, `oczekiwany` — albo cały nowy, albo cały stary, nigdy połowa (`putBytes` jednym wpisem) |

### 3.3 Czego w NVS **nie ma**

- **Hasła do własnego AP centrali i OTA** — w `secrets.h` (poza repo),
  kompilowane w program.
- **Historia pomiarów, archiwum solarne, logi, zrzuty awarii** — pliki
  w LittleFS (partycyjny system plików), a nie NVS. Podgląd: `GET /api/fs-list`,
  `GET /api/fs-download`, `GET /api/log-lines`; zrzuty: `/coredumpy`.
- **Bufory w PSRAM** (historia krótka/długa, kolejka telemetrii) — giną przy
  restarcie; `narzedzia/pamiec_centrali.py` liczy ich rozmiar.
- **Bieżący stan** (pompa włączona, alarm, pozycje serw w ruchu) — RAM; do NVS
  trafia tylko to, co ma przetrwać restart (`pozKlapa`, `pozSyberek`,
  `rSerwo*`, `srOstPompa`).
- **Dane WiFi rdzenia** — sterownik WiFi ESP-IDF trzyma własną kopię
  ostatniej sieci (SSID, hasło) w tej samej partycji `nvs`, w przestrzeni
  `nvs.net80211`; to nie `Preferences` i nie widać tego w kodzie projektu.
  Biblioteka arduino-esp32 ma to domyślnie włączone (`WiFiGeneric.cpp`,
  `_persistent = true`; `WiFi.persistent(false)` wyłącza), a **panel LCD ma
  je wyłączone** (`WiFi.persistent(false)` przed `WiFi.mode()`, poprawka B8
  z `AUDYT_PANELU_LCD_2026-11-18_WYCIEKI_DRENAZ_KOD.md`), centrala — nie. Przy zrzucie całej partycji takie wpisy też wyjdą w raporcie (klucze
  z kropką, np. `sta.pswd`, `sta.ssid`) i `--maskuj` je ukryje. To dlatego
  samo „wyczyść `wifi` w Preferences” nie zawsze wystarcza, żeby hasło
  zniknęło z pamięci.

---

## 4. `narzedzia/nvs_odczyt.py` — co potrafi

```
python narzedzia/nvs_odczyt.py nvs_centrala.bin                    # cala zawartosc
python narzedzia/nvs_odczyt.py nvs_centrala.bin --maskuj --pokaz-usuniete
python narzedzia/nvs_odczyt.py nvs_centrala.bin --json > nvs.json
python narzedzia/nvs_odczyt.py nvs_centrala.bin --csv nvs.csv
python narzedzia/nvs_odczyt.py --tablica-partycji tablica.bin     # gdzie jest NVS
python narzedzia/nvs_odczyt.py caly_flash.bin                     # sam znajdzie NVS
python narzedzia/nvs_odczyt.py caly_flash.bin --offset 0x9000 --rozmiar 0x5000
python narzedzia/nvs_odczyt.py caly_flash.bin --wykryj             # tylko szuka
python narzedzia/nvs_odczyt.py --autotest
```

(Na Windows `python`, na Linuksie `python3` — reszta poleceń identyczna.)

- czyta **całą** partycję niezależnie od firmware'u — także klucze, których
  dana wersja programu już nie używa, i wpisy **usunięte** (zostają w pamięci
  do przepisania strony),
- sprawdza CRC nagłówków stron, CRC wpisów, CRC tekstów i blobów — mówi,
  które wpisy NVS sam by odrzucił,
- **`--tablica-partycji plik.bin`** czyta tablicę partycji ESP-IDF (zrzut
  0x8000, 0xC00 B) i wypisuje cały układ pamięci: etykietę, typ, podtyp,
  offset, rozmiar i koniec każdej partycji — plus gotowe polecenie zrzutu
  NVS. To jedyny pewny sposób na offset centrali,
- **cały zrzut flashu zamiast partycji** — narzędzie samo znajduje NVS:
  najpierw z tablicy partycji (dokładnie, z etykietą), a gdy jej nie ma,
  po nagłówkach stron NVS; w obu przypadkach mówi, co znalazło i gdzie,
- `--wykryj` tylko szuka partycji w zrzucie całego flashu (po nagłówkach
  stron) i nic nie parsuje,
- `--csv` daje plik w formacie wejściowym narzędzia Espressif
  (`esp-idf-nvs-partition-gen`), więc z odczytu da się **odtworzyć** partycję
  na drugą płytkę (`python -m esp_idf_nvs_partition_gen generate nvs.csv
  nvs.bin 0x5000`); bloby idą jako `hex2bin`, więc wiersz np. `otapanel:zapis`
  ma kilkaset bajtów szesnastkowych w jednej komórce — to normalne,
- `--autotest` sprawdza samo narzędzie na obrazach budowanych w pamięci
  (44 przypadki: liczby, teksty krótsze i dłuższe niż wpis, blob
  wielostronicowy, wpis usunięty, chunk bloba bez indeksu, zepsute CRC,
  zły offset, tablica partycji — poprawna, zakończona 0xFF, zakończona MD5,
  bez partycji NVS, śmieci, wykrywanie w obrazie flashu, CSV).
  Jeśli masz zainstalowane `esp-idf-nvs-partition-gen`, dochodzi 6 przypadków
  pełnego koła: CSV z odczytu → generator Espressif → nasz czytnik (50/50).

### 4.1 Podglądarka graficzna — `narzedzia/nvs_podglad.html`

Jeśli wolisz klikać niż czytać tekst: **jeden plik HTML, bez instalowania
i bez internetu.** Otwórz go w przeglądarce (dwuklik) i przeciągnij zrzut
`nvs.bin` na okno. Nic nie jest nigdzie wysyłane — plik jest czytany lokalnie
w przeglądarce.

Co pokazuje:

| Zakładka | Co tam jest |
| --- | --- |
| **Ustawienia** | przestrzenie i klucze, z **opisem po polsku** dla kluczy tego projektu („Strategia pompy: 1 = cykl czasowy, 2 = cykl temperaturowy”), wyszukiwarka, przycisk „maskuj sekrety” (domyślnie włączony) |
| **Strony** | każda strona 4096 B jako siatka 126 wpisów: zielony = zapisany, czerwony = usunięty, żółty = błąd zapisu, niebieska ramka = wpis zajęty przez dłuższą wartość; do tego numer sekwencyjny, wersja formatu i stan CRC |
| **Ostrzeżenia** | te same ostrzeżenia co w wersji tekstowej (złe CRC, chunki bez indeksu, klucze bez przestrzeni, klucze > 15 znaków) |
| **Tablica partycji** | gdy wczytasz zrzut całego flashu albo plik z 0x8000 — układ pamięci i gotowe polecenie zrzutu NVS |

Detale, które oszczędzają czas:

- **przyjmuje wszystko**: zrzut partycji NVS, cały zrzut flashu (samo znajdzie
  partycję NVS) i sam plik tablicy partycji (0x8000, 0xC00) — wtedy od razu
  powie, pod jakim offsetem jest NVS na tej płytce,
- **„Pokaż na przykładzie”** — wbudowany, sztuczny zrzut
  (`narzedzia/nvs_demo.bin`, dane zmyślone), żeby zobaczyć interfejs bez
  podłączania pieca,
- **„Zapisz JSON”** — eksport do pliku (działa też z `file://`, gdzie
  schowek bywa zablokowany),
- **„Raport”** — gotowy raport tekstowy (`.txt`) do wklejenia w maila albo na
  forum: strony z CRC, tablica partycji, wszystkie klucze z opisami; **sekrety
  w raporcie są zawsze zamaskowane**, nawet gdy na ekranie je odsłonisz,
- **„Porównaj…”** — drugi zrzut (np. „przed” i „po” wgraniu) i tabela różnic:
  co dodane, co usunięte, co zmienione (`55 → 66`); przy tym samym pliku
  napisze wprost „brak różnic”,
- **„Edytuj wartość”** — przy każdym kluczu liczbowym i tekstowym przycisk
  *zmień*: nowa wartość jest sprawdzana z typem (`u8` tylko 0–255, tekst
  najwyżej tyle, ile mieści jeden wpis NVS), a zmiana ląduje w panelu na
  górze z listą „co było → co będzie”,
- **„Pobierz .bin”** — zapisuje **nową partycję NVS** z Twoimi zmianami.
  Zanim odda plik, sama go odczytuje od zera i porównuje: jeśli któraś wartość
  się nie zgadza albo pojawiło się ostrzeżenie, napisze o tym i każe **nie
  wgrywać** pliku. Do tego podaje gotowe polecenie `write-flash` dla offsetu
  z tablicy partycji i przypomina o kopii zapasowej,
- **bajty wpisu** — kliknięcie nazwy klucza rozwija hexdump: 32-bajtowy
  nagłówek wpisu (przestrzeń, typ, span, CRC, klucz, dane) i — przy dłuższych
  wartościach — kolejne wpisy z danymi, z adresami w pamięci.

Ta sama logika parsowania co w `nvs_odczyt.py` (CRC, wpisy, bloby, strony) —
sprawdzone: na pięciu obrazach (w tym wygenerowanych przez Espressif)
**124 klucze zgodne co do wartości**, a obrazy z błędami dają te same
ostrzeżenia. Wersja tekstowa zostaje do skryptów i do `--csv`.

Format, który czyta, jest opisany w nagłówku pliku wraz ze źródłami
(ESP-IDF `nvs_flash`: `nvs_page.hpp/cpp`, `nvs_types.hpp/cpp`,
`nvs_storage.cpp`, `compressed_enum_table.hpp`). Zgodność sprawdzona na
obrazach z oficjalnego `esp-idf-nvs-partition-gen 0.3.0`:

- 48 kluczy w 8 przestrzeniach, typy `u8/i8/u16/i16/u32/i32/string`, teksty
  0–300 B (także dłuższe niż jeden wpis) — co do klucza i wartości zgadza się
  wszystko,
- blob 7 680 B (`file`/`binary` w CSV generatora) — odczytany w całości, równy
  plikowi wejściowemu, złożony z 2 chunków leżących na dwóch stronach,
- CRC wpisów, tekstów i blobów zgadza się bajt w bajt z tym, co liczy ESP-IDF
  (a nie tylko „nie zgłasza błędu”).

### 4.2 Zapis z powrotem do NVS („Pobierz .bin”)

Podglądarka umie też **złożyć partycję od nowa** z poprawionymi wartościami:
`budujPartycje()` w tym samym pliku HTML. Układa wpisy po kolei na stronach,
ustawia stany wpisów w tablicy strony (to nie jest kosmetyka — bez tego NVS
uważa wpisy za puste), liczy CRC wpisów, tekstów i blobów oraz CRC nagłówka
każdej strony. Duże bloby dzieli na chunki po 1 984 B i dopisuje wpis indeksu,
tak jak robi to ESP-IDF.

Jak to jest sprawdzone (wszystko w `node`, bez przeglądarki):

| Test | Wynik |
| --- | --- |
| zrzut → zapis → odczyt tą samą podglądarką, 4 obrazy (demo, 48 kluczy, 13 kluczy, blob) | 23/23 przypadki, **wartości identyczne**, 0 ostrzeżeń |
| to samo, czytane **naszym Pythonem** (`nvs_odczyt.py`) | 0 ostrzeżeń, wartości identyczne klucz w klucz |
| CSV z podglądarki → oficjalny generator Espressifa → nasz czytnik | 25/25 kluczy zgodnych |
| nasz zapis `.bin` vs plik wygenerowany przez Espressifa z tego samego CSV | **0 różnic w wartościach** |
| plik pobrany z interfejsu (po edycji `tempOn` 55 → 66) czytany Pythonem | 41 wpisów, CRC ok, `tempOn = 66`, 0 ostrzeżeń |

Czego zapis **nie** robi: nie zgaduje typu klucza przy zmianie tekstu na inny
typ, nie przenosi starych wpisów „usuniętych” (nowy plik jest uporządkowany),
nie obsługuje partycji, w której wpis przestrzeni leży poza zrzutem. Przy
wgraniu nadpisujesz całą partycję NVS — dlatego najpierw kopia
(`read-flash`), potem `write-flash`.

**Uwaga o CRC.** NVS liczy `esp_rom_crc32_le(0xFFFFFFFF, …)`, a ta funkcja
neguje wejście i wyjście — dlatego wynik to `zlib.crc32(dane, 0xFFFFFFFF)`,
a **nie** `zlib.crc32(dane)`. Ta różnica to najczęstszy powód, dla którego
amatorskie parsery NVS pokazują „złe CRC” na zdrowym urządzeniu.

Jeśli ktoś woli narzędzie Espressif: w ESP-IDF jest `nvs_partition_tool`
(`nvs_tool.py`, tylko Python, bez IDF) — pokazuje to samo, ale nie ma
`--wykryj`, CSV ani maskowania, i wymaga trzech plików `.py` z drzewa IDF
(są w `components/nvs_flash/nvs_partition_tool/`). Nasze narzędzie jest
w repo po to, żeby nie trzeba było niczego ściągać.

---

## 5. Gdy centrala ma hasło w NVS, a Ty chcesz je zmienić

Kolejność ważności w kodzie (`loadFirebaseConfig()`, `BEZPIECZENSTWO_HASLA.md` §5):

1. NVS `firebase` — jeśli komplet (`api_key` ≥ 20 znaków, `email` z `@`,
   `pass` ≥ 8 znaków), używa NVS i **`secrets.h` nic nie zmieni**,
2. `secrets.h` (`FIREBASE_BOOT_*`) — tylko gdy NVS niekompletne; wtedy od razu
   zapisuje je do NVS.

Dlatego po zmianie hasła trzeba albo wysłać `/fbprov|API_KEY|EMAIL|PASSWORD`
na Telegramie, albo wyczyścić całą partycję NVS:

```powershell
python -m esptool --port COM16 erase-region 0x9000 0x5000
```

To **kasuje całe NVS** — razem z WiFi, Telegramem i ustawieniami pieca
(piec wstanie na wartościach domyślnych z kodu). Offset i rozmiar bierz
z tablicy partycji (2.1, krok 1), nie zgaduj. Zrzut z rozdziału 2 pozwala najpierw zobaczyć, co jest, i po próbie
sprawdzić, czy nowe hasło faktycznie weszło.

---

## 6. Lista kontrolna

- [ ] wiem, którą płytkę czytam (centrala czy panel), i offset/rozmiar `nvs`
      mam albo z `partitions.csv`, albo z `--tablica-partycji` (centrala)
- [ ] `read-flash` zakończone bez błędu, plik ma rozmiar równy wielokrotności
      4096 B (0x5000 = 20 480 B = 5 stron)
- [ ] `python narzedzia/nvs_odczyt.py plik.bin` — bez „BLAD”, bez ostrzeżeń
      o złych CRC
- [ ] raport robiony z `--maskuj`, jeśli ma iść dalej niż na mój komputer
- [ ] zrzut **nie** trafia do repo i nie leży w katalogu synchronizowanym
      publicznie (zawiera hasła WiFi, token Telegram, hasło Firebase)
- [ ] jeśli zmieniałem cokolwiek na partycji (`erase-region`,
      `esp_idf_nvs_partition_gen generate`), mam zapisany **wcześniejszy
      zrzut** — bez niego nie ma do czego wrócić
