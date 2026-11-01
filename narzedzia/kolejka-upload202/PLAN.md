# 202 — plan przed kodem

Ograniczona kolejka posiadająca kopie nazw i danych z callbacku; brak zapisu
wskaźników do buforów sieciowych. Cała porcja mieści się albo jest odrzucona,
bez cichego przyjęcia połowy. Przepełnienie kończy żądanie błędem, nie retry
nietrwałego wskaźnika. Obsługa kolejki nie jest implementacją pause/backpressure TCP.

Jeden konsument wykonuje Registry201/Pool200 poza blokadą kolejki. Przyjęcie,
koniec żądania i disconnect nie wykonują I/O. Zakończenie i miejsce na odpowiedź
rezerwowane w rekordzie żądania, niezależnie od pełnej kolejki danych.
Disconnect unieważnia oczekujące dane i odpowiedź; trwająca operacja może skończyć.
Sprzątanie tylko w konsumencie. Stare tokeny i ponowne attach tego samego klucza
nie mogą trafić do nowego żądania ani skasować błędu starego.

Testy z rzeczywistymi komponentami 197/200/201 i FS.cpp. Kopiowanie, podział
porcji, przepełnienie, dwa żądania, przerwanie, dwa wywołania konsumenta, mutacje,
sanitizery, odtworzenie ZIP. Bez pełnego parsera, SDK, FreeRTOS i urządzenia.
Nie zmieniać main 3.55.8 ani zamrożonych etapów. Nie obiecywać daty końca.
