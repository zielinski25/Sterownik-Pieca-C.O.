# Plan po 210 — dlaczego 211, a nie od razu trasa HTTP

## Kolejność wynika z audytu 210, nie z upodobania

210 pokazał, że pliki historii czyta w 3.55.8 **osiem wywołań w czterech funkcjach** i że dwa
wysyłacze Telegramu robią to bez blokady i bez zapinki. Trasa pobrania z 209 (zapinka 207 + adapter
208 + decyzja o żądaniu) jest gotowa, ale **nie była jeszcze wpisana do centrali** — a wpisanie jej
bez naprawy wysyłaczy znaczyłoby: nowa, bezpieczna trasa obok dwóch starych, niebezpiecznych.

Dlatego kolejność jest taka:

| etap | co | stan |
|---|---|---|
| 207 | zapinka prefiksu historii (offline) | **gotowe, spakowane** |
| 208 | adapter pobrania + prawdziwa pętla 199 (offline) | **gotowe, spakowane** |
| 209 | trasa decyzyjna `/history` (offline) | **gotowe, spakowane** |
| 210 | audyt: kto dotyka plików historii w 3.55.8 | **gotowe, spakowane** |
| **211** | **oba wysyłacze TG na zapince — patch v3.55.9 + testy** | **ten etap** |
| 212 | rejestracja trasy 209 w `main_centrala.cpp` (v3.55.9 → v3.56.0) | następny |
| 213 | audyt całego programu (nie tylko historii) | później |
| 214 | pełny build ESP (PlatformIO) i kwalifikacja na urządzeniu | po 212, wymaga Twojej zgody i sprzętu |

## Czego 211 świadomie NIE robi

- Nie rejestruje `/history` — to 212. Ten etap nie dotyka żadnego handlera HTTP.
- Nie zmienia formatu wiadomości Telegram ani menu.
- Nie włącza rotacji ani czyszczenia historii (A: stara historia może zostać odrzucona, ale nowe
  pomiary muszą być dalej zapisywane i pobieralne — to zostaje bez zmian).
- Nie wgrywa nic i nie wysyła nic na Telegram.

## Co 211 daje 212

Gotowy, przetestowany wzorzec „weź potwierdzony prefiks pod blokadą pisarza i wydaj tylko tyle”.
Trasa 212 zrobi to samo przez 207/208/209, ale zamiast `historia193::Calls` dostanie `fs::File`
albo ten sam VFS — decyzja w 212, po przeczytaniu, jak 209 wpiąć w `AsyncWebServer` bez zmiany
rytmu 8s/60s.
