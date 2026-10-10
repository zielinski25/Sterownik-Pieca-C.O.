# Raport agenta — poprawki logów ESP32-S3

**Data:** 2026-10-10

**Wersja źródłowa:** `v3.35.2`

**Zakres źródłowy:** `Cenatrala Pieca.zip` → `src/main_centrala.cpp`. Archiwum zostało zaktualizowane; rozpakowany katalog roboczy usunięto po edycji. Nie dodano rozpakowanych plików firmware do repo.

## Wykonane

1. **§5.1 — log TG-CB cooldown:** w `TG_NET_FAIL()` pole `cooldown=` korzysta teraz z `tgNetBackoffMs / 1000UL`, a nie z absolutnego `tgNetCooldownUntilMs / 1000UL`. Znacznik `tgNetCooldownUntilMs` nadal służy bramce czasu. Potwierdzono brak `tgNetCooldownUntilMs / 1000UL` w źródle.
2. **§5.2 — zamykanie epizodów Firebase:** dodano deklarację `fbCbSkipEpisodesReset()`, wywołanie jej w `FB_NET_SUCCESS()` przed resetem streaka oraz definicję po deklaracjach obu stanów `LogThrottleStan`. Funkcja zamyka aktywne `skip_status` i `skip_cmd` przez istniejący `logThrottleWyczysc()`; nieaktywny stan pozostaje no-op.
3. `FIRMWARE_VERSION` podniesiono z `v3.35.1` do `v3.35.2`; na początku `main_centrala.cpp` dodano wpis changelogu z osobnymi podpunktami `[2.1]` i `[2.2]`, datą i polami Co/Problem/Przyczyna/Naprawa.

### Pliki i lokalizacje w `src/main_centrala.cpp`

- changelog: linie 1–18;
- `FIRMWARE_VERSION`: linia 4948;
- prototyp `fbCbSkipEpisodesReset()`: linia 5531;
- makro `FB_NET_SUCCESS()` i wywołanie resetu epizodów: linie 5563–5569;
- makro `TG_NET_FAIL()` z poprawionym argumentem: od linii 5600;
- stany skip Firebase: linie 27180 i 27600;
- definicja `fbCbSkipEpisodesReset()`: linie 27602–27605.

Źródło znajduje się wewnątrz ZIP-a — powyższe linie odnoszą się do wersji zaktualizowanej w `Cenatrala Pieca.zip`.

## Weryfikacja

- `unzip -t "Cenatrala Pieca.zip"`: **PASS**, archiwum jest poprawne.
- Porównanie wpisów ZIP: **PASS**, zachowano listę 13 wpisów; zmieniono wyłącznie `src/main_centrala.cpp`.
- Kontrole statyczne: **PASS** — wersja i data changelogu, poprawny argument `cooldown`, brak starego formatowania `tgNetCooldownUntilMs / 1000UL`, reset obu epizodów przed wyzerowaniem stanu breakera oraz umiejscowienie helpera po obu stanach.
- Build `cd firmware && pio run -e esp32-s3-n16r8`: **nie uruchomił się** — `pio: command not found` (exit 127). Kompilacji nie należy uznawać za zaliczoną; PlatformIO nie instalowano.
- Nie flashowano urządzenia, nie uruchamiano uploadu/OTA i nie testowano na sprzęcie.

## Status pozostałych punktów §5

- **§5.3:** bez zmian; wymaga dalszej analizy siedmiu sekcji mutexów i potwierdzenia bezpieczeństwa pominięcia zapisu przy timeout.
- **§5.4:** bez zmian; nie zmieniano restartów, RTC RAM ani ścieżek OTA — czeka na decyzję/człowieka.
- **§5.5:** bez zmian; nie dodawano kolejnych logów checkpointu ze względu na wymóg zgody i wpływ na LittleFS.
- **§5.6:** bez zmian; reinit Firebase pozostaje bez zmian, ponieważ wymaga testu sieciowego.
- **§5.7:** bez zmian w kodzie; cache DNS Telegrama pozostaje propozycją do osobnego przeglądu.
- **§5.8:** niczego nie przenoszono i nie zmieniano w README/platformio.ini. Do osobnego zgłoszenia: README odwołuje się do nieobecnych w repo katalogów `firmware/` i `panel/`; `platformio.ini` zawiera jawne dane autoryzacji OTA i stały adres IP — wartości nie są tu powielane. Warto zaplanować bezpieczne przeniesienie po uzgodnieniu provisioning/konfiguracji, bez usuwania ich w tej poprawce.
- Opcjonalnego resetu epizodu TG z §5.2 nie dodano: warunek „tylko jeśli kompiluje się bez ostrzeżeń” nie może być potwierdzony bez PlatformIO.

## Git i dostarczenie

- Utworzone lokalne commity na bieżącej gałęzi:
  - `3fe542a` — `fix(tg): log TG-CB pokazuje czas cooldownu (v3.35.2 [2.1])`
  - `83c903c` — `fix(fb): zamknij epizody skip po resecie breakera (v3.35.2 [2.2])`
- Gałąź pozostała `arena/539ab1b6-sterownik-pieca-c-o`. W tym środowisku gałąź sesji jest stała, więc nie utworzono wskazanej w zadaniu `fix/v3.35.2-fb-tg-throttle-logs`.
- Niczego nie wypchnięto. Przed użyciem OTA potrzebny jest build oraz osobny, uzgodniony release/tag `v3.35.2` zgodny z wersją w źródle.

## Odpowiedzi i nadal otwarte pytania (§7)

- **Restarty SW (2026-10-06, ok. 14:49, 14:57 i 15:27):** użytkownik ocenia, że prawdopodobnie wynikały z OTA. To pozostaje hipotezą, nie potwierdzeniem konkretnej operacji; log zawiera `powod=SW`, a boot 8 potwierdza zatwierdzenie partycji OTA.
- **Pompa OFF we wszystkich próbkach:** użytkownik potwierdził, że w piecu nie palono, więc wyłączona pompa była oczekiwana. Nie otwieramy osobnego problemu pompy.
- **Stall TG (2026-10-06, ok. 16:45–20:03):** nadal otwarte — czy zachował się checkpoint `diag` z czasu stallu albo core dump?
