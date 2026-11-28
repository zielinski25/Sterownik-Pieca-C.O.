TESTY PACZKI 3.58.14 (na PC, bez plytki)
========================================
./uruchom_testy.sh               czesci A-D
CPP_PATH=/inny/main_centrala.cpp  - sprawdza wskazany plik
JSDOM_DIR=/katalog                - gdzie jest/ma byc zainstalowany pakiet jsdom (npm i jsdom@24)

A  test_zrodlo_zew.js <plik .cpp albo .html>
   Wycina z HTML (raw-string R"PIECHTML( w pliku centrali albo wprost Piec.html)
   funkcje applyIncomingData() oraz blok ZEWN_ZRODLA/zewnZrodloSekcjaHtml() i
   uruchamia je w jsdom z atrapami (S, real, receivedFields, seg(), live$,
   sendCommand() zwracajacy ACK bez zmiany statusu). 18 sprawdzen:
   status zewnZrodlo=1 -> t_zewn = DS18B20, oba surowe odczyty w t_zewn_ds/t_zewn_2;
   przyciski z odczytami; klikniecie Zew 2 -> "ustaw zewnZrodlo 2" i NIC lokalnie,
   dopoki status nie potwierdzi; status zewnZrodlo=2 -> kafelek DHT22, przycisk,
   magistrala; zmiana z aplikacji (sam status) -> strona podaza bez komendy;
   Zew 2 + t_zewn_2=null -> BRAK DANYCH; symulacja zewn ma pierwszenstwo;
   firmware bez pola -> jak dotad + "wybor niedostepny"; zewnZrodlo=3/1.5 i
   t_zewn_2=95 odrzucone. A2: Piec.html; A3: 3.58.13 nie ma sekcji (kod 2).
B  test_zrodlo_zew.cpp + wytnij_zrodlo_zew.py
   Wycina z pliku deklaracje zewnZrodlo, galaz ustawParametrPieca("zewnZrodlo"),
   odczyt NVS z setup() i 3 linie JSON; kompiluje g++ -Wall -Wextra -Werror z
   atrapami String/Preferences; 12 sprawdzen (1/2 OK + NVS, 0/3/-1 odrzucone bez
   zapisu, start bez klucza/z uszkodzonym -> 1, JSON "zewnZrodlo":2 w trzech
   miejscach, klucz <= 15 znakow).
C  FIRMWARE_VERSION, zmienna raz i przed ustawParametrPieca(), NVS 1 zapis/1
   odczyt, pole w 3 JSON-ach, komenda "ustaw" generyczna, JS (numericRules,
   integerKeys, komenda przycisku), node --check obu blokow <script>,
   3.58.13 + patch = ten plik (SHA).
D  testy 3.58.13 (SUWAK-PO-ACK 18, SYM-OD-RAZU 11) i 3.58.11 (DYM-SYM 21) na tym pliku.
