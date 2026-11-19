# Centrala 3.57.13 — Zew 1 i pokój 12 bitów

Zmiana względem dokładnego kandydata 3.57.12: `sensors1.begin(9)` → `sensors1.begin(12)`. Magistrala GPIO4 obejmuje DS18B20 Zew 1 i pokój. W 12 bitach krok wynosi 0,0625°C; nie jest to gwarancja większej dokładności. Zew 2 to DHT22 i nie zmienia się. CO, powrót, trociny, bojler i panel zachowują dotychczasową konfigurację (10 bitów). Interwały odczytów, wysyłka Firebase i format telemetrii nie są zmienione. Rozdzielczość ustawiana jest podczas inicjalizacji magistrali; rzeczywiste działanie zależy od biblioteki i czujników w projekcie.

Scheduler już czeka co najmniej 750 ms na konwersję, więc kod zachowuje zapas potrzebny dla nominalnego maksimum 750 ms DS18B20 12 bit. Odczyt pokoju i Zew 1 nadal co ok. 120 s. W Firebase / interfejsach wartości nadal w dziesiątych stopnia — nie widać pełnych kroków 0,0625°C.

**Istotne:** konfiguracja biblioteki i kopii EEPROM w rzeczywistych czujnikach nie została zweryfikowana. W publicznej bibliotece nettigo `begin()` zapisuje ustawienie czujników przy starcie; po fizycznym odcięciu zasilania przez istniejący mechanizm power-cycle kod nie wywołuje ponownie `begin()`. Czujnik może wtedy przywrócić rozdzielczość z własnej EEPROM. Praca 12-bit po takim zdarzeniu nie jest potwierdzona; nie zapisywano nic do EEPROM i nie zmieniano logiki obsługi awarii. Do zweryfikowania na sprzęcie i w używanej bibliotece.

## Pliki

- `src/main_centrala.cpp` — pełne źródło kandydata, bez pozostałych plików projektu.
- `centrala-3.57.12-do-3.57.13.patch` — poprawka dla dokładnego pliku 3.57.12 z poprzedniego kandydata (SHA-256 po normalizacji CRLF do LF: `ea6256bebe8bdf370388fc80bd61231f7297a09c4dbe02f52e77167f31d8d2f2`). Sam numer wersji nie gwarantuje zgodności pliku.
- `buduj-3.57.13.ps1` — lokalna poprawka z tej paczki; kontrola sum, kopia zapasowa, kompilacja w `Documents\PlatformIO\sterownik-co-esp32` dla `esp32-s3-n16r8`, kopia BIN na pulpit. Nie wgrywa i nie publikuje OTA. Jeśli hash źródła się nie zgadza, zatrzymuje się bez zmian. Nie omijać kontroli; ustalić różnicę.

## Windows

Rozpakuj wszystkie pliki do jednego folderu (z zachowaniem struktury), następnie w PowerShell, z tego folderu:

```powershell
powershell -ExecutionPolicy Bypass -File .\buduj-3.57.13.ps1
```

Po poprawnej kompilacji na pulpicie powstaje `SterownikCO-3.57.13-KANDYDAT.bin`. Samo zbudowanie i skopiowanie pliku niczego nie wgrywa. Weryfikacja na sprzęcie, możliwość przywrócenia poprzedniej wersji i nadzór instalacji są istotne przed wdrożeniem. Po restarcie potwierdź `/version` oraz obserwuj Zew 1 i pokój przez kilka kolejnych odczytów; wartości z jedną cyfrą po przecinku nie dowodzą samodzielnie 12 bitów. Odczyt rejestru konfiguracyjnego byłby rozstrzygający.

## Testy i ograniczenia

Wykonano testy hostowe fragmentów harmonogramu DS, jakości, DHT/BMP, filtra i telemetrii z poprzedniego kandydata oraz kontrolę zmiany, zastosowania i odwrócenia patcha. To nie jest pełna kompilacja firmware ani test fizycznego ESP, czujników, automatyki i alarmów. Używana fizycznie wersja biblioteki DS18B20 nie została tu potwierdzona. Wszystkie uwagi bezpieczeństwa i ograniczenia 3.57.12 nadal obowiązują; wersja 3.57.13 ich nie zmienia.
