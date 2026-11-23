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
   pip install esptool
   esptool.py --port COM5 read_flash 0x9000 0x5000 nvs_centrala.bin
   python3 narzedzia/nvs_odczyt.py nvs_centrala.bin
   ```

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
   popraw polecenie. Gdy nie wiesz: `--wykryj` znajduje partycję NVS w zrzucie
   całego flashu po nagłówkach stron i podaje jej offset.

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
   `BEZPIECZENSTWO_HASLA.md` §5 mówi, dlaczego zmiana `secrets.h` nie zmienia
   hasła w piecu, który ma je już w NVS — dokładnie dlatego warto umieć to
   odczytać.

---

## 2. Skąd wziąć zrzut (trzy drogi + jedna obejściowa)

### 2.1 Kablem USB — najprościej, czyta wszystko

```
pip install esptool
esptool.py --port COM5 read_flash 0x9000 0x5000 nvs_centrala.bin     # Windows
esptool.py --port /dev/ttyUSB0 read_flash 0x9000 0x5000 nvs_centrala.bin   # Linux
```

- `0x9000` = początek partycji `nvs`, `0x5000` = jej długość (20 480 B = 5 stron).
- Dla panelu LCD: `LCD-Centrali/partitions.csv` ma dokładnie te wartości.
  Dla centrali sprawdź jej plik partycji (patrz punkt 2 wyżej).
- Gdy nie wiesz, gdzie NVS leży w danej płytce: zrzuć cały flash
  (`esptool.py read_flash 0 0x1000000 caly.bin` — 16 MB na N16R8, chwilę trwa)
  i uruchom `python3 narzedzia/nvs_odczyt.py caly.bin --wykryj`.
- Port: w Menadżerze urządzeń (Windows) albo `ls /dev/ttyUSB* /dev/ttyACM*`
  (Linux). Na płytkach z USB-CDC bywa `COM` inny niż przy CH340.

### 2.2 Z Linuxa bez esptool (awaryjnie)

`pip` bywa niedostępny na maszynie produkcyjnej, a `esptool` jest w każdym
PlatformIO: `~/.platformio/packages/tool-esptoolpy/esptool.py`. To ten sam
program — wystarczy wywołać go ścieżką.

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
python3 narzedzia/nvs_odczyt.py nvs_centrala.bin
python3 narzedzia/nvs_odczyt.py nvs_centrala.bin --maskuj --pokaz-usuniete
python3 narzedzia/nvs_odczyt.py nvs_centrala.bin --json > nvs.json
python3 narzedzia/nvs_odczyt.py nvs_centrala.bin --csv nvs.csv
python3 narzedzia/nvs_odczyt.py caly_flash.bin --offset 0x9000 --rozmiar 0x5000
python3 narzedzia/nvs_odczyt.py caly_flash.bin --wykryj
python3 narzedzia/nvs_odczyt.py --autotest
```

- czyta **całą** partycję niezależnie od firmware'u — także klucze, których
  dana wersja programu już nie używa, i wpisy **usunięte** (zostają w pamięci
  do przepisania strony),
- sprawdza CRC nagłówków stron, CRC wpisów, CRC tekstów i blobów — mówi,
  które wpisy NVS sam by odrzucił,
- `--wykryj` znajduje partycję w zrzucie całego flashu (po nagłówkach stron),
- `--csv` daje plik w formacie wejściowym narzędzia Espressif
  (`esp-idf-nvs-partition-gen`), więc z odczytu da się **odtworzyć** partycję
  na drugą płytkę (`python -m esp_idf_nvs_partition_gen generate nvs.csv
  nvs.bin 0x5000`); bloby idą jako `hex2bin`, więc wiersz np. `otapanel:zapis`
  ma kilkaset bajtów szesnastkowych w jednej komórce — to normalne,
- `--autotest` sprawdza samo narzędzie na obrazach budowanych w pamięci
  (29 przypadków: liczby, teksty krótsze i dłuższe niż wpis, blob
  wielostronicowy, wpis usunięty, chunk bloba bez indeksu, zepsute CRC,
  zły offset, wykrywanie w obrazie flashu, CSV).

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
na Telegramie, albo wyczyścić przestrzeń `firebase` (np. `esptool.py
erase_region 0x9000 0x5000` — **kasuje całe NVS**, więc razem z WiFi,
Telegramem i ustawieniami pieca; piec wstanie na wartościach domyślnych
z kodu). Zrzut z rozdziału 2 pozwala najpierw zobaczyć, co jest, i po próbie
sprawdzić, czy nowe hasło faktycznie weszło.

---

## 6. Lista kontrolna

- [ ] wiem, którą płytkę czytam (centrala czy panel) i z którego `partitions.csv`
      biorę offset i rozmiar `nvs`
- [ ] `esptool.py read_flash …` zakończone bez błędu, plik ma rozmiar
      równy wielokrotności 4096 B
- [ ] `python3 narzedzia/nvs_odczyt.py plik.bin` — bez „BLAD”, bez ostrzeżeń
      o złych CRC
- [ ] raport robiony z `--maskuj`, jeśli ma iść dalej niż na mój komputer
- [ ] zrzut **nie** trafia do repo i nie leży w katalogu synchronizowanym
      publicznie (zawiera hasła WiFi, token Telegram, hasło Firebase)
