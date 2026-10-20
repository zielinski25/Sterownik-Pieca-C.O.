Host tests (run from this directory):

  g++ -std=c++11 -Wall -Wextra -Werror AutoIgnitionColdTimerTest.cpp -o /tmp/auto-ignition-test && /tmp/auto-ignition-test
  g++ -std=c++11 -Wall -Wextra -Werror MieszadloTrocinyAutoTest.cpp -o /tmp/mixer-sawdust-test && /tmp/mixer-sawdust-test
  g++ -std=c++11 -Wall -Wextra -Werror AlarmBuzzerProfilesTest.cpp -o /tmp/alarm-buzzer-test && /tmp/alarm-buzzer-test
  g++ -std=c++11 -Wall -Wextra -Werror SolarDailyArchiveTest.cpp -o /tmp/solar-daily-test && /tmp/solar-daily-test

Testy logiki archiwum (bez Arduino):
  g++ -std=c++11 -Wall -Wextra -Werror SolarDailyArchiveTest.cpp -o /tmp/solar-daily-test
  /tmp/solar-daily-test        -> PASS (339 sprawdzen)
  Sprawdza granice doby Europe/Warsaw 23/24/25 h, agregat kanalu t_panel,
  symulacje, dziury, detekcje poboru, odtworzenie po restarcie, snapshot GHI,
  URL z granicami UTC, format linii raportu, godziny min/max, prog "za malo
  probek", rozroznienie "prognoza na jutro" / "brak probek" / "brak odczytow".

SYMULATOR RAPORTU (v3.53.0) -- uruchom PRZED kazda prosba o wgranie firmware:
  g++ -std=c++11 -Wall -Wextra -Werror -I.. ReportSimulation.cpp -o /tmp/reportsim
  /tmp/reportsim              -> WYNIK OK: 3559 sprawdzen, 0 bledow
  /tmp/reportsim --verbose    -> pelna tresc kazdej wiadomosci

  Sklada CALE wiadomosci Telegrama tym samym kodem, ktory jest w firmware
  (solar_daily::buildReport + splitForTelegramParts z ../SolarDailyArchive.h)
  i sprawdza KAZDA wiadomosc osobno: <= 4096 znakow, niepusta, prefiks
  "(ciag dalszy)" tylko na wiadomosciach 2..N, zaden znak nie ginie:
  swiezy start, 00:05, prog 30 probek, poludnie, wieczor, doba zamknieta,
  dziura, symulacje, tryb "spuszczona", rekord CO, doba 23/25 h, 30 dni,
  awaria GHI, brak zegara, pusty katalog -- plus przemiatanie calej doby co
  10 minut (145 wariantow) i kazdej liczby dni 1..30 (30 wariantow), oba
  z pelnym sprawdzeniem podzialu. Drukuje granice wiadomosci i ich rozmiary.

  Dlaczego to istnieje: wersje v3.52.7-v3.52.11 naprawialy rzeczy widoczne
  dopiero w gotowej wiadomosci, bo testy sprawdzaly tylko pojedyncza linie.
  Kazda taka poprawka kosztowala jedna aktualizacje OTA.

Naglowek ../SolarDailyArchive.h nie zawiera I/O ani sieci -- dzieki temu ten
sam kod dziala na ESP32 i na komputerze.
