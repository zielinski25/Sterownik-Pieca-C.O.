# 199 — plan przed kodem

Zakres: rzeczywista pętla wysyłania odpowiedzi przypiętej biblioteki, walidacja
rozmiaru i ponowna kontrola źródła po odczycie, również przed wysłaniem bufora.
Zatrzymanie zgłoszone z innego zadania nie wykonuje close klienta: zamknięcie
należy do kontekstu wywołującego pętlę odpowiedzi. Nie podmieniać callbacków TCP.

Najpierw ustalić API, potem osobna propozycja poprawki biblioteki i testy
wyodrębnionej rzeczywistej metody. Nie udawać pełnego serwera/AsyncTCP/ESP.
Sprawdzany odczyt plikowy bez szablonów; template path pozostaje poza kwalifikacją.
Testy: błędy podczas odczytu, za krótki plik, nieprawidłowy rozmiar, bufor oczekujący,
backpressure, dodatnie przebiegi, natychmiastowy i późniejszy stop, mutacje.
Nie zmieniać main 3.55.8 ani zamrożonych 195–198. Bez SDK, urządzenia i OTA.
