Host tests (run from this directory):

  g++ -std=c++11 -Wall -Wextra -Werror AutoIgnitionColdTimerTest.cpp -o /tmp/auto-ignition-test && /tmp/auto-ignition-test
  g++ -std=c++11 -Wall -Wextra -Werror MieszadloTrocinyAutoTest.cpp -o /tmp/mixer-sawdust-test && /tmp/mixer-sawdust-test
  g++ -std=c++11 -Wall -Wextra -Werror AlarmBuzzerProfilesTest.cpp -o /tmp/alarm-buzzer-test && /tmp/alarm-buzzer-test
  g++ -std=c++11 -Wall -Wextra -Werror SolarDailyArchiveTest.cpp -o /tmp/solar-daily-test && /tmp/solar-daily-test

SolarDailyArchiveTest.cpp sprawdza logike dziennego archiwum (v3.51.0) bez Arduino:
granice doby Europe/Warsaw 23/24/25 h, agregat t_bojler, symulacje, dziury,
detekcje poboru CWU, odtworzenie po restarcie, snapshot GHI i URL z granicami UTC.
Naglowek ../SolarDailyArchive.h nie zawiera I/O ani sieci.
