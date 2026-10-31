#197 — plan przed kodem

Zakres: kompletny, osobny mechanizm właściciela operacji i uchwytów Arduino FS,
nie kolejna częściowa flaga w centrali.196/main3.55.8 pozostaje zamrożony.

- Wszystkie FS& i File mają proxy; nie oddawać backendu ani jego File.
- Jedna blokada właściciela, osobne atomowe zatrzymanie. Operacja przyjęta przed
  zatrzymaniem może się zakończyć. Po odczekaniu właściciela nie ma dalszego I/O.
- Stała pula slotów i bloków shared_ptr; rezerwacja PRZED open/openNextFile.
- Destruktor proxy po błędzie nie wykonuje close: plik zostaje w slocie do resetu.
  Cały właściciel i jego zależności mają trwać do końca uruchomienia, bez destrukcji.
- Kopie File, jawny close jednej kopii, ostatni destruktor, ponowne użycie slotu,
  katalogi, błąd podczas open/close i równoległe operacje wymagają testów.
- Osobna dzierżawa właściciela dla istniejącego sprawdzanego toru publicznych FD.
  Nie wolno w niej ponownie wywoływać proxy File (blokada nierekurencyjna).
- Nie udawać detekcji błędów void Arduino close/flush. Zatrzymanie musi dostawać
  wiarygodny sygnał; można samodzielnie wychwycić krótki write.
- Sprawdzić też semantykę błędu read w rzeczywistym FS.cpp i konsekwencje HTTP.
- Dopiero po przeglądzie wszystkich uchwytów, portu blokady FreeRTOS, kolejności
  logMutex/owner i transportu wolno przygotować integrację centrali.

Bez urządzenia, resetu, wznowienia, A1 i zmiany wersji CPP. Ten pakiet nie obiecuje
rozwiązania ciągłości rejestracji ani pełnej izolacji wszystkich bibliotek ESP.
