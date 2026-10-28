# 206 — prawdziwe nagłówki żądania i przyjęcie przed pierwszym bajtem pliku

**NIE WGRYWAĆ. release_ready=false.** Brak zmian centrali 3.55.8, SDK i urządzenia. To proponowana, wąska zmiana biblioteki plus testy, nie gotowy firmware.

## Co było brakujące

Etap 205 sprawdził ciało formularza, ale metodę, adres, granicę, długość i nagłówek `Authorization` wstrzykiwał test. Etap 206 uruchamia **prawdziwe, przypięte funkcje** `_parseReqHead`, `_addGetParams`, `urlDecode`, `_parseReqHeader`, `_parseLine`, `AsyncWebHeader::parse` i `stringToMethod`. Udowodniono bajtowo, że 205 ich nie zmienił.

Dodano jeden punkt zaczepienia w bibliotece: **politykę serwera wywoływaną po wyborze handlera, a przed pierwszym bajtem ciała i przed `100 Continue`**. Pozwala ona przyjąć zadanie do puli 204 jeszcze podczas odbioru nagłówków albo odmówić — bez czekania na koniec przesyłania.

## Zachowanie

- `setAttachPolicy206(policy, context)` instaluje jedną politykę na serwer. Druga instalacja jest odrzucana, dopóki nie wywoła się `clearAttachPolicy206()`. Sama funkcja jest wywoływana w kontekście sieciowym.
- Polityka testowa sprawdza metodę i trasę `/upload`, typ `Bearer` i token, kanoniczny `Content-Length` (tylko cyfry, bez znaku, bez zer wiodących, zgodny z długością faktycznie sparsowaną przez bibliotekę) oraz multipart z długością do 8192 B i włącza tryb 205.
- **Zły token:** żądanie jest przejmowane, ale oznaczane jako trywialne — biblioteka nie parsuje ciała, nie woła `handleUpload`, nie dotyka magazynu — i odpowiada 403.
- **Błędna rama** (brak/ujemna/niekanoniczna długość, brak multipartu, zbyt duża długość, sprzeczne duplikaty `Content-Length`): połączenie jest zamykane przed ciałem. To świadoma odmowa bez odpowiedzi 400, bo rama jest niewiarygodna.
- **Poprawny przypadek:** polityka przyjmuje do puli 204 i włącza tryb 205; ciało idzie przez prawdziwy parser, koniec jest potwierdzany przez 205, odpowiedź powstaje po pracy kolejki.

`Content-Length` formularza nie jest długością pliku. Długość pliku jest osobnym parametrem przyjęcia (limit magazynu 4096 B).

## Granice

Modelowane: `String`/`Stream` (host), klient TCP i odpowiedź, dopasowanie trasy (dokładna ścieżka i metoda, nie `AsyncURIMatcher`), kolejność i harmonogram FreeRTOS, cała biblioteka i ESP. Nie ma pomiaru RAM/WDT, zaniku zasilania, usuwania częściowego pliku, niezmiennej historii podczas rotacji ani kwalifikacji urządzenia. `_parseChunkedBytes` i pozostałe ścieżki poza nowym trybem nie są tu testowane. Nagłówki innych żądań (websocket, SSE, pliki statyczne, JSON) nie były przedmiotem tego etapu. Zgodność z proxy i przemycanie żądań nie są kwalifikowane.

Zmiana układu klasy żądania i serwera wymaga pełnej przebudowy biblioteki; nie wolno mieszać starych obiektów z nowymi. Kod biblioteki LGPL-3.0-or-later, licencje zachowane.

## Testy i wyniki

Końcowy run002: **20 grup, 301 instancji** w native i ASan+UBSan z LSan. **11 skompilowanych mutacji wykrytych** (obejmują obejście tokenu, przyjęcie innej trasy, pominięcie trybu 205, niekanoniczną długość, brak przerwania, nietrywialny handler przy odmowie, podmianę polityki, brak jej wywołania, wczesny final i przepisanie długości).

Zakres: podział wejścia w każdym miejscu, odbiór bajt po bajcie, wielkość liter i OWS, granice cytowane/uciekane/zakończone spacjami/o długości 1 i 70, puste i za długie granice, brak multipartu, NUL i nagłówek bez dwukropka, duplikaty `Content-Length`, długości niekanoniczne/ujemne/za duże, `X-Expected-Entity-Length` bez `Content-Length`, `100-continue` (zapis przed ciałem), chunked, rozłączenie w połowie, 50 kolejnych uploadów, odmowa bez zajęcia miejsca, ponowna instalacja polityki, rama dłuższa o 4 bajty, tekst granicy z błędnym sufiksem.

Wcześniejsze przebiegi: run001 miał 18 grup przed dodaniem przypadków ramy i finala. Końcowy wynik to run002.

## Odtworzenie

Linux, Python 3, C++17:

```sh
python3 -B narzedzia/naglowki-upload206/run.py
python3 -B narzedzia/naglowki-upload206/mutacje.py
python3 -B narzedzia/parser-upload205/run.py
python3 -B narzedzia/adapter-upload204/run.py
```

`run.py` najpierw odtwarza kandydata 205, potem nakłada poprawkę 206 i wymaga bajtowej równości z czystych, przypiętych źródeł. Nie są to komendy wgrywania.

Źródła: https://github.com/ESP32Async/ESPAsyncWebServer/blob/c47917dcaba2acefb04277cd679f58a60b4fab30/src/WebRequest.cpp, `WebServer.cpp`, `AsyncWebHeader.cpp`, `literals.h` — kopie i sumy w `upstream/SOURCES.json`.
