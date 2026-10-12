# Firmware v3.38.0 — patch źródłowy

Pakiet zawiera:

- `main_centrala.cpp` — pełne źródło centrali przygotowane dla v3.38.0;
- `main_centrala.cpp.patch` — różnicę względem `src/main_centrala.cpp` z pliku `Cenatrala Pieca.zip`;
- ten README z zakresem zmian i statusem weryfikacji.

## Zakres

- Nowa, osobna funkcja „Rozpalanie” dla serw: automatyczne przygotowanie poniżej 25°C, sekwencja syberek → klapa, aktywność do przekroczenia 30°C i wznowienie dotychczasowego trybu z bieżących pozycji.
- Trwałość w NVS stanu aktywnego, dostępności zimnego okna i blokady ponownego automatycznego otwarcia po alarmie dymu.
- Nadzór czujnika dymu przez pięć minut od ruchu serwa/kliknięcia; po alarmie do późniejszego z terminów: pięć minut od ostatniego ruchu lub trzy minuty od skasowania alarmu. Alarm można skasować wyłącznie świeżym pomiarem poniżej progu z histerezą.
- Bezpieczne zatrzymanie wymuszenia przy alarmie dymu, klapa zamykana i syberek otwierany; w zwykłym alarmie syberek wykonuje początkowy skonfigurowany krok oraz kolejne kroki co trzy minuty.
- Wspólny kontrakt Firebase `alarm_mute <dym|ogrz|oba> <sekundy|off>` oraz pola statusu wyciszenia, zgodne z aplikacją Android.
- Status JSON i komenda Firebase `rozpalanie_serwa`; komendy ręczne `klapa`/`syberek` nie nadpisują aktywnego wymuszenia.

## Użycie patcha

1. Rozpakuj `Cenatrala Pieca.zip` do osobnego katalogu roboczego. Nie edytuj ani nie podmieniaj ZIP-a w głównym checkoutcie.
2. W katalogu zawierającym `platformio.ini` zastosuj:

   ```sh
   patch -p1 < main_centrala.cpp.patch
   ```

   Patch oczekuje pliku `src/main_centrala.cpp` i nie zmienia pozostałych plików archiwum.
3. Zbuduj firmware bez uploadu, używając skonfigurowanego środowiska PlatformIO. W tej sesji nie wykonano flashowania ani wysyłania poleceń do pieca.

## Weryfikacja w tej sesji

- `git diff --check` dla zmian Androida: bez błędów.
- APK i testy Androida nie zostały zbudowane: środowisko nie ma Javy (`JAVA_HOME` i `java` nie są dostępne).
- PlatformIO zainstalowano w tymczasowym środowisku, ale `pio run -e esp32-s3-n16r8` nie doszedł do kompilacji: pobieranie platformy wymagało hosta `release-assets.githubusercontent.com`, niedostępnego w środowisku sieciowym.
- Nie uruchamiano `pio run -t upload` ani `pio run -e esp32-s3-ota -t upload`.
