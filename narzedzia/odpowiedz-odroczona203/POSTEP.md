# Stan po 203

Przetestowany tor: kopia callbacku w 202 -> zapis w pracowniku przez 201/200/197 ->
odbiór wyniku w kontekście request -> wyodrębnione send/_send biblioteki.
Zniszczenie request, gdy pracownik jeszcze działa, nie wymaga jego wskaźnika
w pracowniku. Niepewne uchwyty pozostają pod kontrolą wcześniejszych komponentów.

Nie jest to jeszcze zintegrowana centrala ani pełny serwer. Obszar sieć↔zapis
ma teraz sprawdzony pomocnik odpowiedzi i propozycję hooka, ale wciąż brakuje
rzeczywistego adaptera parsera i pomiarów na docelowym stosie. Nie zamknięto także
spójnego pobierania podczas rotacji, pełnej integracji i kwalifikacji ESP.
Nadal nie ma podstaw do daty końca ani przeliczenia obszarów na liczbę pakietów.
