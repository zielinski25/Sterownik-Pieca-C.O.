# Raport bramy przed wgraniem (etap 214)

Plik po patchu: `main_centrala-v3.55.8-to-v3.56.2.patch`
SHA-256 wyniku: `369026dbe324a05805518b22f1e12eb950b5abffa6d17c65e3c32fd8762cf800`

| # | kontrola | wynik | szczegoly |
|---|---|---|---|
| S01 | FIRMWARE_VERSION = v3.56.2 dokladnie raz | OK | linie: [5591] |
| S02 | kazdy include etapow 211-213 wystepuje raz | OK | 3/3 po jednym |
| S03 | naglowki HTTP PO <ESPAsyncWebServer.h> | OK | kolejnosc prawidlowa |
| S04 | kazdy nowy symbol globalny ma 1 definicje | OK | 8/8 po jednej |
| S05 | domyslny token pusty (pobieranie wylaczone bez secrets.h) | OK | znaleziono |
| S06 | secrets.h.example: zastepnik, nie sekret | OK | czysto |
| S07 | w repo nie ma pliku secrets.h | OK | brak |
| S08 | rejestracje: /history ANY, /history/long ANY, /history/link GET | OK | 3/3 |
| S09 | panel w literalzie HTML, bez tokenu, z biletem | OK | przycisk 2611 znakow |
| S10 | patch odwracalny (--dry-run -R) | OK | odwraca sie czysto |
| S11 | bez CRLF i bez BOM | OK | czysto |
| S12 | naglowki firmware bez STL (tylko 6 dozwolonych) | OK | czysto |
| S13 | naglowki bez wyjatkow, RTTI i std::string | OK | czysto |
| S14 | oba wysylacze Telegramu nadal na zapince 211 | OK | 2 wysylacze x (pin.open + pin.close) |
| S15 | historiaSys zdefiniowany przed bramka | OK | linia 22826 < 28696 |
| S16 | esp_system.h przed pierwszym esp_random() | OK | include linia 5163, uzycie linia 19078 |
| S17 | naglowki kompiluja sie jako gnu++11 i gnu++17 | OK | oba przeszly |
| S18 | sekcja tokenu dokladnie jedna | OK | 1 |

Bledy: 0
