# Testy plików HTML (prawdziwy DOM)

Po co: `porownanie-wykresow.html` i `Piec.html` to pliki, których **nie da się
skompilować** — błąd wychodzi dopiero w przeglądarce. Ten test uruchamia plik
w prawdziwym DOM (`jsdom`), ze zaślepką sieci, i sprawdza zachowanie.

Uruchomienie (Windows, PowerShell — w katalogu repo):

```
cd narzedzia\testy-html
npm init -y
npm install jsdom
node test-porownanie-jsdom.js
```

Na końcu ma wyjść **`WYNIK jsdom: 34 OK, 0 FAIL`**. Kod wyjścia 0 = wszystko gra.

Inny plik niż domyślny:

```
$env:PLIK = "C:\sciezka\do\pliku.html"; node test-porownanie-jsdom.js
```

## Co ten test sprawdza (stan 31.10.2026, `porownanie-wykresow.html` v2)

| grupa | sprawdzenia |
|---|---|
| struktura | wszystkie 37 elementów, których szuka skrypt, istnieje w pliku |
| start bez sesji | brak zapytań do sieci, ostrzeżenie „wpisz hasło", sekcja konta rozwinięta |
| start z sesją | odświeżenie tokenu (bez hasła), pobranie bazy, rysowanie, statystyki, tabela kanałów |
| tryby | kliknięcie każdego z 3 przycisków bez błędu, poprawny tryb po kliknięciu |
| wylogowanie | czyści token i hasło z przeglądarki, rozwija sekcję konta |
| token martwy | najpierw odświeżenie, potem logowanie zapamiętanym hasłem |
| złe hasło | czytelny komunikat, brak danych, sekcja konta rozwinięta |

## Czego ten test NIE sprawdza

- **prawdziwego rysowania** — `jsdom` nie ma canvasa; liczymy wywołania rysowania
  na zaślepce, więc błąd w geometrii wykresu tego nie złapie,
- **prawdziwego Google** — sieć jest zaślepiona; zła nazwa użytkownika/hasła
  wyjdzie tylko u Ciebie,
- wyglądu (CSS) — `jsdom` nie liczy układu.

Pełny test to i tak otwarcie pliku w przeglądarce i spojrzenie na wykresy.
