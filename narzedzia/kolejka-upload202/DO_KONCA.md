# Ile jeszcze do końca tej poprawki historii i plików?

Nie ma obecnie wiarygodnej daty, liczby kolejnych pakietów ani procentu ukończenia.
Przetestowane komponenty nie są równoważne gotowej poprawce całej centrali.

| Obszar | Jest | Do zamknięcia |
|---|---|---|
| Sieć ↔ zapis | Właściciel plików, transakcje historii, stan uploadu, rejestr żądań, kopiująca kolejka; próby komputerowe | Rzeczywisty parser i życie request, budzenie/praca konsumenta, ograniczenia czasu, wysłanie odpowiedzi w kontekście sieciowym, zatrzymanie pobierania przy canSend=false |
| Pobieranie historii | Kontrola długości i błędu, propozycja poprawki pętli odpowiedzi | Stabilne źródło podczas dopisywania/rotacji; długość nie dowodzi niezmienności bajtów; sprawdzenie całej biblioteki i używanych trybów |
| Cała centrala | Zamrożona baza 3.55.8 i sprawdzone interfejsy komponentów | Faktyczna integracja, wszystkie drogi FS, semantyka odmowy u odbiorców, kolejność blokad, FreeRTOS, RAM, priorytety, WDT i czas pracy |
| ESP i wydanie | Brak kwalifikacji urządzenia dla tych komponentów | Pełna kompilacja właściwych zależności, wyniki na ESP, testy zdalnego dostępu i regresji; dopiero potem decyzja o wydaniu |

To cztery obszary, NIE cztery obiecane pakiety. Etap 202 zamyka brak własnej kopii
buforów w testowanym torze, ale nie zamyka jeszcze całego pierwszego obszaru.
Nie podwyższamy wersji firmware ani nie prosimy o wgrywanie niepołączonych elementów.
