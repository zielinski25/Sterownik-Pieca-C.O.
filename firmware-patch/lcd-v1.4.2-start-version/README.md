# LCD v1.4.2 — wersja na ekranie startowym

Bazą jest LCD v1.4.0 z Dysku wraz z poprzednią poprawką ikon v1.4.1. Podniesiono `FIRMWARE_VERSION_PANEL` do **v1.4.2** i dodano wpis do historii zmian w `main.cpp`. To zmodyfikowany kod źródłowy, nie oficjalne wydanie ani gotowy firmware.

## Instalacja

[Pobierz ZIP z plikami LCD](pobierz/LCD-v1.4.2-pliki-do-podmiany.zip). Zrób kopię zapasową projektu LCD. Skopiuj cztery pliki z `lcd/src` w ZIP-ie do `src` projektu LCD v1.4.0 lub v1.4.1: `main.cpp`, `dashboard_lvgl_poc.cpp`, `dashboard_lvgl_poc.h` i `SiecIkony.h`. Trzy pierwsze zastąp, czwarty dodaj lub zastąp. Pliki zawierają także poprzednią poprawkę ikon.

Nie nadpisuj `src/secrets.h`, `platformio.ini` ani innych plików projektu. Jeśli masz własne późniejsze zmiany w tych czterech plikach, porównaj je przed podmianą. Nie kopiuj plików do projektu centrali — **centrala pozostaje v3.58.7**, ta zmiana dotyczy tylko LCD.

Gdy projekt ma już dokładnie v1.4.1 z poprzedniej paczki i wolisz minimalną zmianę, zamiast kopiować cztery pliki możesz zastosować `lcd-v1.4.1-do-v1.4.2.patch` do katalogu projektu LCD. Nie stosuj obu metod jednocześnie.

## Działanie

Po uruchomieniu lub wybudzeniu panel pokazuje przez 2,5 sekundy `Panel LCD` i wersję z rzeczywistej stałej firmware, również bez połączenia z centralą. Po automatycznym restarcie z zapamiętanym aktywnym alarmem ekran startowy jest pomijany, aby nie opóźniać obsługi alarmu. Normalny start jest dłuższy o około 2,5 sekundy. Nie ma zmian w centrali ani w logice OTA.

## Weryfikacja

Pełne testy hostowe LCD przeszły, w tym scenariusze wyświetlenia wersji z centralą i bez niej, pominięcia ekranu przy alarmie i OTA. Test symulatora można odtworzyć na projekcie z oryginalnego ZIP-a LCD v1.4.0: skopiuj cztery pliki jak wyżej, a następnie zastosuj `tests/symulator-z-v1.4.0.patch` do katalogu projektu i uruchom `testy_host/uruchom_testy.sh`. Testy wymagają Linux/WSL, kompilatora hostowego, LVGL i ArduinoJson jak w instrukcji oryginalnego projektu.

**Nie wykonano kompilacji dla ESP32 ani testów na sprzęcie.** Przed użyciem przy instalacji grzewczej sprawdź panel w bezpiecznych warunkach i zachowaj poprzedni firmware.
