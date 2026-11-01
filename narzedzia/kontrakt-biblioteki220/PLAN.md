# Etap 220 — plan

## Cel

Sprawdzić nasz kod na **prawdziwych** nagłówkach biblioteki ESPAsyncWebServer,
a nie na naszych zaślepkach. Etap nie zmienia firmware — jest ostatnią
kontrolą statyczną przed kompilacją u użytkownika.

## Kolejność pracy (wykonana)

| # | Krok | Stan |
|---|---|---|
| 1 | parser klas/deklaracji z prawdziwych nagłówków (z `__attribute__`, `= 0`, `#ifdef`) | zrobione |
| 2 | silnik rozstrzygania przeciążeń (rangi konwersji, domyślne parametry) | zrobione |
| 3 | 7 kontroli Z01–Z07 | 7/7 zielone |
| 4 | tabela 19 wywołań + porównanie z modelem (15 różnic) | udokumentowane |
| 5 | 10 mutantów (w naszym kodzie i w kopiach nagłówków biblioteki) | 10/10 czerwone |
| 6 | Paczka ZIP + pokwitowanie | patrz `../paczki-udostepnione/` |

## Tabela ryzyk

| Ryzyko | Pokryte | Jak |
|---|---|---|
| pole klasy bazowej jest prywatne → nasz konstruktor się nie skompiluje | tak | Z02, mutant M6 |
| nasza sygnatura `override` nie zgadza się z wirtualną | tak | Z03, mutanty M1, M2 |
| wywołanie nie ma żadnego przeciążenia | tak | Z04, mutant M4 |
| wywołanie jest niejednoznaczne (dwa równorzędne przeciążenia) | tak | Z04 (rangi), przegląd 19 wywołań |
| nazwa z zaślepki w kodzie firmware | tak | Z05, mutanty M3, M9 |
| wywołanie funkcji historii, której nie ma w prawdziwym nagłówku | tak | Z06, mutant M5 |
| nowy symbol, którego biblioteka nie ma (`HTTP_ANY`, `exact`) | tak | Z07, mutanty M8, M10 |
| biblioteka podmieniona na inną wersję | tak | Z01, mutant M7 |
| **kompilacja** (składnia, semantyka szablonów, linkowanie) | nie | PlatformIO użytkownika |
| zachowanie w czasie działania | nie tutaj | etapy 211–219 |

## Świadomie poza zakresem

- żadnych zmian w firmware, panelu i nagłówkach,
- brak kompilacji (nie mamy toolchainu ESP w tym środowisku),
- brak analizy Arduino core / FreeRTOS — te symbole są używane od dawna
  w bazowym pliku, który użytkownik kompilował.
