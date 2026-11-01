# Etap 219 — plan

## Cel

Jedno miejsce, które przed wgraniem mówi „ten plik jest cały”: buduje plik od
zera z patchy, sprawdza strukturę całego pliku (2,4 MB, 47 tys. linii), stronę
WWW, składnię wszystkich skryptów i obecność wszystkich poprawek z etapów
211–218 — a potem udowadnia, że potrafi wykryć błąd (10 mutantów + 2 stany
historyczne).

## Kolejność pracy (wykonana)

| # | Krok | Stan |
|---|---|---|
| 1 | `audyt.py` — lekser C++, 16 kontroli (B01–B16) | zrobione |
| 2 | `build.py` — trzy stany od zera (v3.55.8, v3.56.3, v3.56.4) + SHA | 3 PASS |
| 3 | audyt na trzech stanach (zielony tylko v3.56.4) | zgodnie z oczekiwaniem |
| 4 | `mutanty.py` — 10 mutacji, każda czerwona | 10/10 |
| 5 | Paczka ZIP + pokwitowanie | patrz `../paczki-udostepnione/` |

## Tabela ryzyk

| Ryzyko | Pokryte | Jak |
|---|---|---|
| plik się nie kompiluje (nawias, raw string) | tak | B02, B03, mutanty M1, M2 |
| panel wypadł ze strony (poza raw stringiem) | tak | B03, B04, M3 |
| strona rozjechana (niedomknięty tag, zepsuty skrypt) | tak | B06, B07, B08, M7, M8 |
| kolejność include'ów cofnięta (błąd D1) | tak | B09, M4 |
| zła wersja w pliku | tak | B11, M5 |
| duplikat globalnej po patchu | tak | B12, M9 |
| trasa znowu na gołym napisie | tak | B13, B14, M6 |
| któraś poprawka zniknęła z pliku | tak | B16 (matryca z bazą) |
| znaczniki `TODO`/konflikt w kodzie | tak | B15 |
| **logika** kodu (czy robi to, co trzeba) | nie tutaj | etapy 211–218 |
| kompilacja ESP/PlatformIO | nie | użytkownik |
| wygląd strony w telefonie | nie | poza zakresem |

## Świadomie poza zakresem

- żadnych zmian w firmware (etap tylko czyta),
- żadnych nowych funkcji,
- brak analizy semantycznej kodu (to nie kompilator; brama sprawdza strukturę),
- brak testów wydajnościowych (to etap 217).
