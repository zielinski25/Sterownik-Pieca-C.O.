# Centrala v3.58.18 — serwis historii i OTA

Poprawka na bazie v3.58.17, **wersja do testów na odłączonej płytce**, nie gotowa do bezwarunkowego wdrożenia do kotłowni. Zachowuje dotychczasową logikę sterowania i interlocki. Instalacja wymaga pełnego, właściwego projektu użytkownika; ta paczka go nie zawiera.

## Co zmieniono

- Stara strona: jawne potwierdzenie zlecenia usunięcia **obu** historii, z ostrzeżeniem o kopii. Żądanie używa istniejącego kanału A1 i wykonuje się **dopiero przy następnym restarcie**, nie restartuje centrali i nie czyści na żywo pamięci RAM. Dotychczasowy `/api/history/clear` pozostaje zablokowany. Dane zbierane między żądaniem a restartem też mogą zostać usunięte.
- Przycisk sprawdzenia pokazuje istnienie znaczników żądania i zakończenia, ale same znaczniki nie stanowią dowodu zakończenia; należy sprawdzić komunikat startowy i pliki. Powtórnego żądania nie składa się, gdy poprzedni plik żądania istnieje. Tymczasowy plik A1 chroniony przed ogólnym edytorem plików.
- Karta OTA starej strony sprawdza najnowszy wpis Releases bezpośrednio z przeglądarki i zleca istniejącym mechanizmom aktualizację centrali lub panelu. Wymaga internetu w przeglądarce dla sprawdzania i internetu centrali dla instalacji. Lista plików nie jest weryfikacją obrazu. Przyjęcie komendy przez LAN i jej wstawienie do kolejki OTA **nie oznacza instalacji**; dalszy wynik trzeba sprawdzić w Telegramie/logach i po restarcie. Błąd pełnej kolejki OTA jest przekazywany zamiast fałszywego sukcesu.
- Poprawiono komunikaty wyłączania symulacji oraz stan przy braku odpowiedzi; wybór trybu serwa jest podświetlany dopiero po pozytywnej odpowiedzi. Brak odpowiedzi oznacza stan nieznany, a nie dowód, że polecenie nie dotarło. Suwak serwa pozostaje poleceniem, nie pomiarem fizycznego ruchu.

## Istotne ograniczenia bezpieczeństwa

**Kod A1/HistoryBoot z pełnego projektu nie był dostępny**: nie można tutaj potwierdzić rzeczywistego odzyskiwania po zaniku zasilania, zachowania LittleFS i wszystkich przejść A1. Nowy interfejs jedynie zleca dotychczasową procedurę; nie twierdzimy, że fizyczne usunięcie sprawdzono. Po utracie odpowiedzi zadanie może już oczekiwać — sprawdź znaczniki i logi przed ponowieniem. Procedura usuwa oba CSV, nie umożliwia niezależnego czyszczenia jednego. Testy hostowe nie zastępują prób restartu i odcięcia zasilania na kopii danych.

Istniejące GitHub OTA w centrali używa `setInsecure()` i **nie weryfikuje certyfikatu TLS**. Nie zapewnia to uwierzytelnienia pobranego firmware; zaufana sieć zmniejsza ryzyko, lecz go nie eliminuje. Zanim użyjesz OTA, potrzebne są weryfikacja certyfikatów i wiarygodne uwierzytelnianie obrazu, a także test rzeczywistego wydania. W tym etapie nie zmieniano mechanizmu flashowania. Nie publikowano Release ani binarek i nie sprawdzono, czy aktualne wydanie zawiera pasujące pliki.

Token LAN pobiera istniejąca strona z centrali; chroni przed zwykłym CSRF (nagłówek bez CORS), **nie uwierzytelnia innych urządzeń w tej samej sieci**. Sieć lokalna musi być izolowana i zaufana. Inne dotychczasowe kanały sterowania mogą mieć osobne zasady bezpieczeństwa. Przycisk potwierdzenia nie jest zabezpieczeniem przed użytkownikiem sieci ani przed innymi kanałami.

## Instalacja i testy

Przed próbą wykonaj kopię całego projektu i obu CSV. Nie formatuj LittleFS i nie uruchamiaj `uploadfs`. Prywatna paczka zawiera `main_centrala.cpp`, `HistoriaExtra.h`, `InternetProbe.h` i `WyborStrony.h`; podmień wszystkie cztery w kopii własnego `centrala/src`. Sekrety, pozostałe nagłówki i konfiguracja PlatformIO nie są dołączone. `zmiany.patch` odnosi się **wyłącznie do maina v3.58.17**, a nie do starszych wersji. Patrz też ograniczenia v3.58.17 dotyczące historii, PSRAM i retencji.

Testy uruchamiane z korzenia repo, gdzie `$MAIN` wskazuje prywatny plik main:

```sh
node firmware-patch/stara-strona-v3.58.18/tests/test_serwis.js "$MAIN"
node firmware-patch/stara-strona-v3.58.18/tests/test_ustawienia.js "$MAIN"
python3 firmware-patch/stara-strona-v3.58.18/tests/test_contract.py "$MAIN"
```

Przeszły testy hostowe funkcji serwisowych (odmowy, token, kolejkowanie, wynik, brak sieci, sprawdzanie wydania), ustawień, HTML i składni skryptu oraz wybrane regresje v13 i v15–17. Starszy test opcji v13 oczekuje 7 serii; od v17 jest ich 8 i należy użyć nowszego testu opcji. Testy z atrapami nie uruchamiają ESP, przeglądarki ani rzeczywistego protokołu A1. **Nie wykonano pełnej kompilacji ESP32, pomiarów pamięci ani testów sprzętowych, przeglądarkowych, zaniku zasilania lub realnego OTA.** Przed użyciem konieczne są próby na odłączonym urządzeniu, w tym restart, awarie zasilania, dane mieszane i wszystkie stany alarmowe; nigdy nie testuj tego najpierw na działającej instalacji.
