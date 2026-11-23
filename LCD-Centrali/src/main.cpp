// ============================================================================
// WYSWIETLACZ CO — panel dotykowy (ESP32 + 3,5" SPI TFT ILI9488 + XPT2046)
// ============================================================================
// Druga, ODDZIELNA plytka wobec centrali (projekt "Piec CO", main.cpp).
// Ta plytka NIE steruje niczym bezposrednio (brak serw, czujnikow, przekaznikow
// tutaj) — jest CIENKIM KLIENTEM HTTP centrali, po tej samej sieci WiFi:
//
//   GET  /api/status           -> co STATUS_INTERWAL_MS, dane live (temp., klapa,
//                                  syberka, pompa, alarmy) — patrz StanPieca nizej
//   GET  /api/ustawienia-get   -> wartosci parametrow: co 5 s przy otwartym
//                                  ekranie ustawien, co 60 s w tle, od razu po
//                                  wlasnym poleceniu (diag::RytmUstawien, v1.4.3)
//   POST /api/ustawienia-set   -> gdy user dotknie +/- na ekranie Ustawienia
//                                  (pole=<nazwa>&wartosc=<int>) — DOKLADNIE ten
//                                  sam endpoint co panel WWW w przegladarce
//   POST /api/servo-manual     -> gdy user przesunie suwak na ekranie Serwo
//                                  (os=klapa|syberek&procent=<0-100>)
//
// Wszystkie pola w /api/ustawienia-get sa CALKOWITE (byte/int po stronie
// centrali) — zero miejsc po przecinku, wiec ten kod tez traktuje je jako long.
//
// PINY TFT/DOTYKU: piny EKRANU skonfigurowane w platformio.ini (build_flags,
// TFT_eSPI czyta je jako makra -D...). PINY DOTYKU sa NATOMIAST zdefiniowane
// PONIZEJ w tym pliku (TOUCH_SCK/TOUCH_MOSI/TOUCH_MISO/TOUCH_CS) i obslugiwane
// WLASNYM, recznym sterownikiem SPI (bitbang) — NIE przez wbudowana obsluge
// dotyku TFT_eSPI.
//
// [FIX-DOTYK-OSOBNA-MAGISTRALA] — diagnostyka w osobnej sesji wykazala, ze
// na tej konkretnej plytce ILI9488 (klon 3,5" z AliExpress) wspoldzielenie
// linii SCLK/MOSI/MISO miedzy ekranem a dotykiem (typowe podejscie, ktorego
// uzywa domyslna konfiguracja TFT_eSPI z TOUCH_CS) NIE dzialalo — potwierdzone
// wieloma niezaleznymi testami (m.in. reczny bitbang na wspoldzielonych
// pinach zawsze zwracal zero, a analogiczna, potwierdzona dzialajaca
// konfiguracja z innego projektu na tym samym typie modulu uzywala w pelni
// oddzielnej magistrali SPI dla dotyku). Dodatkowo pierwotny GPIO33 uzyty
// pod T_DO okazal sie fizycznie wadliwy na tym konkretnym ESP32 — po
// przelutowaniu dziala poprawnie, wiec zostaje na GPIO33.
// ============================================================================

// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// [2026-11-24] v1.5.18 - KAFELEK ZEWN BEZ CZUJNIKA: "3.4°C" ZAMIAST "~3°C"
//   - User (24.11): "Czemu jest znaczek tyldy i pogoda zaokraglona" -> wybor
//     "3.4°C". Temperatura z prognozy (pogoda_temp z centrali 3.58.17, gdy
//     DS18B20 daje -1.0) ma teraz ten sam format co pomiar z czujnika:
//     "%.1f°C", bez tyldy i bez zaokraglania do calych stopni. Na ekranie nie
//     widac wiec, czy liczba jest z czujnika, czy z prognozy (swiadoma
//     decyzja usera). "-0.0" -> "0.0". Bez prognozy nadal "--".
//   - Zmiana tylko w dashboard_lvgl_poc.cpp (jedna galaz w odswiezaniu
//     kafelka ZEWN) + symulator (kafelki_szerokosc: "3.4°C", "-12.6°C" 96 px
//     czcionka 28 miesci sie, "0.0°C" bez minusa) + projektant HTML.
// [2026-11-23] v1.5.17 - KAFELEK ZEWN: UKLAD "E" Z PROJEKTANTA, NAPISY W 2 LINIACH
//   - Wybor z projektanta docs/symulator_kafelka_zewn.html (23.11, wieczor):
//     "ikony=kolor rozmiar=40 napis=rodzaj czas=pelny uklad=E kolor=bialy
//     brak_czujnika=prognoza" + "popraw napisy bo sie niektore nie mieszcza"
//     (zrzut: na niebieskim pasku "deszcz za 45 m..." wchodzilo pod ikone).
//   - Uklad E: napis o pogodzie (bialy, czcionka 14) W MIEJSCU tytulu "ZEWN",
//     temperatura czcionka 28 w zwyklym wierszu wartosci jak inne kafelki,
//     niebieski pasek z v1.5.14 ZNIKA. Obok ikony 40 px jest 63 px na napis,
//     wiec napis ma 2 linie: zjawisko / czas ("deszcz" / "za 45 min",
//     "śnieg" / "za 2 h", "pada" / "30 min", "silny" / "wiatr"); linia
//     szersza niz 63 px dostaje odstep liter -1 ("za 45 min" 69 -> 61 px).
//     "napis=rodzaj": bez opadu/zjawiska (slonce, chmury) napisu nie ma -
//     tytul "ZEWN" wraca, stan nieba pokazuje sama ikona.
//   - "brak_czujnika=prognoza": bez DS18B20 (-1.0) temperatura z prognozy
//     centrali 3.58.17 (nowe pole pogoda_temp, Open-Meteo temperature_2m)
//     jako "~3°C"; starsza centrala / null -> "--" jak dotad.
//   - Symulator: scenariusz kafelki_szerokosc na nowy uklad (2 linie, ~3°C),
//     sprawdz_pola_statusu zna pogoda_temp; projektant HTML ma domyslnie
//     wybrany uklad i prawdziwe 2-liniowe lamanie.
// [2026-11-23] v1.5.16 - WYKRESY: TLO WYKRESU JAK TLO EKRANU
//   - Zdjecie z 23.11 (dwa zakreslone dolne rogi): od kiedy wykres siega do
//     krawedzi ekranu (1.5.15), jasniejsza "karta" KOL_KARTA (0x10A3) na
//     calej szerokosci konczyla sie tuz nad czarnym pasem podpisow czasu i w
//     dolnych rogach - tam, gdzie wyswietlacz TN ma diody podswietlenia -
//     bylo widac inny odcien. Teraz tlo wykresu = tlo ekranu (KOL_TLO),
//     bez zaokraglenia rogow; tlo etykiet osi Y tez KOL_TLO. Pole wykresu
//     wyznaczaja linie siatki.
// [2026-11-23] v1.5.15 - WYKRESY: WYKRES NA CALY EKRAN
//   - Zdjecia z 23.11 ("marnowanie miejsca wkolo, wykres ma byc po calosci"):
//     osobny wiersz tytulu "[Dzienny] 6 serii" (36 px) i staly dwuwierszowy
//     pas legendy (46 px) zabieraly 1/4 ekranu, wnetrze wykresu mialo jeszcze
//     ~10 px paddingu z kazdej strony z motywu.
//   - Teraz jeden pasek u gory: strzalka wstecz, legenda ("[Dzienny]" + kropki
//     z nazwami serii, czcionka 14) i mniejszy krzyzyk (26 px). Legenda ma
//     wysokosc po zawartosci - 1 wiersz do 4-5 serii, wiecej tylko gdy trzeba.
//     Wykres zaczyna sie 4 px pod legenda i konczy na y=299, podpisy osi
//     czasu 302..319, padding gora/dol 4 px.
//   - Wykres ZAWSZE na cala szerokosc ekranu (0..480; drugie zdjecie z
//     zaznaczonymi pustymi pasami po bokach). Etykiety osi Y leza NA polu
//     wykresu przy krawedziach (lewa os od x=10, prawa do x=470), na
//     polprzezroczystym tle w kolorze wykresu (linia pod liczba jest
//     przygaszona, liczba czytelna); skrajne etykiety trzymane wewnatrz
//     pola. v1.5.12 wystawiala je obok wykresu, co kosztowalo 50-70 px z
//     kazdej strony. Pole danych od marginesu obudowy (10 px) do 470 px,
//     zeby najnowszy pomiar (prawa krawedz) nie chowal sie pod ramka.
//     Pole danych: z ok. 346x192 px na ok. 460x263 px (x1,8).
//   - Symulator: dotkniecie wykresu i sprawdzenia tytulu dostosowane
//     (legenda zamiast tytulu); wszystkie scenariusze wykresy_* przechodza.
// [2026-11-23] v1.5.14 - ZEWN: POGODA (NIE TYLKO DESZCZ) - UKLAD WYBRANY W PROJEKTANCIE
//   - Kafelek ZEWN wg wyboru uzytkownika w docs/symulator_kafelka_zewn.html
//     ("ikony=kolor rozmiar=40 napis=zawsze czas=pelny uklad=F kolor=nieb
//     brak_czujnika=kreski"): temperatura ZAWSZE czcionka 28, pod nia
//     niebieski PASEK (18 px, tlo #153a5e, napis #8fc7ff czcionka 14) z
//     pogoda: "deszcz za 45 min", "śnieg za 2 h", "burza za 1,5 h", "pada
//     30 min", "sypie", "mży", a bez opadu "mgła", "silny wiatr" (porywy
//     >= 55 km/h) albo stan nieba "słonecznie" / "pogodnie" (noc) /
//     "przejaśnienia" / "pochmurno". Pasek znika tylko, gdy centrala nie
//     ma prognozy. Przy napisie temperatura stoi 12 px wyzej (y=40).
//   - 13 kolorowych ikon pogody 40x40 (src/ikony_pogoda.h, generator
//     testy_host/prototypy/ikony_pogody.py): slonce, ksiezyc, slonce za
//     chmura, chmura, deszcz, ulewa, mzawka, snieg, platek (sypie), burza,
//     mgla, wiatr, grad. Dwukolorowe ikony to 2 warstwy 1-bit (druga
//     lv_img nad pierwsza, kazda ze swoim recolor) - LVGL ALPHA_1BIT ma
//     jeden kolor na obrazek. Bez prognozy stare ikony wg temperatury.
//   - Nowe pola /api/status centrali 3.58.16: pogoda_kod (WMO teraz),
//     pogoda_kod_opad (WMO w slocie poczatku opadu z deszcz_stan),
//     pogoda_dzien (1/0), wiatr_porywy (km/h); -1 = brak. Starsza centrala
//     bez tych pol -> kafelek jak w v1.5.13 bez pasku (sam deszcz_stan
//     wciaz daje "za 2 h"/"pada" na pasku - pola sa niezalezne).
//   - Czujnik zewn. -1.0 (brak DS18B20) -> "--" zamiast "-1.0°C".
//   - Symulator: scenariusze zewn_pogoda_* (snieg za 2 h, burza, mgla, noc,
//     pada, wiatr, brak czujnika); sprawdz_pola_statusu zna 4 nowe pola.
// [2026-11-23] v1.5.13 - ZEWN: TEMPERATURA NIE ZNIKA PRZY DESZCZU
//   - Kafelek ZEWN przy nadciagajacym/padajacym deszczu pokazywal "za 6 h" /
//     "jeszcze 30 min" / "pada" ZAMIAST temperatury (zdjecie z 23.11:
//     "niepotrzebnie ona znika"). Teraz temperatura zostaje (czcionka 20,
//     tuz pod ikona), a napis o deszczu to drugi, niebieski wiersz czcionka
//     14 pod nia: "za 2 h", "pada 30 min" (= pada, koniec za ok. 30 min;
//     "jeszcze 30 min" nie miesci sie w 114 px), "pada"
//     (zewn_deszcz_odswiez() w dashboard_lvgl_poc.cpp). Bez
//     deszczu kafelek wyglada dokladnie jak dotad (czcionka 28).
//   - Test hosta: kafelki_szerokosc sprawdza oba wiersze (tresc, czcionki,
//     ze napis o deszczu jest pod temperatura i nad dolem karty, i ze znika
//     po deszczu). RAM: +1 etykieta LVGL (~100 B).
// [2026-11-26] v1.5.12 - WYKRESY: PRACA POMPY I CZYTELNIEJSZY EKRAN
//   - Nowa seria POMPA (kolumna 35 odpowiedzi /api/history/panel, kolumna
//     8 starego CSV): nie linia, lecz 12-pikselowy PAS u dolu pola wykresu
//     (niebieski odcinek = pompa pracowala w tym przedziale). Nie zajmuje
//     zadnej osi Y, nie wlicza sie do limitu osi, liczy sie do 8 serii.
//   - Etykiety osi Y stoja OBOK pola wykresu (lewa os: liczby dosuniete do
//     wykresu, prawa: od wykresu), nie na nim - plaska linia juz nie
//     przecina napisu. Pole wykresu zweza sie o szerokosc etykiet; bez
//     prawej osi siega do prawej krawedzi jak dotad. Minimalny margines osi
//     0,5 -> 1,0 jednostki (stala seria: rowne podzialki 44.0 .. 46.0).
//   - Wybor serii: nazwy jak kafelki pulpitu (PIEC CO, PIEC CO SR., POKOJ,
//     SYBERKA), 12 chipow 109x46 w siatce 460x154 (WILGOTNOSC w jednym
//     wierszu), przyciski trybu 140x34, nowy WYCZYSC obok POKAZ WYKRES (N),
//     pod nimi podpowiedz "Wybrano N z 8 - lewa os C - prawa os %".
//   - Naprawa: lv_obj_set_x() na wykresie wyrownanym TOP_MID bylo
//     przesunieciem od srodka, nie polozeniem (wykres z lewa osia wystawal
//     36 px za ekran). Teraz set_pos.
//   - Test hosta: wykresy_pompa_i_osie (5 zrzutow). RAM: +1 lv_chart (96 pkt
//     x 2 B + obiekt, ok. 0,4 kB), flash ok. +1,5 kB.
// [2026-11-25] v1.5.11 - IKONY BOJLER I PANEL JAK W APLIKACJI ANDROID
//   - BOJLER: szary kontur (IKONA_BOJLER_KONTUR), w srodku woda rysowana
//     prostokatem + bitmapa fali (IKONA_BOJLER_WODA przycieta do okienka
//     18x27 px). Poziom = (t-15)/55 obciete do 5..95 % jak w Ilu.kt
//     aplikacji; kolor: <=35 C niebieski, <=55 C bursztyn, >55 C pomarancz.
//     Bez danych z centrali ("--") woda znika. Zero nowych bitmap - pelna
//     ikona z v1.4.x zostaje tylko jako zrodlo fali.
//   - PANEL: ikona z czterech warstw 40x40 (stojak, rury, zbiornik, slonce)
//     - kolektor sloneczny jak w aplikacji; alarm przegrzania barwi rury
//     i zbiornik na czerwono (KOL_ERR), slonce i stojak bez zmian. Stare
//     IKONA_PANEL_SLONCE/KOLEKTOR (2 tablice) -> 4 tablice: +400 B flash.
//   - Nowy test hosta bojler_poziom_ikony (poziom/kolor dla 20/45/70/200/-1 C,
//     ukrycie przy "--", alarm panelu); dostep testowy
//     lvgl_dashboard_poc_bojler_woda(). Centrala bez zmian.
// [2026-11-24] v1.5.10 - GALKI SUWAKOW JASNOSCI W CALOSCI WIDOCZNE; CZYTELNE ZIELONE PRZYCISKI
//   - Zdjecie z plytki na v1.5.9 (ekran WYSWIETLACZ): galka suwaka jasnosci
//     (promien 12) wystaje 4 px pod 16-px pasek, a pasek stal na samym dole
//     wiersza - dolne 4 px galki przycinal wiersz ("kolka przyciete").
//     Pasek 4 px wyzej (y 30..46 w wierszu 0..50), galka 26..50 w calosci.
//     Test sen_ustawienia mierzy teraz galke tez w pionie.
//   - Zielone przyciski "wlaczone" (WL. harmonogramu, "Przyciemnij po: 30 s",
//     "Uspij po: 2 min", Antystop WL., taktowanie/moc na ZASILANIE): jasna
//     mieta KOL_BEZPIECZNA z szarym napisem miala kontrast ok. 1,5:1 - na LCD
//     nieczytelne. Nowe KOL_WLACZONE (ciemna zielen) + bialy napis: ok. 6,5:1.
//     KOL_BEZPIECZNA zostaje dla ikony BEZPIECZNA na SERWA, paska Antystop i
//     wskaznika ladowania.
//   - Centrala: bez zmian.
// [2026-11-24] v1.5.9 - MARGINES OD OBUDOWY NA WYKRESACH; LUKI SERW I SUWAKI JASNOSCI BEZ PRZYCIECIA
//   - Zdjecia z plytki na v1.5.8 (22.11, 18:03): wydrukowana obudowa zaslania
//     skrajne piksele ekranu, wiec ostatni podpis osi czasu ("18:00") przy
//     prawej krawedzi byl uciety do "18:0", a lewa etykieta osi Y "198 °C"
//     widoczna jako "'98 °C". Podpisy osi X trzymaja teraz 10 px od obu
//     krawedzi ekranu (WYKRESY_MARGINES_OBUDOWY), etykiety osi Y odsuniete
//     od krawedzi o 6 px (bylo 2). Srodek podpisu dalej pod kreska - jak
//     w v1.5.8 (na zdjeciu z ukosu wyglada na przesuniety, pomiar na
//     zrzutach z hosta: srodek podpisu = x kreski +-1 px).
//   - Ekran SERWA: luk pozycji ("kolko" obok 100%) byl ustawiony 2 px nad
//     gornym brzegiem wiersza, a wiersz przycina dzieci - kolko mialo sciety
//     wierzcholek. Teraz luk 0..28 px wewnatrz wiersza, etykieta procentow
//     wysrodkowana do niego.
//   - Ekran WYSWIETLACZ: suwaki jasnosci jak suwaki serw w v1.5.8 - przy 100%
//     galka konczyla sie 12 px za koncem paska. Pasek 16..340 px wiersza,
//     tor galki zawezony z prawej (pad_right 12): przy 100% galka rowno z
//     koncem paska. Etykieta "100%" nie wystaje juz 2 px nad wiersz.
//   - Centrala: bez zmian (3.58.10). Testy hosta: serwo_stopnie i
//     sen_ustawienia mierza geometrie (galka, luk w granicach wiersza).
// [2026-11-24] v1.5.8 - OS CZASU WYKRESOW PODPISANA PRZY KAZDEJ KRESCE; SAMOTNE PUNKTY WIDAC; SUWAKI SERW
//   - Zdjecia z plytki na v1.5.7 (22.11): pod wykresem jeden podpis na
//     srodku ("16:25 - 17:25", "22.11 14:01 - 22.11 17:25", "19.11 08:55 -
//     22.11 16:51"), a 3 pionowe kreski siatki co 1/4 szerokosci bez zadnej
//     wartosci. Teraz kreski stoja w ROWNYCH godzinach i kazda ma podpis
//     w pasie pod wykresem: godzina -> co 15 min ("16:30 ... 17:15"), doba
//     -> co 4 h (o polnocy data "23.11" zamiast "00:00"), tydzien -> kazda
//     polnoc z data ("19.11 ... 25.11"). Krok dobierany do dlugosci okna
//     (najmniejszy, ktory daje <= 7 podpisow), wiec krotsza historia (np.
//     3,5 h po restarcie centrali) dostaje co 30 min. Pierwszy podpis ma
//     date, gdy centrala ja podaje (dzien/tydz). Panel liczy to z naglowka
//     #panel-hist (od=, do=, od_etyk=) - zadnej zmiany w centrali 3.58.10;
//     stara centrala / brak zegara: dawny podpis "od - do" / "czas
//     nieznany" zostaje. Dol wykresu 310 -> 296 px, zeby pas podpisow
//     (303..320) nie wchodzil w dolne etykiety osi Y.
//   - Wykres tygodniowy na zdjeciu: 9 punktow + 87 przerw = same "kropki" i
//     pionowa kreska przy prawej krawedzi. To prawdziwe dane (dlugi bufor
//     centrali ma wpisy tylko z chwil, gdy pracowala; kreska = dwa sasiednie
//     punkty, 50 min obok siebie, o bardzo roznych wartosciach), ale LVGL
//     nie rysuje punktu bez sasiada - pojedyncze pomiary miedzy przerwami
//     byly niewidoczne. Gdy ktoras seria ma samotny punkt, panel wlacza
//     znaczniki punktow (4 px); przy ciaglych danych nadal sama linia.
//   - Ekran STEROWANIE SERWAMI: przy 100 % galka suwaka wystawala 12 px za
//     koniec paska, pod luk - wygladala na obcieta. Pasek 16..344 z torem
//     galki zawezonym z prawej (pad_right 12): przy 100 % galka konczy sie
//     rowno z paskiem, 6 px przed lukiem. "113%" ze starszego zdjecia to
//     stopnie sprzed v1.5.6 - od v1.5.6 etykieta nie przekracza 100 %.
//   - Testy: podpisy osi czasu w wykresy_centrala_liczy (doba co 4 h, godzina
//     co 15 min), wykresy_przerwy (tydzien: 7 dat; tydzien jak na zdjeciu:
//     9 punktow/87 przerw -> znaczniki wlaczone), wykresy_po_restarcie_
//     centrali; serwo_stopnie otwiera ekran SERWA przy 100 % (zrzut).
// [2026-11-24] v1.5.7 - WYKRESY 4x MNIEJ RAM-U; PANEL NIE ZRYWA WIFI, GDY CENTRALA PRZYSYLA WYKRES
//   - Log z plytki na v1.5.6 (22.11 16:44-16:47): po starcie ram=34752, przy
//     otwartych Wykresach ram=7120, ram_min=1932, alok_nieudane=42,
//     malo_pamieci. Model pamieci (pomiar_pamieci.sh --przypadki) dawal to
//     samo: ekran Wykresy +18,6 kB, szczyt przy wykresie +22,7 kB, na plytce
//     zostawalo ok. 3,8 kB.
//   - Punkt wykresu (WykresyPunkt, dashboard_lvgl_poc.cpp) mial 24 B:
//     etykieta[12] + wartosc + minimum + maksimum. Minimum i maksimum nikt
//     nie rysowal (jedna linia na serie), a etykieta byla ta sama dla
//     wszystkich 8 serii i czytana tylko dla pierwszego i ostatniego punktu
//     (podpis zakresu przy centrali < 3.58.4). Teraz punkt to 4 B (sama
//     wartosc, NaN = przerwa) + jedna wspolna tablica etykiet 96 x 12 B
//     (g_wykresyEtykiety). Bufory ekranu Wykresy: 18 432 B -> 4 224 B.
//     Model: ekran Wykresy +4,4 kB (bylo +18,6), szczyt +8,6 kB (bylo
//     +22,7); prog bez odmow pamieci 8 000 B wolnego RAM-u (bylo 22 250);
//     na plytce w najgorszej chwili ok. 18 kB (bylo 3,8 kB). Wyglad wykresow
//     bez zmian (te same punkty w testach wzorcowych).
//   - W tym samym logu: w srodku pobierania wykresu (8 z 16 linii odebranych)
//     panel wymusil ponowne polaczenie WiFi (wymuszam_polaczenie), bo
//     /api/status nie odpowiadal od 12 s - centrala byla zajeta wysylaniem
//     historii. Pobieranie padlo ("urwana ... errno=113"), WiFi zerwane na
//     kilka sekund, kolejne proby wykresu "brak_wifi". Teraz kazdy nowy bajt
//     historii i kazda odpowiedz na polecenie zeruje licznik milczenia
//     (diag::Ratunek::centralaOdezwalaSie()): dopoki centrala cokolwiek
//     przysyla, panel nie zrywa z nia polaczenia. Status nadal stary ->
//     kafelki "--" i dane_nieaktualne jak dotad; drabina ratunkowa rusza
//     12 s po OSTATNIM znaku zycia centrali, restart po 45 s bez statusu
//     bez zmian.
//   - Testy: wykresy_centrala_zajeta (status milczy, historia plynie 20 s:
//     bez wymuszam_polaczenie, wykres jest; po koncu historii wymuszenie po
//     12 s; status wraca), przypadek 2d w testRatunek(). Scenariusz
//     wykresy_pamiec liczy bufory po 384 B.
// [2026-11-24] v1.5.6 - IKONA WIFI NIE MRUGA PRZY CISZY CENTRALI; SERWA 0-100 %
//   - Zgloszenie z plytki na v1.5.5: "szukanie wifi mruga". Log: WiFi
//     polaczone caly czas (wifi=1, rssi -68..-81), ale /api/status co jakis
//     czas nie odpowiada w limicie -> po 10 s dane_nieaktualne, po chwili
//     dane_wrocily. Kazde dane_nieaktualne zamienialo ikone WiFi w naglowku
//     na X ("brak WiFi"), dane_wrocily z powrotem na kreski = mruganie.
//   - wifi_ikona_odswiez(): polaczono=false to "brak danych z centrali", nie
//     "brak WiFi". Gdy radio jest polaczone (pomiar co 2 s, ten sam co dla
//     ekranu ZASILANIE), a centrala milczy: kreski wg RSSI panelu na
//     POMARANCZOWO, bez X. X tylko gdy WiFi.status() != WL_CONNECTED.
//     Kafelki "--" i naglowek CENTRALA MILCZY bez zmian.
//   - Limity statusu (SiecCfg) 400/1500 -> 1000/3000 ms: w tym samym logu
//     POST z limitem 1000 ms przechodzil w 716 ms miedzy "timeout laczenia"
//     statusow z limitem 400 ms - centrala odpowiadala, tylko wolniej.
//     Mniej falszywych cisz; prog dane_nieaktualne (10 s) bez zmian.
//   - Zdjecie ekranu SERWA: "KLAPA 113%". /api/status podaje pozycje serw w
//     STOPNIACH (klapa 0..180, syberek 0..90 - main_centrala.cpp "Wspolny
//     kontrakt jednostek"), panel pokazywal je jako procenty. Teraz
//     diag::serwoStopnieNaProcent(): 113 st. klapy = 63 %, 86 st. syberka =
//     96 %, nic powyzej 100 %. Suwaki na ekranie SERWA startuja od tej
//     samej, przeliczonej wartosci; POST /api/servo-manual dalej w
//     procentach (centrala sama przelicza na stopnie). Log tag=PANEL dalej
//     podaje surowe stopnie. Opis trybu BEZPIECZNY: "syberek 100% (otwarty)".
//   - Wykresy: "klikam, pisze 'Centrala nie przyjela polaczenia', dopiero za
//     ktoryms razem sa wykresy" - te same czkawki AP. Zapytanie o historie,
//     ktorego centrala NIE PRZYJELA (limit laczenia, connect() odrzucony,
//     gniazdo zamkniete bez bajtu odpowiedzi), idzie samo jeszcze raz po
//     500 ms, do 3 prob (WYKRESY_PROB_MAKS, wykresyMoznaPonowic()); ekran w
//     tym czasie: "Ladowanie danych z centrali... / Centrala nie
//     odpowiedziala, proba 2 z 3". Nie ponawiamy: zapytanie przyjete bez
//     odpowiedzi przez 6 s, odpowiedz urwana, limit calosci, brak pamieci,
//     kod HTTP. Wyjscie z ekranu w odstepie przerywa. Log: tag=WYKRESY
//     zdarzenie=ponowienie proba=2/3 powod=brak_polaczenia etap=..;
//     zdarzenie=blad ma prob=N. Etap z (zamkniete przed naglowkami) to
//     teraz "nie przyjela polaczenia", nie "zla odpowiedz".
//   - Testy: ikona_wifi_cisza, serwo_stopnie, wykresy_ponowienie;
//     testSerwoStopnie() w test_diagnostyka.cpp.
// [2026-11-24] v1.5.5 - POLECENIA Z DOTYKU PONAWIANE (3 PROBY), NIE 1 x 400 MS
//   - Log z 24.11 15:40: trzy dotkniecia WL pompy i WYL skonczyly sie
//     "POLECENIE NIE DOSZLO DO CENTRALI", choc statusy co 2 s przechodzily
//     (status_blad=0) - padaly na pojedynczych czkawkach AP: POST mial 400 ms
//     na polaczenie i JEDNA probe. Na baterii (oszczedzanie radia) i przy
//     centrali probujacej STA (zaklocenia wlasnego AP, patrz main_centrala
//     2.12.1.x) to za malo.
//   - Kanal polecen (komendaFetch) ma wlasne limity: 1000 ms laczenie,
//     2000 ms calosc (NonBlockingHttp::ustawLimity(); statusy zostaja przy
//     400/1500 ms z SiecCfg - maja padac szybko, ida co 2 s).
//   - Polecenie bez odpowiedzi centrali wraca na poczatek kolejki i idzie
//     jeszcze raz po 700 ms, do 3 prob (diag::KolejkaPolecen::ponow(),
//     PROB_MAKS, ODSTEP_PONOWIENIA_MS). Pasek "NIE DOSZLO" dopiero po
//     ostatniej (najpozniej po ok. 7,4 s). Odrzucenie przez centrale
//     ({"ok":false}) nie jest ponawiane. Nowsza wartosc tego samego pola
//     uniewaznia ponowienie starej. Log: tag=KOMENDA zdarzenie=ponowienie
//     pole=... proba=2/3 powod=...; polecenie_nieudane ma teraz prob=N.
//   - Wszystkie polecenia panelu sa idempotentne (ustaw wartosc / stan), wiec
//     powtorzenie POST-a, ktory centrala przyjela, a odpowiedz zginela, nic
//     nie zmienia.
// [2026-11-24] v1.5.4 - WSKAZNIK "47% USB" + WGRYWANIE Z VSCODE PRZEZ WIFI
//   - Wskaznik z ogniwem i ladowarka: "47% USB" zamiast "USB 47%" (najpierw
//     procenty, potem zrodlo - uwaga uzytkownika po v1.5.3).
//   - Do v1.5.3 panel dalo sie zaktualizowac tylko kablem albo przez centrale
//     (/update_panel, plik w wydaniu na GitHubie). Teraz tak jak centrala:
//     ArduinoOTA na porcie 3232, haslo OTA_HASLO_PANEL (to samo co centrali),
//     nazwa "panelco". W platformio.ini srodowisko [env:esp32dev_wifi]
//     (upload_protocol = espota, upload_port = 192.168.4.50; haslo dopisuje
//     skrypt narzedzia_pio/ota_haslo.py z src/secrets.h - nie lezy w ini).
//   - Komputer musi byc w sieci centrali (PiecCO_AP), panel obudzony (na
//     baterii po 5 min bez dotyku spi - wtedy nie odpowie) i polaczony z WiFi.
//     ArduinoOTA.begin() dopiero po pierwszym polaczeniu (otaVscodeKrok()),
//     mDNS wylaczony (wgrywanie po IP, bez dodatkowego zadania i RAM-u).
//   - W trakcie wgrywania: ekran AKTUALIZACJA PANELU z paskiem (ten sam co
//     przy aktualizacji z centrali), watchdog karmiony w onStart/onProgress
//     (jak w centrali, [WDT-OTA-ESPOTA]). Po udanym wgraniu biblioteka sama
//     restartuje panel; nowy program czeka na zatwierdzenie jak kazdy OTA
//     (potwierdzPartycjeJesliCzeka() na koncu setup(), inaczej bootloader
//     wraca do poprzedniego). Log: tag=OTA-VSCODE zdarzenie=gotowe|start|
//     postep|koniec|blad.
//   - Jesli w tym samym czasie panel pobiera plik z centrali, Update.begin()
//     w ArduinoOTA sie nie uda (blad begin_failed) - nic sie nie psuje.
// [2026-11-24] v1.5.3 - WSKAZNIK ZASILANIA: "USB 47%" I OD RAZU
//   - Uzytkownik: "lcd jest na baterii mimo podlaczenia usb, nie pokazuje
//     napisu usb". Z ogniwem wskaznik nigdy nie pisal "USB" - ladowanie
//     bylo widac tylko po zielonym kolorze procentow. Teraz z ladowarka napis
//     to "USB 47%" (zielony), bez ladowarki "47%"; bez ogniwa dalej "USB".
//   - Stan GPIO35 trafia na wskaznik od razu po zmianie (senKrok(), co
//     sekunde; log tag=ZASILANIE zdarzenie=usb_podlaczone|usb_odlaczone),
//     a nie dopiero z pomiarem baterii co 30 s.
//   - Jesli mimo to po wpieciu ladowarki do TP4056 nie ma "USB": GPIO35 nie
//     jest sciagany do masy - sprawdzic przewod z padow CHRG/STDBY (oba!)
//     i czy ladowarka jest w TP4056, a nie w gniezdzie USB DevKita.
// [2026-11-24] v1.5.2 - EKRAN OBROCONY O 180 STOPNI + NAPISY W KAFELKACH
//   - EKRAN_OBROT 3 (bylo setRotation(1)): panel w obudowie z ogniwem jest
//     zamontowany zlaczem w lewo - obraz byl do gory nogami (zdjecie z 22.11).
//     Kalibracja dotyku z NVS zostaje: klucz dotyk/obrot mowi, w jakiej
//     orientacji ja robiono (brak klucza = 1, panele sprzed v1.5.2);
//     przy innym obrocie ekranu dotykOdczytaj() obraca piksel o 180 stopni
//     (diag::dotykObroc180). Nowa kalibracja zapisuje obrot=EKRAN_OBROT.
//     Log: tag=DOTYK zdarzenie=kalibracja_z_pamieci obrot_kalibracji=1
//     obrot_ekranu=3 odwrocony=1.
//   - KAFELKI (dashboard_lvgl_poc.cpp): "za 105m" (deszcz za 105 min) nie
//     miescil sie w kafelku ZEWN. ustawNapisDopasuj() dobiera czcionke
//     28/20/14 do szerokosci kazdej wartosci kafelka (a w ostatecznosci
//     lamie na dwie linie), mniejsza czcionka siedzi na dole wiersza
//     wartosci. Napisy o deszczu pelnymi slowami: "za 45 min", "za 2 h"
//     (od 100 min do pol godziny), "jeszcze 30 min", "jeszcze 1,5 h", "pada".
//     Scenariusz kafelki_szerokosc sprawdza najszersze wartosci kazdego
//     kafelka (-25.5°C, 1013 hPa, 100 %, 9999, Klapa 100%).
// [2026-11-24] v1.5.1 - SEN: TRZY UWAGI Z PLYTKI (uzytkownik, po wgraniu v1.5.0)
//   - PO WYBUDZENIU PRZYCISKIEM OD RAZU KAFELKI: ekran startowy ("WYSWIETLACZ
//     CO / Panel vX / Laczenie z WiFi / Centrala vY") tylko przy starcie
//     zasilania i po resecie. Po wybudzeniu z glebokiego snu (ext0/ext1)
//     pulpit LVGL jest rysowany PRZED laczeniem z WiFi (startPoSnie w
//     polaczWifi(): jeden lvgl_port_tick() zamiast napisow TFT_eSPI), dane
//     dochodza do kafelkow w tle; ekranStartowyWersje() tylko loguje.
//     Log: tag=SEN zdarzenie=pulpit_po_wybudzeniu ms=<od startu>.
//   - PIERWSZE DOTKNIECIE PRZYCIEMNIONEGO EKRANU TYLKO ROZJASNIA, nie otwiera
//     kafelka: filtr diag::dotykPrzepusc() w lvgl_touch_read() (lvgl_port.cpp)
//     pochlania caly dotyk, ktory zaczal sie przy przyciemnionym ekranie
//     (senEkranPrzyciemniony() nizej), i zeruje licznik bezczynnosci LVGL.
//   - CZASY DO WYBORU NA EKRANIE WYSWIETLACZ, wiersz "Na baterii": przyciemnij
//     po 30 s / 1 / 2 / 5 / 10 min / nigdy, uspij po 2 / 5 / 10 / 30 min /
//     1 godz. / nigdy (diag::SEN_*_OPCJE_MS). W NVS indeksy wyswietlacz/
//     senPrzyIdx i senSpijIdx (domyslnie 1 min / 5 min = jak v1.5.0);
//     wyswietlaczUstawieniaWyslij("senPrzyciemnij"/"senUspij", indeks).
//     Log przy zmianie: tag=SEN zdarzenie=ustawienie przyciemnij_ms=.. uspij_ms=..
// [2026-11-24] v1.5.0 - PRACA NA BATERII: PRZYCIEMNIENIE I GLEBOKI SEN (krok 3
//   planu z WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18; tematy B1/B5 i 14). Sprzet
//   panelu jest juz zlutowany i sprawdzony (2026-11-20: TP4056 + MT3608, dzielnik
//   100k/100k na GPIO34 - odczyt 3,32 V/1 % i 3,97 V/76 % zgadza sie z
//   multimetrem; GPIO35 z 10k do 3V3; brzeczyk przez C945 na GPIO13; przycisk
//   GPIO14 -> 1k -> GND). Kod snu z [ETAP 3b] (idzSpac(), nigdy nie wolany od
//   v1.4.4) jest wreszcie podpiety - pod LVGL, nie pod stary dashboard.
//   - NA BATERII (ogniwo na dzielniku, TP4056 bez USB): 1 min bez dotyku ->
//     podswietlenie 5 % (diag::SEN_PRZYCIEMNIJ_PO_MS); 5 min bez dotyku ->
//     gleboki sen (diag::SEN_USPIJ_PO_MS). Dotkniecie szyby albo przycisku
//     budzenia przywraca jasnosc i zeruje licznik. Decyzja: diag::senDecyzja()
//     (diagnostyka_panelu.h, sekcja [v1.5.0 SEN]), liczona co sekunde w senKrok().
//   - NA USB: nic sie nie zmienia wzgledem v1.4.7 (zadnego przyciemniania ani snu).
//   - NIE USYPIA i nie przyciemnia: w czasie alarmu (centrala zglasza alarm albo
//     brzeczyk gra), w czasie aktualizacji panelu (otaKrok()/nakladka).
//   - ROZLADOWANA BATERIA: ponizej 3,30 V (poza alarmem) - napis "Bateria
//     rozladowana / Podlacz ladowarke" na 2,5 s i sen od razu; ponizej 3,10 V
//     sen takze w alarmie (ogniwo i tak zaraz odetnie uklad zabezpieczenia).
//   - BUDZENIE: przycisk GPIO14 (ext0, stan niski) jak dotad ORAZ wlozenie
//     ladowarki (ext1 na GPIO35: TP4056 sciaga CHRG/STDBY do masy). Log przy
//     starcie: tag=ZASILANIE zdarzenie=wybudzenie zrodlo=przycisk|usb.
//   - PRZED SNEM dodatkowo: brzeczyk (GPIO13) na stale w stanie niskim przez
//     gpio_hold_en() - tak jak podswietlenie; inaczej tranzystor C945 moglby
//     dostac szum na bazie i modul by cicho pobieral prad. Po wybudzeniu
//     gpio_hold_dis() na obu pinach przed pierwszym pinMode()/ledcAttach().
//   - POMIAR BATERII: pierwszy odczyt od razu po starcie (do v1.4.7 dopiero po
//     30 s - wskaznik przez pol minuty pokazywal "USB"), potem co 30 s jak
//     dotad. Ostatni odczyt jest pamietany (bateriaOstatnieV) dla senKrok().
//   - LOG: tag=SEN zdarzenie=przyciemnienie|jasnosc|usypianie powod=... - tylko
//     przy ZMIANIE stanu, nie co sekunde.
//   Usunieto: ACTIVITY_TIMEOUT_MS, ostatniaAktywnosc, zarejestrujAktywnosc()
//   (licznik bezczynnosci prowadzi LVGL: lvgl_dashboard_poc_bez_dotyku_ms()).
//   Testy: testy_host (test_diagnostyka.cpp: senDecyzja(); symulator: scenariusze
//   sen_*). Centrala bez zmian.
// [2026-11-22] v1.4.7 - EKRAN ZASILANIE: TAKTOWANIE PROCESORA I MOC NADAJNIKA
//   WIFI (audyt 2026-11-18, tematy 11 i 18 / plan krok 6, B3 i B7). Nikt nie
//   zmierzyl jeszcze poboru pradu tego panelu ani nie znal RSSI przy centrali,
//   wiec zamiast stalych - przelaczniki + zywy odczyt, a pomiar testerem USB
//   robi uzytkownik. Ruch do centrali i reszta panelu bez zmian.
//   - Drugi pulpit: trzeci kafelek ZASILANIE (IKONA_BLYSKAWICA). Ekran:
//     przycisk "Taktowanie procesora" 240 -> 160 -> 80 MHz
//     (setCpuFrequencyMhz(); ponizej 80 MHz radio nie dziala) i "Moc
//     nadajnika WiFi" 19,5 -> 15 -> 11 -> 8,5 dBm (WiFi.setTxPower()); pod
//     nimi odczyt: RSSI PANELU, getCpuFrequencyMhz(), WiFi.getTxPower() - to,
//     co uklad naprawde robi; ostrzezenie przy sygnale ponizej -77 dBm.
//   - Zapis w NVS "wyswietlacz": "cpuIdx"/"txIdx" (indeksy w ZASILANIE_CPU_MHZ[]/
//     ZASILANIE_TX[]; nieznany indeks -> 0). Domyslnie 0/0 = DOKLADNIE jak do
//     v1.4.6 - nowa wersja niczego nie zmienia, dopoki ktos nie dotknie ekranu.
//   - Taktowanie stosowane na koncu setup() (kalibracja, laczenie i pierwsze
//     rysowanie zawsze na 240 MHz) i od razu po zmianie z ekranu. APB zostaje
//     80 MHz - SPI ekranu, PWM, UART, millis() bez zmian; wolniejsze tylko
//     liczenie (LVGL ~3x, JSON, OTA).
//   - Moc nadajnika dopiero PO WiFi.mode(WIFI_STA) w polaczWifi() (wczesniej
//     esp_wifi_set_max_tx_power() zwraca blad - temat 18) i cicho po kazdym
//     WiFi.begin() (wifiRozpocznijLaczenie()).
//   - Log: tag=ZASILANIE zdarzenie=cpu|nadajnik kiedy=start|ekran cel_... i
//     odczyt po zmianie, wynik=; linia tag=PANEL co 30 s ma cpu_mhz= tx_dbm=
//     tx_ok= - para linii przed/po przelaczeniu + odczyt z testera USB daje
//     porownanie poboru bez amperomierza w obwodzie.
//   - Testy (testy_host): scenariusze zasilanie, zasilanie_po_restarcie,
//     zasilanie_zly_indeks; atrapy setCpuFrequencyMhz()/getCpuFrequencyMhz()
//     (tylko 240/160/80), WiFi.mode()/setTxPower()/getTxPower() (blad przed
//     startem radia). 57 scenariuszy, 655 sprawdzen.
// [2026-11-19] v1.4.6 ERRATUM (tylko komentarze i testy, binarka v1.4.6 bez
//   zmian): zdanie "pole fw w /api/status istnieje od dawna" ponizej bylo
//   NIEPRAWDZIWE. Centrala do 3.58.7 wysylala "fw" tylko w JSON-ie do
//   Firebase (/piec/status) i w /api/lan/status - a panel czyta /api/status
//   (wifiZbudujJsonStatus()), gdzie tego pola nie bylo. Na plytce: "Centrala -"
//   mimo polaczenia. Testy na PC tego nie wykryly, bo atrapa centrali
//   podawala "fw". Naprawa: CENTRALA 3.58.8 (jedna linia w
//   wifiZbudujJsonStatus()); panel v1.4.6 pokazuje wersje bez zadnej zmiany.
//   Zeby to sie nie powtorzylo: testy_host/sprawdz_pola_statusu.py (krok [1b]
//   uruchom_testy.sh) porownuje liste pol czytanych przez panel z polami,
//   ktore NAPRAWDE wystawia wifiZbudujJsonStatus() w najnowszym pliku
//   centrali w repozytorium.
// [2026-11-21] v1.4.6 - WERSJE NA EKRANIE + PORZADKI PO LOGACH Z PLYTKI.
//   Zgloszenie uzytkownika: "przy starcie LCD nie pokazuje wersji - jest
//   tylko Szukam centrali / Laczenie z WiFi; chce wersje panelu i centrali".
//   Centrala: pole "fw" w /api/status wysyla dopiero 3.58.8 (patrz erratum
//   wyzej); ze starsza centrala panel pokazuje "Centrala -". Ruch do
//   centrali i logika panelu bez zmian.
//   - Ekran startowy (TFT_eSPI, przed LVGL): u gory "WYSWIETLACZ CO" i
//     "Panel v1.4.6" przez caly czas szukania centrali i laczenia z WiFi;
//     po pierwszej odpowiedzi centrali dochodzi "Centrala v3.58.6" (albo
//     "Centrala: brak odpowiedzi") i ekran stoi jeszcze EKRAN_START_POKAZ_MS
//     (1,5 s), zeby dalo sie to przeczytac - tylko przy zimnym starcie, po
//     wybudzeniu z deep sleep bez pauzy (ekranStartowyWersje()).
//   - Ekran WYSWIETLACZ (drugi pulpit): w lewym gornym rogu na stale
//     "Panel v1.4.6 / Centrala v3.58.6" (lvgl_dashboard_poc_ustaw_wersje();
//     "Centrala -" dopoki centrala nie odpowie albo gdy jest starsza i pola
//     "fw" nie ma). Dzieki temu po /update_panel widac na ekranie, co
//     faktycznie dziala, bez kabla.
//   - Nowe pole StanPieca::centralaFw (16 B) z pola "fw" statusu; linia logu
//     "tag=PANEL" ma teraz centrala_fw=... obok centrala=sterownik.
//   - Napis "WYŚWIETLACZ" na kafelku drugiego pulpitu nie jest juz obciety
//     (109 px napisu na 114 px karty): dashboard_lvgl_poc.cpp,
//     kafelek_launcher() scisniecie liter o 1 px, gdy napis sie nie miesci.
//   - GPIO35 (USB_DETECT_PIN) ustawiany jako INPUT, nie INPUT_PULLUP: to
//     pin tylko-wejsciowy bez wewnetrznego pull-upu; INPUT_PULLUP dawal w
//     logu "E gpio: gpio_pullup_en(85): GPIO number error (input-only pad has
//     no internal PU)" przy kazdym starcie i nic nie robil. Zewnetrzne 10 k
//     do 3V3 (patrz komentarz przy USB_DETECT_PIN) jest OBOWIAZKOWE.
//   - otaZapisWczytaj(): przestrzen NVS "ota_panel" otwierana do zapisu
//     (zakladana, gdy jej nie ma) zamiast tylko do odczytu - Preferences
//     logowalo "[E] nvs_open failed: NOT_FOUND" przy kazdym starcie panelu,
//     ktory nigdy jeszcze nie zaczynal aktualizacji. Jednorazowy zapis
//     kilkunastu bajtow w NVS, zachowanie bez zmian.
//   - testy_host: scenariusz wersje_na_ekranie (napisy na ekranie startowym,
//     etykieta na WYSWIETLACZ, centrala bez pola "fw", kafelek bez obciecia);
//     atrapa centrali podaje "fw" (jak centrala 3.58.8+).
// [2026-11-20] v1.4.5 - POLSKIE LITERY NA EKRANIE (punkt D / temat 23 z
//   WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18.md). Centrala bez zmian; ruch do
//   centrali, pamiec RAM i logika panelu bez zmian - zmienia sie tylko to,
//   co widac na wyswietlaczu LVGL.
//   - Nowe czcionki src/czcionki/lv_font_pl_14/20/28.c: Montserrat Medium
//     wygenerowany skryptem LVGL (built_in_font_gen.py, 4 bpp) z 18 polskimi
//     literami ąćęłńóśźż ĄĆĘŁŃÓŚŹŻ obok dotychczasowych 158 glifow (ASCII,
//     stopien, kropka, ikony LV_SYMBOL_*). Wbudowane montserrat_14/20/28
//     WYLACZONE w include/lv_conf.h (zero duplikatu); LV_FONT_DEFAULT =
//     lv_font_pl_14. Koszt: +1 271 / +2 139 / +3 671 B = +7 081 B flashu,
//     0 B RAM. Wysokosc linii 17/24/31 px (bylo 16/22/30 - ogonki Ą/Ę
//     schodza nizej), stad etykiety o 1-2 px wyzsze; uklad ekranow ten sam.
//   - Napisy na ekranach LVGL po polsku: POKÓJ, CIŚNIENIE, WILGOTNOŚĆ, ŚR,
//     WŁĄCZ/WYŁĄCZ, WŁ./WYŁ., RĘCZNY, PRÓG, POŻAR / DYM, WYŚWIETLACZ, Jasność
//     w dzień/nocy, POWRÓT, Wewnętrzna, POKAŻ WYKRES, napisy bledow ekranu
//     Wykresy, nakladka OTA ("Łączenie z centralą..."), pasek bledu polecenia
//     ("POLECENIE NIE DOSZŁO DO CENTRALI"), naglowek pulpitu ("BRAK PAMIĘCI",
//     "ZŁA ODPOWIEDŹ", "BŁĄD EKRANU"; diag::przyczynaNapis()). BEZ ZMIAN
//     (celowo): log Seriala i tokeny do centrali (ASCII), napisy rysowane
//     TFT_eSPI przed startem LVGL ("Szukam centrali", "Laczenie z WiFi",
//     kalibracja dotyku, "BRAK PAMIECI - RESTART") - czcionki TFT_eSPI nie
//     maja polskich liter, a to ekrany na kilka sekund.
//   - testy_host: scenariusz polskie_litery (czcionki maja wszystkie 18
//     liter; kazdy znak kazdej etykiety na kazdym ekranie ma glif w swojej
//     czcionce; napisy skladane w trakcie pracy tez; zadnego napisu
//     "odogonkowionego"); uruchom_testy.sh i pomiar_pamieci.sh kompiluja
//     src/czcionki/*.c.
// [2026-11-20] v1.4.4 - PORZADKI (krok 7 z WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18.md
//   = C1, C7, C8 z audytu). Firmware robi DOKLADNIE to samo co v1.4.3 - zero
//   zmian w zachowaniu, ekranie i ruchu do centrali; centrala bez zmian.
//   - C1: z tego pliku zniknely dwa od dawna niebudowane programy: stary
//     dashboard rysowany wprost na TFT (LVGL_POC_TEST=0: rysuj*(),
//     obsluzDotyk(), ekrany EKRAN_*, kafelki, panele, ikony wektorowe,
//     sprite kafelka, ~1 400 linii) i lokalna symulacja danych bez centrali
//     (LVGL_POC_SYMULACJA=1, ~350 linii) wraz z oboma makrami; martwe
//     ogony setup()/loop() za "return;" oraz touchReadChannelStabilnie(),
//     wyslijUstawienie(). W dashboard_lvgl_poc.cpp: martwy posredni ekran
//     "SERWO - WYBIERZ OS" (serwo_wybor_*). ZOSTAJE (celowo, na krok 3):
//     zarejestrujAktywnosc()/idzSpac() i pomiar baterii - patrz sekcja
//     ZASILANIE. 5 922 -> ok. 4 500 linii.
//   - C7: nieuzywane CENTRALA_HOST/CENTRALA_PORT/HTTP_TIMEOUT_MS (prawda
//     jest w SiecCfg), dwie identyczne galezie w aktualizujSezon() scalone,
//     literowki w komentarzach, zdanie o usypianiu w README OTA.
//   - C8: src/ kompiluje sie z -Wall -Wextra (platformio.ini build_src_flags
//     i testy_host/uruchom_testy.sh; do v1.4.3 testy kompilowaly firmware
//     z -w) - poprawione: %lu dla uint32_t, nieuzywane parametry callbackow.
//     Oczekiwany stan: zero ostrzezen.
// [2026-11-19] v1.4.3 - MNIEJ GADANIA Z CENTRALA (krok 5 z
//   WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18.md, rozdzial 4 = B4 i A2 z audytu).
//   Ekran pokazuje to samo co v1.4.2; centrala bez zmian.
//   - B4 (diag::RytmUstawien): /api/ustawienia-get co 5 s TYLKO gdy ktorys
//     ekran ustawien jest otwarty (serwo, pompa, dym, prog przegrzania;
//     lvgl_dashboard_poc_ekran_ustawien_otwarty()) i od razu przy jego
//     otwarciu; w tle co 60 s. Bylo: co 5 s zawsze = 12 z 42 polaczen na
//     minute po dane, ktorych nikt nie ogladal. Teraz w spoczynku 31/min.
//   - B4: tryb pompy ("wybor") i tryb serwa ("tryb_serwa") panel bierze
//     takze z /api/status (co 2 s) - pulpit widzi zmiane z telefonu/WWW
//     w 2 s, nie dopiero przy nastepnym odczycie ustawien. Ta sama ochrona
//     przed stara wartoscia co w v1.4.2 (kolejkaPolecen.poleZmieniane()).
//   - A2 (filtr ArduinoJson, diag::POLA_STATUSU_CZYTANE[]): z ok. 80 pol
//     /api/status rozpakowywane jest 46, ktore panel czyta. Prosb o pamiec
//     o polowe mniej (5 506 -> 2 760 na minute), szczyt przy odczycie
//     5,3 -> 2,7 kB, za to filtr trzyma ok. 2,4 kB sterty na stale (liczby:
//     README "Co zmienia v1.4.3"). Ustawienia bez filtra (przychodza raz na
//     minute). Test na PC pilnuje, ze kazde doc["..."] z
//     obslugaStatusFetch() jest na liscie filtra.
//   Testy: testy_host/test_diagnostyka.cpp (rytm, filtr) i scenariusz
//   "ustawienia_na_zadanie" w symulator_panelu.cpp; pomiar_obciazenia.sh.
// [2026-11-19] v1.4.2 - POLECENIA NIE GINA I KONIEC RESTARTOW BEZ CENTRALI
//   (krok 4 z WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18.md, rozdzial 4 = C4 z
//   audytu, oraz B6). Krok 3 (sen na baterii) celowo pozniej - czeka na
//   podlaczenie baterii i przycisku do plytki.
//   - C4 (kolejka polecen, diag::KolejkaPolecen w diagnostyka_panelu.h +
//     poleceniePlanuj()/poleceniaKrok() tutaj): do v1.4.1 kazdy most
//     (pompaManualWyslij, wyslijTrybPracy, wyslijServoManualny, ...) przy
//     zajetym kanale HTTP pisal "poprzednia komenda jeszcze w toku, pomijam"
//     i dotkniecie PRZEPADALO - przy szybkim klikaniu co drugie, przy
//     przytrzymanym "+" wiekszosc. Teraz polecenie czeka w kolejce i idzie,
//     gdy kanal sie zwolni; dla tego samego pola liczy sie ostatnia wartosc,
//     a pole zmieniane w kolejce czeka 200 ms ciszy (przytrzymanie = jeden
//     POST z koncowa wartoscia zamiast dwudziestu). Flaga "co odswiezyc po
//     poleceniu" (PoKomendzie) wedruje z poleceniem, nie przez globalna
//     zmienna; GET-y ida po OSTATNIM poleceniu serii.
//   - C4 (stara wartosc nie nadpisuje ekranu): odpowiedz GET
//     /api/ustawienia-get, ktora wystartowala przed koncem polecenia dla
//     pola, pomija to pole (kolejkaPolecen.poleZmieniane(); log
//     zdarzenie=stara_wartosc_pominieta). pobierzUstawienia() zawolane w
//     trakcie GET-a nie przepada (ustawieniaGetPonow) - do v1.4.1 ekran
//     zostawal wtedy ze stara wartoscia nawet 5 s.
//   - C4 (porazka widoczna): polecenie bez potwierdzenia centrali = czerwony
//     pasek na dole ekranu przez 3 s ("POLECENIE NIE DOSZLO DO CENTRALI" /
//     "ODRZUCONE: tryb serwa nie jest reczny") + log
//     zdarzenie=polecenie_nieudane. Do v1.4.1 tylko log.
//   - B6 (diag::progRestartuMs()): czwarty i kazdy nastepny samoczynny
//     restart bez danych dopiero po 6 h, nie co 5 min - przy centrali
//     wylaczonej na noc panel restartowal sie ~100 razy bez zadnego skutku.
//     W zamian przy milczacej centrali ponowne laczenie co 30 s
//     (Ratunek::PROG_PONOWIENIA_MS), a naglowek liczy czas czytelnie:
//     "BRAK WIFI 2 h 05 min" zamiast "BRAK WIFI 7384 s"
//     (diag::czasBrakuDanych()).
//   Testy: testy_host/test_diagnostyka.cpp (kolejka, Ratunek, naglowek) i
//   scenariusze "polecenia_nie_gina", "bez_centrali_dlugo",
//   "centrala_milczy_dlugo" w symulator_panelu.cpp.
// [2026-11-18] v1.4.1 - MALE I PEWNE POPRAWKI PO AUDYCIE (krok 1 z
//   WYNIKI_SZUKANIA_PANEL_LCD_2026-11-18.md, rozdzial 4; ustalenia z
//   AUDYT_PANELU_LCD_2026-11-18_WYCIEKI_DRENAZ_KOD.md). Zadna z nich nie
//   zmienia tego, co panel pokazuje ani jak rozmawia z centrala.
//   - C2: platformio.ini przypina platforme (ta sama co centrala, pioarduino
//     55.03.38-1, rdzen 3.3.8) i dokladne wersje bibliotek (TFT_eSPI 2.5.43,
//     ArduinoJson 7.4.3, LVGL 8.4.0). Testy na PC uzywaja tych samych.
//   - A5/temat 7: linia "tag=PANEL" ma trzy nowe pola: stos_min= (najmniejszy
//     zapas stosu petli od startu, w bajtach), alok_nieudane= (ile razy od
//     startu system odmowil pamieci) i alok_nieudana_b= (rozmiar ostatniej
//     odmowionej prosby). Odmowy liczy funkcja zarejestrowana w systemie
//     (heap_caps_register_failed_alloc_callback) - sama tylko zapisuje liczby.
//   - temat 8: po starcie panel sprawdza, czy w partycji coredump lezy zrzut
//     po awarii. Jesli tak: jedna linia "tag=ZRZUT" (zadanie, adres bledu,
//     powod paniki, slad wywolan) i kasowanie zrzutu, zeby nastepna awaria
//     miala miejsce. Adresy ze sladu tlumaczy sie na funkcje poleceniem
//     xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/esp32dev/firmware.elf
//     <adresy> (plik .elf z TEGO SAMEGO budowania, ktore jest na plytce).
//   - C3: kalibracja dotyku (tylko na plytce bez zapisanej kalibracji) karmi
//     watchdoga w petlach czekania na palec; przedtem musiala sie zmiescic
//     w 15 s, inaczej panel restartowal sie w kolko (powod task_wdt).
//   - A4: odpowiedz centrali na GET/POST (siec_nieblokujaca_LCD.h) ma limit
//     SiecCfg::ODPOWIEDZ_MAX_B = 8 kB; dluzsza konczy zadanie bledem
//     "odpowiedz za dluga" (etap 'd'). Prawdziwy /api/status ma ok. 2 kB.
//   - A6: malloc() w press_flash_zainstaluj() (dashboard_lvgl_poc.cpp) ze
//     sprawdzeniem wyniku; bez pamieci przycisk nie ma efektu wcisniecia,
//     ale dziala.
//   - C5: odczyty POLA[] po nazwie ida przez polaWartosc("nazwa"), ktore przy
//     nieznanej nazwie zwraca 0 i wypisuje "tag=USTAWIENIA lvl=ERR
//     zdarzenie=nieznane_pole" zamiast czytac pamiec sprzed tabeli.
//   - B8: WiFi.persistent(false) przed WiFi.mode() - biblioteka nie zapisuje
//     juz nazwy sieci i hasla do NVS przy kazdym WiFi.begin(); panel ma je
//     w programie.
//   - C9: jasnosc dzienna i nocna maja dolna granice JASNOSC_MIN_PROCENT = 5 %
//     (suwaki, wartosc z NVS, zapis). Przedtem 0 % gasilo ekran na stale,
//     takze po restarcie. Rozjasnianie na czas alarmu - razem z usypianiem
//     (krok 3), bo to ten sam fragment kodu.
//   - Krok 2 (B2, tez v1.4.1, tylko dashboard_lvgl_poc.cpp): pulpit ustawia
//     napis, kolor, ikone, widocznosc i wyrownanie TYLKO wtedy, gdy nowa
//     wartosc rozni sie od tej w obiekcie (sekcja [v1.4.1 RYSOWANIE]). LVGL 8
//     przy kazdym ustawieniu oznacza obiekt do przerysowania, wiec pulpit z
//     niezmienionymi danymi byl przerysowywany w 61 % co 2 s. Po zmianie:
//     0,01 ekranu/min zamiast 18,30 (testy_host/pomiar_obciazenia.sh); to,
//     co zostalo, to animacje (wirnik pompy, ekran POMPA) i licznik sekund w
//     naglowku, gdy centrala milczy. Wyglad ekranu bez zmian.
//   Nie zmienione w tym kroku (celowo): usypianie na baterii (krok 3),
//   polecenia (krok 4), odpytywanie centrali (krok 5).
// [2026-11-12] v1.4.0 - AKTUALIZACJA PRZEZ CENTRALE (OTA): W TRAKCIE PRACY, Z
//   POSTEPEM NA EKRANIE, BEZ WGRYWANIA W KOLKO.
//   Decyzja uzytkownika: "Dzialaj" po raporcie
//   RAPORT_OTA_PANELU_PRZEZ_CENTRALE.md (w repozytorium).
//   Jak bylo (v1.1.0-v1.3.0, z czytania kodu i z uruchomienia na PC):
//   - Panel sprawdzal oferte centrali RAZ, na koncu setup(). Pracujacy panel
//     nie zaczynal aktualizacji, dopoki ktos go nie wylaczyl i nie wlaczyl.
//   - Pobieranie (biblioteka HTTPUpdate) nie wracalo do petli: pusty ekran,
//     bez odpytywania centrali, watchdog wypisany na ten czas. Zaczynalo sie
//     takze w trakcie alarmu.
//   - Panel nie pamietal, co juz wgral. Centrala do 3.58.5 zdejmuje oferte
//     tylko wtedy, gdy panel zglosi wersje identyczna z tagiem wydania
//     ("v1.3.0" kontra "3.57.9") - wiec po udanym wgraniu panel wgrywal ten
//     sam plik przy kazdym starcie.
//   Jak jest (kod: sekcja "[v1.4.0 OTA] AKTUALIZACJA PANELU PRZEZ CENTRALE"
//   nizej; decyzje: diagnostyka_panelu.h, sekcja [v1.4.0 OTA]):
//   - Panel zna identyfikator programu, z ktorego pracuje: poczatek sumy
//     SHA-256, ktora esptool dopisuje na koncu kazdego programu ESP32
//     (esp_partition_get_sha256()). Zglasza go centrali w kazdym zapytaniu
//     o status (panel_obraz=..., a przy pierwszym starcie po wgraniu
//     panel_obraz_proba=...). Centrala 3.58.6 podaje identyfikator pliku,
//     ktory ma dla panelu ("panel_ota_obraz") - rowne = nie ma czego wgrywac.
//   - Panel pamieta w NVS (przestrzen "otapanel"), co zrobil z ostatnia
//     oferta: ile razy zaczynal pobierac, czy wgral, czy nowy program
//     wystartowal. Jedna oferta = najwyzej 3 proby; plik raz wgrany nie jest
//     wgrywany ponownie - takze przy starszej centrali, ktora oferty nie
//     zdejmie nigdy.
//   - Aktualizacja zaczyna sie W TRAKCIE PRACY (otaKrok() w loop()), gdy:
//     nie ma alarmu, dane z centrali sa swieze, minelo 20 s od startu, nikt
//     nie dotykal ekranu od 15 s, nie trwa pobieranie wykresu ani wysylanie
//     polecenia, wolnego RAM-u jest co najmniej 30 kB (najwiekszy blok 12 kB),
//     a na samym ogniwie bateria ma co najmniej 30 %.
//   - Pobieranie nie zatrzymuje petli (StrumienHttpGet::startBinarnie() w
//     siec_nieblokujaca_LCD.h): do 4 kB na obrot, prosto do Update.write().
//     Ekran pokazuje "AKTUALIZACJA PANELU" z paskiem postepu, panel dalej
//     pyta centrale o status, watchdog pilnuje petli jak zawsze.
//   - Alarm albo wycofanie oferty w trakcie pobierania przerywa je od razu;
//     taka proba sie nie liczy. Cisza 15 s albo 5 minut calosci konczy probe.
//   - Przed uruchomieniem nowego programu: naglowek pliku (program ESP32, nie
//     ESP32-S3 centrali), rozmiar, identyfikator rowny temu z oferty, a na
//     koncu sprawdzenie calego obrazu przez ESP-IDF (Update.end()).
//   - Po restarcie: jesli nowy program nie doszedl do zatwierdzenia i
//     bootloader wrocil do poprzedniego, panel to rozpoznaje (dziala inny
//     program niz wgrany) i tej oferty wiecej nie probuje.
//   - Panel mowi centrali, czego nie zrobi i dlaczego
//     (panel_ota_odmowa=<nr>-<powod>) oraz dlaczego jeszcze nie zaczal
//     (panel_ota_czeka=<powod>). Starsza centrala te parametry pomija.
//   - Haslo OTA jest kodowane do adresu (%XX) i nie pojawia sie w logu
//     ([NET-DIAG] pokazuje "token=***").
//   Zgodnosc: z centrala do 3.58.5 panel aktualizuje sie tak samo (sam, w
//   trakcie pracy, raz na tag wydania), ale centrala nie dowie sie o wyniku i
//   zostanie z niezdjeta oferta (blokada /update do recznego zdjecia - patrz
//   README). Pelna obsluga: centrala 3.58.6
//   (firmware-patch/v3.58.6-panel-ota).
//   Usunieto: wykonajAktualizacjeOTAPanelu(), sprawdzIWykonajOtaPanelu(),
//   OTA_TIMEOUT_MS, #include <HTTPClient.h> i <HTTPUpdate.h> (panel nie
//   uzywa juz zadnej z tych bibliotek).
//   Pamiec: w spoczynku bez zmian (stan OTA to ok. 250 B zmiennych
//   globalnych); w trakcie pobierania Update trzyma bufor 4 kB.
//   Sprawdzone na PC (testy_host/): decyzje - test_diagnostyka.cpp; caly
//   przebieg z prawdziwym kodem decyzji centrali 3.58.6 - scenariusze ota_*.
//   Typy i nazwy sprawdzone naglowkami arduino-esp32 3.0.7 i 3.3.8. NIE
//   kompilowane w PlatformIO, NIE uruchamiane na plytce: sam zapis do flash,
//   bootloader i wycofanie nieudanego programu sprawdzi dopiero plytka.
// [2026-11-12] v1.3.0 - WYKRESY: PUNKTY LICZY CENTRALA, POBIERANIE NIE
//   ZATRZYMUJE PANELU.
//   Prosba uzytkownika: "jak wybiore wykresy, to centrala ma pobrac dane z
//   Firebase albo lokalnie z pliku centrali i wyslac do panelu LCD".
//   Jak bylo (v1.2.2, z czytania jej kodu): po "POKAZ WYKRES" panel sam
//   pobieral od centrali CALY CSV historii (/api/history: do ok. 100 kB,
//   /api/history/long: do ok. 1,8 MB) i sam sciskal go do 96 punktow.
//   Pobieral blokujacym HTTPClient: do konca pobierania stal ekran, dotyk i
//   odpytywanie /api/status. Na odczyt mial 8 s - czego nie zdazyl pobrac,
//   tego na wykresie nie bylo, bez zadnej informacji. Kazde niepowodzenie
//   dawalo ten sam napis "Brak danych (WiFi/centrala niedostepna)" i zadnej
//   linii w logu.
//   Jak jest:
//   - Panel prosi centrale o GOTOWE punkty:
//       GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96
//     Centrala z firmware 3.58.4 (paczka firmware-patch/v3.58.4-panel-wykresy
//     w repozytorium) sama wybiera zakres, dzieli go na 96 rownych odcinkow
//     czasu i odsyla najwyzej 96 wierszy (18-20 kB). Bierze je ze swojej
//     lokalnej historii - tej, z ktorej rysuje strona WWW i ktora dopisuje do
//     plikow /hist_short.csv i /hist_long.csv na swoim flashu. Firebase nie
//     jest uzywany: centrala ma te dane u siebie, a Firebase wymagalby
//     internetu.
//   - Centrala starsza niz 3.58.4 odpowiada na ten adres przekierowaniem
//     (302). Panel pobiera wtedy caly CSV po staremu i rysuje z niego to samo
//     co v1.2.2 (porownane punkt po punkcie z punktami policzonymi kodem
//     v1.2.2: testy_host/wzorce_v122/). Kolejnosc wgrywania panelu i
//     centrali jest wiec dowolna.
//   - Pobieranie (oba sposoby) nie blokuje petli: nowa klasa StrumienHttpGet
//     w siec_nieblokujaca_LCD.h czyta odpowiedz linia po linii w kolejnych
//     obrotach loop(), ok. 4 kB na obrot, do stalego bufora. Ekran, dotyk,
//     ekran alarmu, odpytywanie /api/status i watchdog pracuja w tym czasie
//     normalnie. Limity: 1,5 s na polaczenie, 6 s ciszy, 20 s na calosc.
//   - Wyjscie z wykresu w trakcie pobierania (dotkniecie, X, bezczynnosc,
//     ekran alarmu) zamyka polaczenie.
//   - Pod "Ladowanie danych z centrali..." widac, ile juz doszlo (wiersze,
//     kB).
//   - Gdy wykresu nie ma, ekran mowi dlaczego: brak WiFi, centrala nie
//     przyjela polaczenia, nie odpowiedziala w czasie, blad HTTP, odpowiedz
//     urwana, odpowiedz nie jest historia, brak pamieci, centrala nie ma
//     jeszcze danych, jeden pomiar to za malo. Gdy centrala ma wylaczony
//     zapis historii na flash, napis o braku danych to dopowiada.
//   - Kazde pobranie zostawia w logu linie "tag=WYKRESY" z wynikiem (opis:
//     komentarz "[v1.3.0 WYKRESY] Dane do ekranu Wykresy" nizej).
//   - Wykres z punktow centrali 3.58.4: os czasu jest rowna (v1.2.2 dzielil
//     dane na punkty wg numeru wiersza, a wiersze historii tygodniowej nie
//     sa rowno odlegle w czasie); przerwa w pracy centrali to przerwa w
//     linii o szerokosci tej przerwy; czujnik bez odczytu to przerwa w linii
//     tylko tego czujnika; podpis pod wykresem to zakres podany przez
//     centrale, np. "12.11 13:01  -  13.11 13:00".
//   - Stary sposob, gdy caly CSV nie dojdzie w 20 s albo polaczenie sie
//     urwie: wykres z tego, co doszlo, z dopiskiem NIEPELNE.
//   - Odpowiedz, ktora nie jest historia (np. strona HTML): napis zamiast
//     wykresu. Kod v1.2.2 rysowal z takiej strony wykres - kazda linia tekstu
//     stawala sie punktem (testy_host/wzorce_v122/punkty.txt, wpis
//     dzien_html_zewn).
//   - Zakres osi jest liczony tylko z prawdziwych wartosci. Dotad przerwa
//     jako PIERWSZY punkt serii dawala zakres nieokreslony (porownania z NaN
//     sa zawsze falszywe - wykresy_chart_zakres_osi() w
//     dashboard_lvgl_poc.cpp). Z punktami centrali to zwykly przypadek:
//     czujnik bez odczytu na poczatku zakresu.
//   Usunieto: wykresyFetchWykonaj() i HISTORIA_TIMEOUT_MS. HTTPClient zostal
//   tylko dla aktualizacji OTA.
//   Pamiec (testy_host/pomiar_pamieci.sh, model 32-bitowy na PC): po starcie
//   firmware trzyma tyle samo co v1.2.2 (129 244 B). Zmiennych statycznych
//   przybylo 512 B (4 468 B zamiast 3 956 B: bufor jednej linii 300 B, adres
//   zapytania, stan pobierania). Kod pobierania nie prosi o pamiec ze sterty.
//   W czasie pobierania i rysowania wykresu sterta rosnie chwilowo o 2,9 kB
//   (punkty z centrali) albo 5,6 kB (caly CSV) - to rysowanie LVGL i zwykle
//   odpytanie /api/status, ktore wypada w tym samym czasie; mniej niz limit
//   jednego odpytania (7,5 kB). Piec wykresow z rzedu nie zostawia nic.
//   Pamieci, ktora na czas pobierania bierze stos sieciowy plytki (lwIP),
//   ten pomiar nie widzi - pokaze ja "ram_min=" w linii "zdarzenie=pobrano".
//   Sprawdzone: na PC (testy_host/: 262 sprawdzenia jednostkowe, 34
//   scenariusze / 377 sprawdzen, takze z AddressSanitizer/UBSan i w modelu
//   32-bitowym; 8 nowych scenariuszy wykresy_*; odpowiedzi centrali uzyte w
//   tych testach sa plikami wypisanymi przez kod centrali 3.58.4 -
//   testy_host/wzorce_centrali/) i skladniowo prawdziwymi naglowkami
//   arduino-esp32 3.0.7 / 3.3.8. NIE budowane przez PlatformIO i NIE
//   uruchamiane na plytce. Czasy pobierania z testow na PC nie sa pomiarem
//   plytki (tam siec jest lokalna).
//
// [2026-11-12] v1.2.2 - PRZYCZYNA: PANELOWI BRAKUJE PAMIECI RAM.
//   Uzytkownik doslal log PANELU (v1.2.0) z tego samego zdarzenia co log
//   centrali opisany przy v1.2.1. Co w nim jest:
//   - Po starcie prawie kazda odpowiedz centrali konczy sie linia
//       tag=STATUS lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=json
//       http=200 json=NoMemory bajtow=1811 cialo_bajtow=1683 ...
//     Odpowiedz przyszla cala (HTTP 200, poczatek i koniec poprawne), a
//     ArduinoJson nie dostal od malloc() pamieci na jej rozpakowanie. Tak
//     bylo przez ok. 18 minut; kafelki pokazywaly "--".
//   - Linie "tag=PANEL" podaja heap=47032 heap_min=39624 blok_max=38900 i
//     blok_max jest IDENTYCZNY we wszystkich liniach przez godzine. To nie
//     jest pamiec, z ktorej korzysta malloc(): ESP.getFreeHeap() i
//     getMaxAllocHeap() w arduino-esp32 3.x doliczaja niezajety kawalek
//     obszaru na kod w RAM-ie (IRAM), a z niego zwykly malloc() nie
//     przydziela (sprawdzone w zrodlach arduino-esp32 3.0.7/3.3.8 i ESP-IDF
//     5.1.4/5.5 - opis przy ramWolny()). Naprawde wolne bylo: ok. 8 kB w
//     spoczynku, ok. 2 kB w chwili bledu, 0,6-0,9 kB w najgorszej chwili.
//   - Wczesniejszy restart w tym logu to "task_wdt ... loopTask (CPU 1)":
//     petla glowna krecila sie w miejscu przez 15 s. Slad wywolan jest
//     gleboki i ma powtarzajace sie adresy, jak rysowanie drzewa obiektow
//     LVGL; LVGL 8 po nieudanym przydziale pamieci staje w "while(1);".
//     Adresow nie da sie tu zamienic na nazwy funkcji (brak pliku .elf z
//     tej kompilacji), wiec to NAJPRAWDOPODOBNIEJ ta sama przyczyna, nie
//     pewnik.
//   - Od ok. 19. minuty do konca logu (ponad 40 minut) kazda proba konczy sie
//     limitem czasu, przy wifi=1 i zmieniajacym sie rssi. Z tego logu NIE da
//     sie rozstrzygnac, czy to panel gubil ramki z braku pamieci, czy
//     centrala przestala mu odpowiadac. W obu przypadkach pomogl dopiero
//     reczny reset - tym zajmuje sie drabina z v1.2.1 (ponowne polaczenie po
//     12 s, restart po 45 s).
//   Skad brak pamieci (pomiar: testy_host/pomiar_pamieci.sh - ten firmware
//   jako program 32-bitowy z licznikiem malloc(), z narzutem sterty ESP32):
//     v1.1.0 i v1.2.0 trzymaly po starcie 171 kB: ekrany LVGL (wszystkie
//     budowane przy starcie, ok. 2400 blokow) 110 kB, bufor rysowania LVGL
//     38,4 kB, bufory punktow ekranu Wykresy 18,5 kB, bufory odpowiedzi
//     HTTP 2,5 kB, reszta 1,6 kB.
//     Plytka miala wedlug logu 234,6 kB przed budowa ekranow i ok. 8 kB po
//     starcie - na WiFi, lwIP i zadania systemu przypada wiec ok. 56 kB.
//     Jedno odpytanie o status potrzebowalo chwilowo ok. 7,4 kB (bez buforow
//     samego WiFi), czyli prawie wszystko, co bylo wolne.
//     v1.1.0 miala ten sam problem (trzymala o 250 B mniej), tylko o nim nie
//     pisala: nieudane rozpakowanie konczylo sie cichym `return` - stad
//     pierwsze zgloszenie "kafelki sie nie aktualizuja" przy samych liniach
//     "etap=sukces" w logu.
//   Zmieniono:
//   - Bufor rysowania LVGL: 16 wierszy zamiast 40 (15 360 B zamiast
//     38 400 B) - lvgl_port.cpp. Obraz jest identyczny (20 zrzutow ekranu
//     z testow na PC, piksel w piksel); szybkosci rysowania na plytce nie
//     dalo sie tu zmierzyc.
//   - Bufory punktow ekranu Wykresy (18 464 B) istnieja tylko wtedy, gdy ten
//     ekran jest otwarty - dashboard_lvgl_poc.cpp, wykresy_bufory_8_*().
//     Razem firmware trzyma po starcie 129 kB zamiast 171 kB. Zmiennych
//     statycznych przybylo ok. 0,9 kB (glownie bufor logBezPamieci()).
//     Szacunek dla plytki z logu: ok. 48 kB wolnego RAM-u zamiast ok. 8 kB.
//   - Odpowiedz centrali nie jest juz kopiowana przed rozpakowaniem
//     (NonBlockingHttp::odpowiedz() zamiast pobierzOdpowiedz()): jedno
//     odpytanie o status potrzebuje chwilowo 5,6 kB zamiast 7,4 kB.
//   - Brak pamieci na ODEBRANIE odpowiedzi jest wykrywany
//     (siec_nieblokujaca_LCD.h, String::concat() zwraca false). Dotad
//     String po cichu nie dopisywal kawalka i odpowiedz z dziura wygladala
//     jak blad centrali ("json=InvalidInput").
//   - Log podaje prawdziwy RAM: ram=, ram_min=, ram_blok= (zamiast heap=,
//     heap_min=, blok_max=) w liniach "tag=PANEL", "odpowiedz_nieczytelna" i
//     "autorestart"; nowe linie "tag=PAMIEC zdarzenie=po_ekranach|po_starcie"
//     (raz, przy starcie) i "tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci"
//     (ponizej 16 kB, najwyzej raz na minute). Te same liczby ida w
//     zgloszeniu restartu do centrali (pola h i b).
//   - Brak pamieci jest osobna przyczyna braku danych: na ekranie
//     "BRAK PAMIECI 14 s" (bylo "ZLA ODPOWIEDZ", czyli wina centrali), w logu
//     przyczyna=pam, w zgloszeniu do centrali auto_pam_..._em200. Dotyczy
//     json=NoMemory, braku pamieci na odebranie odpowiedzi oraz
//     socket()/connect()/send() odrzuconych z ENOMEM, ENOBUFS, ENFILE.
//     Restart po 45 s jak przy innych przyczynach, bez ograniczenia do
//     dwoch.
//   - Linie logu potrzebne wlasnie przy braku pamieci ("tag=PANEL fw=",
//     odpowiedz_nieczytelna, dane_nieaktualne, malo_pamieci, autorestart,
//     lvgl_assert) ida przez logBezPamieci(): Serial.printf() dla tekstu od
//     64 znakow sam prosi o pamiec i bez niej linia przepada.
//   - Blad wewnetrzny LVGL (w praktyce: brak pamieci w trakcie rysowania)
//     nie zawiesza juz panelu na 15 s: include/lv_conf.h kieruje go do
//     panel_lvgl_assert() - napis "BRAK PAMIECI - RESTART" na ekranie, wpis
//     "tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=...", restart po 2,5 s i
//     zgloszenie centrali auto_lvgl_...
//   Sprawdzone: na PC (testy_host/: 262 sprawdzenia jednostkowe, 26
//   scenariuszy / 252 sprawdzenia, takze z AddressSanitizer/UBSan; nowe
//   scenariusze brak_pamieci_json, brak_pamieci_odbior, brak_pamieci_lvgl,
//   wykresy_pamiec, w ktorych malloc() odmawia pamieci na zadanie; te same
//   scenariusze w modelu 32-bitowym) i skladniowo prawdziwymi naglowkami
//   arduino-esp32 3.0.7 / 3.3.8. NIE budowane przez PlatformIO i NIE
//   uruchamiane na plytce.
//   "48 kB" to szacunek z jednego logu - prawdziwa liczbe pokaze linia
//   "tag=PAMIEC zdarzenie=po_starcie ram=..." po wgraniu.
//
// [2026-11-12] v1.2.1 - po zgloszeniu "nadal jest problem: podczas pracy
//   zresetowalem LCD i dopiero wtedy odebral swieze dane" (wg uzytkownika na
//   plytce byla v1.2.0). Przyslany zostal tylko log CENTRALI; logu panelu z
//   tej chwili nie ma, wiec PRZYCZYNA NADAL NIE JEST POTWIERDZONA.
//   [dopisek z v1.2.2: przyczyna zostala potem znaleziona w logu panelu -
//   patrz wpis wyzej. Ponizszy opis zostaje, bo mechanizmy z v1.2.1 dalej
//   dzialaja; pola heap=/blok_max= wymienione nizej nazywaja sie od v1.2.2
//   ram=/ram_blok= i licza inna, mniejsza pamiec.]
//   Co wiadomo:
//   - centrala miala aktualne wartosci (piec=134.0C zewn=49.0C
//     alarmCO=AKTYWNY pompa=ON) i buduje /api/status od nowa przy kazdym
//     zapytaniu, z tych samych zmiennych co linia "tag=STATUS" jej logu
//     (main_centrala.cpp 3.58.3) - starych wartosci panelowi nie podaje;
//   - jej log zawiera wpis "tag=HEALTH" (pierwszy pojawia sie 5 min po
//     starcie) z telemetry_produced=4 - wyglada na to, ze centrala
//     wystartowala ok. 5 min wczesniej, czyli krotko przedtem byla
//     restartowana (wniosek z jednej linii, nie pewnik);
//   - log panelu z 2026-11-11 (starszy build): tuz przed takim samym resetem
//     kazda proba konczyla sie od razu "connect() natychmiastowy blad", a po
//     resecie panel mial dane po 1,6 s;
//   - pamiec: ten firmware uruchomiony na PC w modelu 32-bitowym (zig,
//     x86-linux-musl, licznik malloc/free - pomiar jednorazowy, poza
//     testy_host/) po setup() trzyma ok. 132 kB w ok. 2480 blokach (z tego
//     38 kB to bufor rysowania LVGL), cykl statusu bierze chwilowo ok. 6 kB,
//     a przez 4 h pracy z aktywnym alarmem przybylo 4 kB (jeden blok, raz).
//     Wyciek w kodzie panelu jest wiec malo prawdopodobny; zuzycia przez
//     WiFi/lwIP na plytce ten pomiar nie obejmuje (pokaze je heap= i
//     blok_max= w linii "tag=PANEL").
//   Przypuszczenie (NIE potwierdzone): panel traci lacznosc z centrala,
//   byc moze przy jej restarcie, i sam jej nie odzyskuje. Pewne jest tylko
//   to, ze restart panelu pomaga. Na PC nie da sie tego odtworzyc, bo
//   dotyczy radia.
//   Dodano ([v1.2.1 RATUNEK], opis przy panelRatunekKrok()):
//   - Naglowek dashboardu przy braku danych pokazuje, czego brakuje, i licznik
//     sekund: "BRAK WIFI 14 s" / "CENTRALA MILCZY 14 s" / "ZLA ODPOWIEDZ 14 s";
//     w ostatnich 10 s przed restartem "RESTART ZA 7 s: BRAK WIFI".
//   - Ten sam znacznik (takze "SYMULACJA: piec") stoi teraz na ekranie
//     alarmu, nad przyciskami. W logu centrali z 2026-11-12 alarm
//     przegrzania byl aktywny (piec=134.0C na plytce bez czujnikow); przy
//     takim statusie panel pokazuje pelnoekranowy alarm, ktory zaslania
//     kafelki i naglowek - nie bylo na nim widac, czy dane jeszcze
//     przychodza ani ze temperatura pochodzi z symulacji.
//   - WiFi zglasza polaczenie, a centrala od 12 s milczy: panel rozlacza sie
//     i laczy od nowa (po kolejnych 12 s jeszcze raz, bez zapamietanego
//     BSSID). v1.1.0 i v1.2.0 w tym stanie nie robily nic - reconnect ruszal
//     tylko wtedy, gdy WiFi samo zglosilo rozlaczenie.
//   - 45 s bez poprawnego statusu: panel restartuje sie sam. Kolejne takie
//     restarty z rzedu: po 90 s, 3 min, potem co 5 min; licznik wraca do zera
//     po 2 min nieprzerwanie poprawnych danych. Gdy centrala odpowiada, ale
//     nieczytelnie, restarty sa najwyzej dwa (wiecej nic nie da, a kazdy
//     konczylby sie wiadomoscia z centrali). Czas liczony jest z obrotow
//     petli (najwyzej 1 s na obrot), wiec dlugie pobieranie wykresow nie
//     konczy sie restartem.
//   - Po takim restarcie panel zglasza centrali powod zamiast "sw", np.
//     reset_powod=auto_wifi_r201_n1_t45_h61_b31_en118 (rozpisanie pol:
//     diag::tokenAutorestartu()). Centrala wpisuje go do SWOJEGO logu
//     ("tag=RESET-DIAG ... nietypowy_restart_panelu powod=auto_...") i wysyla
//     wiadomosc - tak jak o kazdym restarcie innym niz "sw"/"deepsleep".
//     Dzieki temu nastepne zgloszenie da sie rozstrzygnac z samego logu
//     centrali. Restart po OTA zostaje "sw".
//     Kazdy restart jest zglaszany raz: po pierwszej odpowiedzi centrali
//     panel wraca do zwyklego "sw", wiec centrala nie oglosi go ponownie po
//     wlasnym restarcie. Z jednej serii restartow (serie konczy godzina
//     poprawnych danych) zglaszane sa pierwsze trzy, potem 6., 12., 24.... -
//     lacznosc, ktora rwie sie raz za razem, nie daje wiadomosci co kilka
//     minut. Numer w serii jest w zgloszeniu ("n6").
//   - Stan alarmu (dym / przegrzanie pieca / panelu, WYCISZ, ZAMKNIJ) jedzie
//     przez taki restart w pamieci RTC: brzeczyk i ekran alarmu wracaja,
//     zanim przyjda dane - restart z braku danych nie gasi alarmu po cichu.
//   - Log: "tag=WIFI zdarzenie=rozlaczono kod=... nazwa=..." (kod przyczyny
//     kazdego rozlaczenia WiFi), "wymuszam_polaczenie", "tag=PANEL
//     zdarzenie=autorestart|start_po_autorestarcie|autorestart_zgloszony|
//     autorestart_pominiety|autorestart_licznik_wyzerowany|alarm_sprzed_restartu";
//     w liniach "GET nieudany" numer i nazwa bledu gniazda (errno); linia
//     "tag=PANEL" ma nowe pola bez_danych_s, przyczyna, wymuszen, rozl_kod,
//     rozl_n, autorestartow.
//   Naprawiono (brzeczyk, znalezione przy okazji; potwierdzone na PC dla
//   v1.2.0 - scenariusz wycisz_alarm):
//   - WYCISZ dzialal dopiero po nastepnym poprawnym statusie, czyli bez
//     danych z centrali wcale; teraz cisza zapada od razu.
//   - Drugie nacisniecie WYCISZ wlaczalo dzwiek z powrotem.
//   - Raz wyciszony typ alarmu zostawal niemy przy kazdym nastepnym
//     wystapieniu, az do restartu panelu; teraz wyciszenie konczy sie razem
//     z alarmem.
//   Sprawdzone tak jak v1.2.0: na PC (testy_host/: 241 sprawdzen
//   jednostkowych, 22 scenariusze / 216 sprawdzen, takze z
//   AddressSanitizer/UBSan) i
//   skladniowo prawdziwymi naglowkami arduino-esp32 3.0.7 / 3.3.8. NIE
//   budowane przez PlatformIO i NIE uruchamiane na plytce. Model WiFi w
//   testach jest zalozeniem - w szczegolnosci "martwe polaczenie" po szybkim
//   restarcie centrali (scenariusz martwe_polaczenie) to przypuszczenie, a
//   nie zaobserwowane zachowanie tych plytek.
// [2026-11-11] v1.2.0 - po zgloszeniu "kafelki sie nie aktualizuja" (log
//   serial-20261110_200814.log: same linie "[NET-DIAG] ... sukces", zero
//   bledow, a ekran stoi). PRZYCZYNA ZGLOSZENIA NIE JEST POTWIERDZONA - log
//   v1.1.0 nie zawieral nic, co pozwalaloby ja wskazac. Ta wersja usuwa
//   bledy, ktore daja taki objaw, i sprawia, ze nastepny log oraz sam ekran
//   beda rozstrzygajace. Sprawdzone na PC (testy_host/: prawdziwy main.cpp i
//   dashboard uruchomione zwyklym g++ z atrapami sprzetu) oraz skladniowo
//   prawdziwymi naglowkami arduino-esp32 3.0.7 / 3.3.8 + ESP-IDF 5.1 (clang,
//   testy_host/sprawdz_naglowki_esp32.sh). W tej sesji NIE budowane przez
//   PlatformIO (GCC, linkowanie) i NIE uruchamiane na plytce.
//   Logi z 2026-11-11 (panel + centrala na golej plytce): panel na biurku ma
//   wgrany build STARSZY niz te zrodla - jeden timeout daje w nim setki linii
//   "[USTAWIENIA] GET nieudany" (brak FIX-LOG-SPAM), a w adresie zapytan
//   widac reset_powod=task_wdt. Centrala podawala wtedy zewn=18.5C (symulacja)
//   i -1.0C dla pozostalych temperatur; te zrodla, uruchomione na PC z takimi
//   danymi, pokazuja je na kafelkach (scenariusz gola_plytka).
//   Naprawiono:
//   - [v1.2.0 DANE-NIEAKTUALNE] "sukces" warstwy sieciowej znaczy tylko, ze
//     centrala zamknela gniazdo. obslugaStatusFetch()/obslugaUstawieniaFetch()
//     nie sprawdzaly kodu HTTP, a przy bledzie JSON robily ciche `return` -
//     kafelki zostawaly ze starymi wartosciami, bez sladu w logu. Teraz kod
//     HTTP, tresc i JSON sa sprawdzane (diag::sprawdzOdpowiedz()), blad daje
//     linie "tag=STATUS|USTAWIENIA lvl=ERR zdarzenie=odpowiedz_nieczytelna
//     przyczyna=..." (najwyzej raz na 10 s), a dane sa uznawane za swieze
//     dopiero po poprawnym odczycie.
//   - [v1.2.0 DANE-NIEAKTUALNE] Wskaznik "dane nieaktualne" (wpis z
//     2026-09-12 nizej: "NIE podpiety") jest podpiety w galezi LVGL: po
//     PROG_NIEAKTUALNYCH_DANYCH_MS (10 s) bez poprawnego statusu kafelki
//     pokazuja "--", a wirnik pompy staje. Log: dane_nieaktualne /
//     dane_wrocily.
//   - [v1.2.0 WIFI-WYBOR] Laczenie po samej nazwie sieci bralo PIERWSZA
//     znaleziona siec (domyslny WIFI_FAST_SCAN), a nie najsilniejsza. Przy
//     dwoch centralach w zasiegu (prawdziwa + testowa: ta sama nazwa, haslo
//     i adres 192.168.4.1) panel mogl pokazywac dane nie tej, w ktorej
//     zmieniano wartosci. Teraz przy zimnym starcie: skan wszystkich kanalow
//     (wifiSkanujCentrale()) i polaczenie po BSSID z najsilniejsza.
//   - [v1.2.0 WIFI-WYBOR] Zapamietany BSSID nigdy nie byl porzucany: po
//     zniknieciu TEJ centrali panel nie laczyl sie z zadna inna az do
//     odlaczenia zasilania. Teraz po 3 nieudanych probach szuka od nowa.
//   - [v1.2.0 FIX-WDT-WIFI] polaczWifi() czekalo do 20 s bez karmienia
//     watchdoga (15 s, panic): bez centrali w zasiegu panel restartowalby
//     sie w kolko (wniosek z kodu i z testu na PC, nie z plytki).
//   - [v1.2.0 FIX-LOG-SPAM-KOMENDA] Jeden nieudany POST wypisywal linie
//     bledu w KAZDYM obrocie loop() (brakowalo komendaFetch.skwitujBlad()).
//   - [v1.2.0 BATERIA-BRAK] Bez ogniwa (0,28 V w logu) wskaznik pokazywal
//     "0%" w kolorze ladowania; teraz ponizej 2,5 V pokazuje "USB".
//   Dodano:
//   - [v1.2.0 CENTRALA-ID] Znacznik w naglowku dashboardu:
//     "SYMULACJA: zewn, pokoj", gdy prawdziwa centrala podaje czujniki jako
//     symulowane (pola "symulacja_*":true w /api/status - symulacja wlaczana
//     z jej panelu WWW); "SYMULATOR", gdy odpowiada centrala-testowa-c3;
//     "2 CENTRALE (XX:YY)", gdy przy starcie byly widoczne co najmniej dwie
//     sieci o nazwie centrali (XX:YY = koncowka BSSID tej, z ktora panel
//     jest polaczony). Jedna centrala bez symulacji = brak znacznika. Log:
//     "tag=PANEL zdarzenie=symulacja_centrali czujniki=...".
//   - [v1.2.0 DIAG] Linia "tag=PANEL fw=... bssid=... centrala=... sym=...
//     status_ok=... t_zewn=... t_ogrz=... dym=... heap=... blok_max=..." 5 s
//     po starcie i potem co 30 s: z kim panel rozmawia, ile odpowiedzi
//     odczytal i jakie wartosci podaje kafelkom (do porownania z linia
//     "tag=STATUS" w logu centrali). Linie "tag=WIFI
//     zdarzenie=skan_centrala|wiele_central|polaczono|reconnect_ok|
//     porzucam_bssid" z BSSID, kanalem i sila sygnalu.
//   Zmieniono:
//   - Hasla (WIFI_PASS, OTA_HASLO_PANEL) nie sa juz wpisane w tym pliku -
//     pochodza z src/secrets.h (poza gitem, wzor: secrets.h.example); ta
//     sama konwencja i te same dwie wartosci co w centrali.
//   - [v1.2.0 LOG-POZIOM] Linie "[NET-DIAG] start/wyslano/sukces" przy
//     kazdym zadaniu (3 linie co 2 s) sa domyslnie wylaczone
//     (SiecCfg::POZIOM_LOGU = 1: bledy + zadania wolniejsze niz 700 ms).
//   - Zimny start trwa o czas skanu dluzej (ok. 1,5-2 s).
//   NIE ZROBIONO: stary dashboard TFT (LVGL_POC_TEST=0, obecnie niebudowany)
//   dostal logi i poprawki WiFi, ale nie przelacza kafelkow na "--".
// [2026-09-12] Naprawiono: BLAD KOMPILACJI ("too few arguments to function
//   'lvgl_dashboard_poc_aktualizuj'") po pierwszym realnym build+upload tej
//   sesji. PRZYCZYNA: rozjazd dwoch rownoleglych wersji main.cpp - ta, na
//   ktorej oparta byla warstwa nieblokujaca (patrz wpis nizej), nie miala
//   jeszcze DESZCZ-ETAP4 (PLAN_DESZCZ_ZEWN.md), doklejonego w miedzyczasie
//   do dashboard_lvgl_poc.h (2 nowe parametry koncowe: deszczStan,
//   deszczMinuty) i do OSOBNEJ kopii main.cpp (starszej architektonicznie -
//   wciaz z blokujacym httpGet()/httpPost(), bez tego pliku non-blocking).
//   ZLACZONO obie linie rozwoju: dodano stan.deszczStan/deszczMinuty (pole
//   struct + parsowanie z /api/status pol deszcz_stan/deszcz_min w
//   obslugaStatusFetch()) i dopisano oba te argumenty na koncu WSZYSTKICH
//   TRZECH wywolan lvgl_dashboard_poc_aktualizuj() w TYM pliku (real -
//   obslugaStatusFetch() i setup() - oraz symulacja LVGL_POC_SYMULACJA,
//   gdzie brak centrali wiec wpisano 0,-1 "brak opadow" na sztywno) -
//   BEZ cofania architektury nieblokujacej (10 mostow LVGL swiadomie NIE
//   odzyskaly wlasnych bezposrednich wywolan tej funkcji - patrz
//   pendingPoKomendzie i uzasadnienie w poprzednim wpisie ponizej). Podniesiono
//   FIRMWARE_VERSION_PANEL v1.0.0 -> v1.1.0 (dwie funkcjonalne zmiany od
//   ostatniego wydania, jeszcze przed pierwszym realnym flashem tej wersji).
// [2026-09-12] Zmieniono: cala warstwa sieciowa (GET /api/status, GET
//   /api/ustawienia-get, POST /api/*-manual i /api/ustawienia-set) przepisana
//   z blokujacego HTTPClient (httpGet()/httpPost(), USUNIETE z tego pliku) na
//   nieblokujaca warstwe z siec_nieblokujaca_LCD.h (klasa NonBlockingHttp -
//   gole gniazda BSD, O_NONBLOCK + select() z zerowym czekaniem, ten sam
//   wzorzec co juz istniejacy wifiReconnectNonBlocking() w tym pliku).
//   PRZYCZYNA: przy wspolbieznym WIFI_MODE_APSTA (centrala), proba STA
//   centrali do domowego routera przerywa jej WLASNY SoftAP na czas
//   przelaczania kanalow (potwierdzony, dlugoletni znany problem sprzetowy
//   ESP32 - jeden radiowy front-end dzielony miedzy AP i STA, patrz
//   espressif/arduino-esp32#1040) - to fizycznie nie do naprawienia od
//   strony sieci, wiec dawny httpGet()/httpPost() (zawsze blokujace) mrozily
//   caly loop() (w tym obsluzDotyk()) na czas ich HTTP_TIMEOUT_MS (1500ms)
//   przy KAZDYM takim zaburzeniu. Research (10+ jezykow + doglebny przeglad
//   forow esp32.com/GitHub Espressif) potwierdzil, ze surowe nieblokujace
//   gniazda + select() to udokumentowany, pierwszorzedny wzorzec ESP-IDF
//   (examples/protocols/sockets/non_blocking), a dwie alternatywy (osobny
//   task FreeRTOS na drugim rdzeniu; esp_http_client z is_async=true) sa tu
//   gorszym wyborem - mieszalyby sie zle z glebokim usypianiem (idzSpac()) i
//   psulyby spojnosc z istniejacym wzorcem wifiReconnectNonBlocking().
//   ZMIANY W main.cpp: pobierzStatus()/pobierzUstawienia() to teraz same
//   "starty" zadania (statusFetch.startGet()/ustawieniaFetch.startGet());
//   parsowanie JSON-a + caly dawny "ogon" (aktualizacja sezonu/LVGL/starego
//   dashboardu) przeniesione do nowych obslugaStatusFetch()/
//   obslugaUstawieniaFetch(), wolanych z loop() co iteracje (w OBU galeziach
//   #if LVGL_POC_TEST, bo te funkcje byly wspoldzielone). Nowa funkcja
//   obslugaKomendaFetch() + flaga pendingPoKomendzie (enum PoKomendzie)
//   spinaja z powrotem POST+nastepujacy-po-nim-GET, ktore juz NIE moga byc
//   jednym synchronicznym wywolaniem (patrz obszerny komentarz przy
//   pendingPoKomendzie) - odkryto przy tym, ze siec_nieblokujaca_LCD.h mial
//   na wzgledzie tylko 3 miejsca POST, a w praktyce jest ich 10 (wszystkie
//   mosty LVGL: pompaManualWyslij/pompaWyborWyslij/serwoTrybWyslij/
//   serwoPozycjaWyslij/serwoUstawienieWyslij/pompaUstawienieWyslij/
//   pompaAntystopWyslij/dymProgWyslij/dymUstawienieWyslij/
//   alarmTempProgWyslij) + stary, nie-LVGL EKRAN_TRYBY/EKRAN_USTAWIENIA/
//   EKRAN_SERWO (przez wspoldzielone wyslijTrybPracy()/wyslijUstawienie()/
//   wyslijServoManualny()) - flaga rozwiazuje to bez oddzielnej maszyny
//   stanow na kazde z tych miejsc. Natychmiastowe odswiezenie po udanej
//   komendzie CELOWO zachowane (w odroznieniu od pierwotnego szkicu w
//   siec_nieblokujaca_LCD.h, patrz uwaga tam) - bez niego kazda zmiana z
//   dotyku odswiezalaby sie dopiero po 2-5s zamiast niemal natychmiast.
//   USUNIETO: blokujacy czerwony pasek+delay(1200) na blad POST-a w starym,
//   nie-LVGL EKRAN_TRYBY/EKRAN_USTAWIENIA (wyslijTrybPracy()/
//   wyslijUstawienie()) - to byla DRUGA blokujaca operacja w tym pliku;
//   teraz tylko log po Serialu (jak juz akceptowany wzorzec w mostach LVGL),
//   ekran i tak pokazuje prawdziwa wartosc po automatycznym
//   pobierzUstawienia() z obslugaKomendaFetch(). DODANO: jednorazowe,
//   OGRANICZONE (PIERWSZA_PROBKA_TIMEOUT_MS=2500ms) oczekiwanie na siec w
//   setup() (funkcja poczekajNaPierwszeDane()) - JEDYNE miejsce w pliku,
//   gdzie to jeszcze robimy, bo na starcie nie ma dotyku do zamrozenia, a
//   bez tego pierwszy rysujAktualnyEkran()/lvgl_dashboard_poc_aktualizuj()
//   i sprawdzIWykonajOtaPanelu() dostalyby same zera zamiast realnych
//   danych z centrali. NIE ZROBIONO w tej sesji (celowo, poza zakresem):
//   [dopisek z v1.3.0: punkt (1) zrobiony - wykresyFetchWykonaj() juz nie
//   ma, ekran Wykresy pobiera dane bez blokowania; patrz wpis v1.3.0 wyzej]
//   (1) wykresyFetchWykonaj() (GET /api/history, ekran Wykresy) NADAL uzywa
//   HTTPClient bezposrednio - to osobny, rzadko wywolywany, strumieniowany
//   przypadek (RAM, patrz FIX-WYKRESY-RAM), przy tym samym zaburzeniu
//   kanalu zamrozi tylko ten jeden ekran, nie caly dotyk; (2) wskaznik "dane
//   nieaktualne" na dashboardzie (WskaznikSwiezosci.saNieaktualne()) wdrozony
//   tylko na poziomie sledzenia (swiezoscStatusu/swiezoscUstawien.odnotujSukces()
//   wolane poprawnie), ale NIE podpiety pod zadne rysowanie w rysujNaglowek()/
//   dashboard_lvgl_poc.* - potencjalny nastepny krok. Patrz tez
//   siec_nieblokujaca_LCD.h (naglowek pliku, sekcja "[ZREALIZOWANE]") i
//   HISTORIA ZMIAN w PLAN_SD_LOGI_AWARYJNE.md/PLAN_LOGOWANIE_ANTYSPAM.md w
//   projekcie centrali dla podobnych, wczesniejszych zmian nieblokujacych.
// [2026-09-05] Dodano: obsluga przycisku WYCISZ na nowym EKRANIE ALARMOWYM
//   (dashboard_lvgl_poc.cpp) - patrz PLAN_EKRAN_ALARMOWY.md. Nowe:
//   g_brzeczykWyciszony/g_brzeczykWyciszonyTyp + funkcja alarmWyciszObsluga()
//   (zarejestrowana jako AlarmWyciszCb, w OBU galeziach #if
//   LVGL_POC_SYMULACJA - ten sam powod co WYSWIETLACZ powyzej: brzeczyk to
//   GPIO na TEJ plytce, wiec dziala identycznie niezaleznie od tego, czy
//   centrala jest akurat podlaczona). brzeczykOdswiezZAlarmow()
//   zmodyfikowane, zeby respektowac wyciszenie - patrz komentarz tam.
//   Wyciszenie jest CELOWO powiazane z KONKRETNYM aktywnym typem alarmu
//   (nie globalnym "cisza na zawsze") - samo znika, gdy typ sie zmieni
//   (alarm ustapi albo eskaluje), zeby nie ryzykowac zapomnianego,
//   wyciszonego na trwale przyszlego pozaru. Dotyczy tez
//   dashboard_lvgl_poc.h (typedef AlarmWyciszCb) i dashboard_lvgl_poc.cpp
//   (caly ekran) - patrz HISTORIA ZMIAN w obu tych plikach oraz
//   CHANGELOG.md [Unreleased].
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"    // [FIX-TFT-BL-HOLD, ta sesja] gpio_hold_en/gpio_hold_dis/gpio_deep_sleep_hold_en dla GPIO21 (TFT_BL)
#include <stdarg.h> // [v1.2.2] va_list w logBezPamieci()
#include <math.h>   // cosf/sinf — uzywane w prostych ikonach wektorowych (rysujIkonaSlonce/Snieg), patrz sekcja [UI]
#include "ikony.h"  // [LCD-IKONY] bitmapy 24x24 (plomien/kropla/suwak/alarm) - patrz komentarz w pliku

// [SIEC-NIEBLOKUJACA, ta sesja] Zastepuje synchroniczne HTTPClient::GET/POST
// (httpGet/httpPost, usuniete z tego pliku) - patrz obszerny naglowek w
// siec_nieblokujaca_LCD.h co do PRZYCZYNY (zaburzenie kanalu SoftAP/STA na
// centrali, potwierdzone jako znane ograniczenie sprzetowe ESP32 w trybie
// WIFI_MODE_APSTA - jeden radiowy front-end dzielony miedzy AP i STA, wiec
// skan/connect() po stronie STA centrali fizycznie przerywa AP na czas
// przelaczania kanalow; patrz espressif/arduino-esp32#1040) oraz ROZWIAZANIA
// (gole gniazda BSD w trybie O_NONBLOCK + select(), dokladnie ten wzorzec co
// oficjalny przyklad ESP-IDF protocols/sockets/non_blocking).
#include "siec_nieblokujaca_LCD.h"
// [v1.2.0 DIAG] Male, czyste funkcje diagnostyki (kod HTTP, dlawik logu,
// straznik swiezosci danych, wybor najsilniejszego AP, obecnosc baterii) -
// bez zaleznosci od Arduino, testowane na PC (testy_host/). Opis w pliku.
#include "diagnostyka_panelu.h"

// [OTA-ROLLBACK] Potrzebne do potwierdzania partycji po OTA (prompt #6) i,
// docelowo, do samego wgrywania aktualizacji (prompt #7). Ten sam naglowek
// WDT co w main_centrala.cpp ([FIX-WDT]) — panel dotad nie mial ZADNEJ
// konfiguracji Task WDT, patrz komentarz przy setup() nizej.
// [v1.4.0 OTA] Update.h zamiast HTTPUpdate.h: panel sam pobiera plik (otaFetch,
// bez zatrzymywania petli) i oddaje go do Update.write(). HTTPClient.h i
// HTTPUpdate.h nie sa juz uzywane nigdzie w tym pliku.
#include <Update.h>
#include <ArduinoOTA.h>   // [v1.5.4 OTA-VSCODE] wgrywanie z PlatformIO przez WiFi (espota, port 3232) - patrz otaVscodeKrok()
#include <esp_ota_ops.h>
#include <esp_partition.h>   // [v1.4.0 OTA] esp_partition_get_sha256() - identyfikator wlasnego programu
#include <esp_task_wdt.h>
#include <esp_wifi.h>    // [v1.2.0 WIFI-WYBOR] esp_wifi_scan_stop() - patrz wifiSkanujCentrale()
#include <esp_heap_caps.h>   // [v1.2.2 PAMIEC] heap_caps_get_*() - patrz ramWolny(); [v1.4.1] heap_caps_register_failed_alloc_callback()
#include <esp_core_dump.h>   // [v1.4.1 ZRZUT] skrot zrzutu po awarii z partycji coredump - patrz zrzutPoAwariiWypisz()
#include "esp_system.h"  // [RESET-DIAG, ta sesja] esp_reset_reason() / esp_reset_reason_t

// [OTA-ROLLBACK] Bez tego override arduino-esp32 SAM potwierdzi biezaca
// partycje jako VALID w initArduino(), zanim setup() zdazy cokolwiek
// sprawdzic — wywolanie esp_ota_mark_app_valid_cancel_rollback() ponizej
// byloby wtedy martwym kodem. Musi byc w zasiegu GLOBALNYM, poza
// jakakolwiek funkcja. Mechanizm zweryfikowany empirycznie w projekcie
// testowym ota_rollback_test — patrz Ustalenie #5 i #6 w
// OTA_PANEL_CALOSC.md.
extern "C" bool verifyRollbackLater() {
  return true;
}

// Interfejs panelu to LVGL (lvgl_port.cpp = sterownik ekranu/dotyku,
// dashboard_lvgl_poc.cpp = wszystkie ekrany). [v1.4.4 C1] Do v1.4.3 ten plik
// zawieral jeszcze DWA inne programy przelaczane makrami: stary dashboard
// rysowany wprost na TFT (LVGL_POC_TEST=0, ~1 400 linii) i lokalna symulacje
// danych bez centrali (LVGL_POC_SYMULACJA=1, ~350 linii). Oba byly od dawna
// niebudowane (makra na stale 1/0) - usuniete razem z makrami. Zachowanie
// firmware jest identyczne jak w v1.4.3; historia w git, gdyby byly potrzebne.
#include "lvgl_port.h"
#include "dashboard_lvgl_poc.h"

// ----------------------------------------------------------------------------
// KONFIGURACJA — podmien pod swoja siec i adres centrali
//
// [ETAP 3 / krok 1, PIEC_ESPNOW_PLAN.md] Wyswietlacz laczy sie teraz z
// WLASNYM Access Pointem centrali (Piec CO), a NIE z domowa siecia WiFi —
// patrz Podejscie A w planie. Wpisz tu DOKLADNIE te same wartosci, jakie
// centrala ma ustawione jako WIFI_AP_SSID / WIFI_AP_PASSWORD.
// Adres centrali to na stale 192.168.4.1 (domyslna brama Soft-AP ESP32)
// niezaleznie od tego, czy centrala ma akurat polaczenie z domowym routerem -
// [v1.4.4 C7] jedyne miejsce, w ktorym jest zapisany, to SiecCfg::HOST/PORT
// w siec_nieblokujaca_LCD.h (dawne CENTRALA_HOST/CENTRALA_PORT tutaj byly
// nieuzywanym duplikatem).
// ----------------------------------------------------------------------------
// [SEKRETY] Hasla NIE leza w tym pliku - sa w pliku "secrets.h" (obok, w src/),
// ktory jest wpisany do .gitignore i zostaje TYLKO u Ciebie na komputerze.
// Ta sama konwencja i TE SAME dwie wartosci co w centrali (main_centrala.cpp):
//   1. Skopiuj "secrets.h.example" -> "secrets.h" (folder src/ panelu).
//   2. Wpisz SECRET_WIFI_AP_PASSWORD i SECRET_OTA_HASLO - identyczne jak w
//      secrets.h centrali (mozna tez skopiowac caly tamten plik).
//   3. Dopiero potem kompiluj i wgraj.
// Bez secrets.h kompilacja ZATRZYMUJE SIE z czytelnym komunikatem.
#if __has_include("secrets.h")
  #include "secrets.h"
#else
  #error "Brak pliku src/secrets.h - skopiuj src/secrets.h.example jako src/secrets.h i wpisz hasla (te same co w secrets.h centrali)."
#endif
#ifndef SECRET_WIFI_AP_PASSWORD
  #error "secrets.h: brak SECRET_WIFI_AP_PASSWORD (haslo sieci centrali - to samo co w secrets.h centrali)."
#endif
#ifndef SECRET_OTA_HASLO
  #error "secrets.h: brak SECRET_OTA_HASLO (haslo OTA - to samo co w secrets.h centrali)."
#endif
static const char* WIFI_SSID     = "PiecCO_AP";         // SSID AP centrali (WIFI_AP_SSID w main.cpp centrali)
static const char* WIFI_PASS     = SECRET_WIFI_AP_PASSWORD;       // haslo AP centrali (WIFI_AP_PASSWORD w main.cpp centrali)

// [ETAP 3 KROK 2] Statyczne IP wyswietlacza — glowna, potwierdzona badaniem
// dzwignia szybkosci polaczenia po wybudzeniu (~200 ms zamiast 1,5-3 s,
// bo ESP32 nie czeka na DHCP). Musi byc w tej samej podsieci co AP centrali
// (192.168.4.x) i NIE moze kolidowac z .1 (brama/centrala) ani z adresami,
// ktore centrala moglaby przydzielic innym klientom AP przez DHCP.
IPAddress WIFI_IP_STATYCZNE(192, 168, 4, 50);
IPAddress WIFI_GATEWAY(192, 168, 4, 1);
IPAddress WIFI_MASKA(255, 255, 255, 0);

// [ETAP 3 KROK 2] Cache BSSID+kanalu centrali w pamieci RTC — przetrwa
// deep sleep (w odroznieniu od zwyklych zmiennych), zerowy koszt, przydatne
// przy czestym wybudzaniu/usypianiu (Etap 3b). Samo w sobie nie przyspiesza
// polaczenia na ESP32 (w przeciwienstwie do ESP8266) — patrz research w
// PIEC_ESPNOW_PLAN.md — ale pozwala uniknac ponownego skanowania kanalu.
RTC_DATA_ATTR uint8_t rtcBssidCentrali[6] = {0, 0, 0, 0, 0, 0};
RTC_DATA_ATTR int32_t rtcKanalCentrali = 0;
RTC_DATA_ATTR bool    rtcBssidZnany = false;

static const uint32_t STATUS_INTERWAL_MS     = 2000;   // jak czesto GET /api/status
// [v1.4.3 B4] Rytm GET /api/ustawienia-get: diag::USTAWIENIA_EKRAN_MS (5 s przy
// otwartym ekranie ustawien) / diag::USTAWIENIA_TLO_MS (60 s w tle) -
// decyduje rytmUstawien.czasNaGet() w loop(). Do v1.4.2: stale 5 s.
// Timeout pojedynczego zapytania HTTP: SiecCfg::CALKOWITY_TIMEOUT_MS (1500 ms)
// w siec_nieblokujaca_LCD.h - [v1.4.4 C7] dawny duplikat HTTP_TIMEOUT_MS usuniety.

// [SIEC-NIEBLOKUJACA, ta sesja] Trzy stale instancje - jedna na kazdy rodzaj
// ruchu (status/ustawienia/komenda), zeby wolny POST nie blokowal mozliwosci
// wystartowania kolejnego GET-a statusu. Definicje klas w
// siec_nieblokujaca_LCD.h; globalne obiekty tutaj (ANALOGICZNIE do
// rtcBssidCentrali wyzej - jedno miejsce prawdy, widoczne w calym pliku).
NonBlockingHttp statusFetch;
NonBlockingHttp ustawieniaFetch;
NonBlockingHttp komendaFetch;
static const uint32_t KOMENDA_LACZENIE_MS = 1000;   // [v1.5.5] limit laczenia POST-a polecenia (statusy: SiecCfg, od v1.5.6 tez 1000 ms)
static const uint32_t KOMENDA_CALOSC_MS   = 2000;   // [v1.5.5] limit calego POST-a (statusy: 1500 ms, od v1.5.6 3000 ms)
StrumienHttpGet wykresyFetch;   // [v1.3.0 WYKRESY] historia dla ekranu Wykresy, linia po linii (patrz obslugaWykresyFetch())
StrumienHttpGet otaFetch;       // [v1.4.0 OTA] nowy firmware panelu z centrali (/panel-ota.bin), tresc binarna (patrz otaKrok())
WskaznikSwiezosci swiezoscStatusu;
WskaznikSwiezosci swiezoscUstawien;
// [v1.2.0 DANE-NIEAKTUALNE] Bylo 3 * STATUS_INTERWAL_MS (6 s) i NIGDZIE nie
// bylo uzywane. Teraz to prog, po ktorym kafelki przechodza na "--" (patrz
// panelOznaczDaneJakoNieaktualne()). 5 okresow (10 s), zeby pojedyncze
// zaburzenie kanalu AP centrali (jej wlasna proba STA do routera, patrz
// HISTORIA ZMIAN z 2026-09-12) nie migalo ekranem.
static const uint32_t PROG_NIEAKTUALNYCH_DANYCH_MS = 5 * STATUS_INTERWAL_MS; // 10000 ms

// [v1.2.0 DIAG] Stan diagnostyki - patrz diagnostyka_panelu.h.
static const uint32_t PANEL_DIAG_PIERWSZA_MS = 5000;   // pierwsza linia "tag=PANEL" krotko po starcie
static const uint32_t PANEL_DIAG_INTERWAL_MS = 30000;  // kolejne co 30 s (ten sam rytm co tag=BATERIA)
diag::Liczniki panelLiczniki;                 // zerowane po kazdej linii "tag=PANEL"
diag::StraznikSwiezosci straznikSwiezosci;    // kiedy kafelki maja pokazac "--"
diag::Dlawik dlawikBleduStatusu;              // nieczytelna odpowiedz /api/status - log najwyzej co 10 s
diag::Dlawik dlawikBleduUstawien;             // to samo dla /api/ustawienia-get
// [v1.2.1 RATUNEK] Co panel robi, gdy nie ma danych z centrali - patrz opis
// przy diag::Ratunek w diagnostyka_panelu.h i panelRatunekKrok() nizej.
diag::Ratunek panelRatunek;                   // drabina: ponowne polaczenie z siecia -> restart panelu
bool ostatniBladStatusuToTresc = false;       // ostatnia nieudana proba /api/status: true = odpowiedz nieczytelna, false = brak odpowiedzi
char ostatniBladStatusuTekst[72] = "";        // ...jej opis do logu (np. "timeout calkowity", "connect() natychmiastowy blad errno=118 EHOSTUNREACH")
char ostatniBladStatusuEtap = '-';            // ...etap, na ktorym padla (NonBlockingHttp::etapBledu(); 'o' = odpowiedz nieczytelna, 'm' = brak pamieci na jej odczytanie)
int  ostatniBladStatusuNumer = 0;             // ...i numer bledu gniazda albo kod HTTP
bool ostatniBladStatusuToPamiec = false;      // [v1.2.2] ...true = panelowi zabraklo pamieci (patrz diag::PRZYCZYNA_PAMIEC)

// ----------------------------------------------------------------------------
// [v1.2.2 PAMIEC] Ile RAM-u panel NAPRAWDE ma
// ----------------------------------------------------------------------------
// ESP.getFreeHeap() / getMinFreeHeap() / getMaxAllocHeap() w arduino-esp32 3.x
// licza pamiec MALLOC_CAP_INTERNAL. Na ESP32 wchodzi w to takze niezajety
// kawalek obszaru na kod w RAM-ie (IRAM, 0x40080000-0x4009FFFF) - a z niego
// zwykly malloc() NIE przydziela (ESP-IDF, components/heap/port/esp32/
// memory_layout.c: typ "IRAM" ma MALLOC_CAP_EXEC|32BIT, bez
// MALLOC_CAP_DEFAULT). W logu panelu z 2026-11-12 stalo przez godzine
// "heap=47032 heap_min=39624 blok_max=38900" - z czego 38900 B to wlasnie ten
// obszar. malloc() mial do dyspozycji ok. 8 kB, a w najgorszej chwili ok.
// 0,7 kB, i stad "json=NoMemory" przy "wolnych" 40 kB. Ponizsze funkcje pytaja
// o MALLOC_CAP_DEFAULT, czyli dokladnie o to, z czego korzysta malloc() (a
// wiec ArduinoJson, String, LVGL i lwIP).
static inline uint32_t ramWolny() { return (uint32_t)heap_caps_get_free_size(MALLOC_CAP_DEFAULT); }
static inline uint32_t ramBlok()  { return (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT); }
static inline uint32_t ramMin()   { return (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT); }

// [v1.4.1 PAMIEC] Odmowy pamieci. System wola alokNieudanaHak() przy KAZDEJ
// nieudanej prosbie o pamiec (malloc/calloc/realloc i wszystko, co z nich
// korzysta: String, ArduinoJson, LVGL, WiFi). Funkcja moze byc wolana z
// dowolnego zadania i z wylaczonymi przerwaniami, wiec TYLKO zapisuje liczby -
// zadnego Serial, zadnej pamieci. Liczby wypisuje panelDiagOkresowo()
// (alok_nieudane=, alok_nieudana_b=). Zdrowy panel ma tu 0; kazda liczba
// wieksza od zera to prosba, ktorej nie dalo sie spelnic, i powod, zeby
// przeczytac log z tej minuty.
static volatile uint32_t alokNieudanych = 0;
static volatile uint32_t alokNieudanaOstatniaB = 0;
static void alokNieudanaHak(size_t rozmiar, uint32_t /*caps*/, const char* /*funkcja*/) {
  alokNieudanych = alokNieudanych + 1;
  alokNieudanaOstatniaB = (uint32_t)rozmiar;
}
// Najmniejszy zapas stosu petli (loopTask) od startu, w bajtach. Stos ma
// 8192 B (arduino-esp32 3.x); na nim pracuje LVGL, ArduinoJson i aktualizacja.
// Gdy ta liczba spada ponizej ok. 1 kB, nastepny glebszy fragment moze
// przepelnic stos - a to wyglada w logu jak losowy restart.
static inline uint32_t stosZapasMin() { return (uint32_t)uxTaskGetStackHighWaterMark(NULL); }
diag::Dlawik dlawikMaloPamieci;               // ostrzezenie "malo_pamieci" - najwyzej raz na minute

// Linia logu, ktora wypisze sie TAKZE przy braku pamieci. Serial.printf()
// dla tekstu od 64 znakow sam prosi o pamiec (Print::vprintf w arduino-esp32
// 3.0.7 i 3.3.8: bufor 64 B na stosie, dla dluzszego tekstu
// malloc(dlugosc + 1)) i gdy jej nie dostanie, linia przepada bez sladu - a
// wlasnie wtedy linie o braku pamieci sa potrzebne. Tutaj tekst powstaje w
// stalym buforze: ani sterta, ani stos (wazne w panel_lvgl_assert(), wolanym
// z glebi rysowania LVGL). Wolac TYLKO z zadania loop() - bufor jest jeden.
// Liczby zmiennoprzecinkowe (%f) dzialaja tu tak samo jak w Serial.printf():
// biblioteka C (newlib, mprec.c) bierze na nie kilka malych blokow pamieci
// przy pierwszym uzyciu w danym zadaniu i potem uzywa ich ponownie. Gdyby
// juz wtedy pamieci nie bylo, newlib przerywa program (assert "Balloc
// succeeded", restart z powodem "panic") - w obu funkcjach jednakowo.
static void logBezPamieci(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void logBezPamieci(const char *fmt, ...) {
  static char linia[768];   // najdluzsza linia ("tag=PANEL fw=...") ma w najgorszym razie ok. 670 znakow (v1.4.1: +3 pola; v1.4.6: centrala_fw=; v1.4.7: cpu_mhz= tx_dbm= tx_ok=)
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(linia, sizeof(linia), fmt, ap);
  va_end(ap);
  if (n >= (int)sizeof(linia)) linia[sizeof(linia) - 2] = '\n';   // obcieta - niech przynajmniej konczy sie nowa linia
  Serial.print(linia);
}

// Czego panelowi brakuje, gdy nie ma danych z centrali - jedna funkcja dla
// naglowka ekranu, logu i drabiny ratunkowej (diag::Przyczyna).
static diag::Przyczyna panelPrzyczynaTeraz() {
  return diag::ustalPrzyczyne(WiFi.status() == WL_CONNECTED, ostatniBladStatusuToTresc, ostatniBladStatusuToPamiec);
}
// Przetrwa ESP.restart() (pamiec RTC, ktorej program przy starcie nie
// zeruje). Po wlaczeniu zasilania zawiera przypadkowe wartosci - sprawdzane
// i czyszczone w autorestartOdczytajPrzyStarcie(), zanim cokolwiek ich uzyje.
RTC_NOINIT_ATTR diag::PamiecAutorestartu rtcAutorestart;
bool startPoAutorestarcie = false;            // true = ten start nastapil po samoczynnym restarcie (panelAutorestart())
bool autorestartDoZgloszenia = false;         // true = w zapytaniach /api/status idzie powod "auto_...", a centrala jeszcze na zadne nie odpowiedziala
// Od kiedy dane sa nieprzerwanie poprawne (do zerowania licznika restartow
// z rzedu - patrz diag::CZAS_STABILNYCH_DANYCH_MS).
bool daneDobreSeria = false;
unsigned long daneDobreOdMs = 0;
// Kod przyczyny ostatniego rozlaczenia WiFi i licznik rozlaczen. Wpisuje je
// zadanie zdarzen WiFi (wifiZdarzenie()), czyta petla glowna - stad volatile.
volatile uint8_t  wifiOstatniKodRozlaczenia = 0;
volatile uint32_t wifiLiczbaRozlaczen = 0;
// [v1.2.0 CENTRALA-ID] Z KTORA centrala rozmawia panel. Prawdziwa centrala i
// centrala testowa (centrala-testowa-c3) nadaja TA SAMA siec (WIFI_SSID) pod
// tym samym adresem - bez tego nie da sie odroznic ich ani z ekranu, ani z
// logu. `centralaToSymulator` pochodzi z pola "symulator":true w /api/status
// (wysyla je tylko centrala testowa); `wifiCentralPrzyStarcie` to liczba AP
// o naszej nazwie widzianych przy ostatnim skanowaniu (patrz polaczWifi()).
bool centralaToSymulator = false;
bool centralaRozpoznana = false;   // czy chocby jeden status zostal poprawnie odczytany
int  wifiCentralPrzyStarcie = 0;   // 0 = nie skanowano (szybka sciezka z BSSID z pamieci RTC)
// Ktore czujniki PRAWDZIWA centrala podaje w tej chwili jako symulowane
// (pola "symulacja_<czujnik>":true w /api/status - symulacja wlaczana z jej
// panelu WWW). Bit i = diag::CZUJNIKI_SYMULOWANE[i]. Niezerowa maska znaczy:
// te wartosci na kafelkach nie sa pomiarem.
uint16_t centralaSymulacjaMaska = 0;

// [SIEC-NIEBLOKUJACA, ta sesja] POST przez komendaFetch i nastepujacy po nim
// GET (odswiezenie /api/status albo /api/ustawienia-get) NIE MOGA juz byc
// jednym synchronicznym wywolaniem jak dawniej (httpPost() zwracal bool
// dopiero PO calej wymianie) - GET musi poczekac, az POST faktycznie sie
// zakonczy (patrz obslugaKomendaFetch() przy pobierzStatus()/
// pobierzUstawienia() nizej). Ta flaga niesie "co zrobic PO zakonczeniu
// biezacej komendy" przez ten czas oczekiwania - ustawiana TUZ PRZED
// wystartowaniem POST-a (po sprawdzeniu komendaFetch.wTrakcie(), wiec zawsze
// odpowiada REALNIE wystartowanemu zadaniu, nigdy odrzuconemu), zerowana
// zaraz po odczytaniu w obsludze. USTAWIENIA jest ustawiana WEWNATRZ
// wyslijTrybPracy() (wspolny mianownik ich wywolujacych -
// patrz komentarze tam); STATUS ustawiaja jawnie tylko te 4 mosty LVGL,
// ktorych ekran pokazuje pola z /api/status, nie z /api/ustawienia-get
// (pompaManualWyslij/pompaWyborWyslij/serwoTrybWyslij/serwoPozycjaWyslij).
enum class PoKomendzie : uint8_t { BRAK, STATUS, USTAWIENIA };
PoKomendzie pendingPoKomendzie = PoKomendzie::BRAK;

// [v1.4.2 C4] Kolejka polecen - zamiast "poprzednia komenda jeszcze w toku,
// pomijam" (co przy szybkim klikaniu gubilo co drugie dotkniecie, a przy
// przytrzymanym przycisku wiekszosc wartosci). Logika w
// diag::KolejkaPolecen (diagnostyka_panelu.h, testowana na PC); tu tylko
// spiecie z komendaFetch: poleceniePlanuj() dodaje, poleceniaKrok() (z
// loop() i po kazdym zakonczonym POST) wysyla, gdy kanal wolny.
// `partiaStatus`/`partiaUstawienia`: co odswiezyc po OSTATNIM poleceniu z
// serii - w trakcie serii GET-y nie sa potrzebne (i tak pominelyby pola w
// kolejce), a po niej jeden GET pokazuje wartosci zatwierdzone przez centrale.
diag::KolejkaPolecen kolejkaPolecen;
bool partiaStatus = false, partiaUstawienia = false;
// [v1.4.2 C4] Kiedy wystartowal biezacy GET /api/ustawienia-get - pola, ktorych
// polecenie skonczylo sie PO tej chwili, sa w odpowiedzi nieaktualne
// (kolejkaPolecen.poleZmieniane()). `ustawieniaGetPonow`: pobierzUstawienia()
// zawolane, gdy GET juz trwal - do v1.4.1 takie wywolanie przepadalo bez
// sladu (ekran zostawal ze stara wartoscia do nastepnego okresowego GET-a,
// nawet 5 s), teraz GET rusza ponownie zaraz po biezacym.
unsigned long ustawieniaGetStartMs = 0;
// [v1.4.3 B4] To samo dla /api/status - od v1.4.3 status niesie tez tryb
// pompy i serwa, wiec i on podlega ochronie przed stara wartoscia.
unsigned long statusGetStartMs = 0;
diag::RytmUstawien rytmUstawien;
bool ustawieniaGetPonow = false;

// [OTA-PANEL, ta sesja] Wersja wkompilowana w ten firmware — zglaszana
// centrali przy KAZDYM /api/status (patrz pobierzStatus() nizej), zeby centrala
// wiedziala co panel realnie ma wgrane (panel nie ma internetu, wiec nie
// moze sam nic potwierdzic Telegramowi). PODNIES RECZNIE przy kazdym
// Release'u firmware_panel.bin — bez tego centrala nigdy nie zobaczy
// potwierdzenia udanej aktualizacji (patrz TG_UPDATE_PANEL w main_centrala.cpp).
static const char* FIRMWARE_VERSION_PANEL = "v1.5.18"; // [2026-11-24] v1.5.18: kafelek ZEWN bez czujnika - temperatura z prognozy jako "3.4°C" (jak pomiar: 1 miejsce po przecinku, bez tyldy i zaokraglania; w v1.5.17 "~3°C"); opis w HISTORII ZMIAN. [2026-11-23] v1.5.17: kafelek ZEWN uklad "E" - napis o pogodzie (2 linie) zamiast tytulu, bez paska, bez czujnika temperatura z prognozy "~3°C" (pogoda_temp z centrali 3.58.17); opis w HISTORII ZMIAN. v1.5.16: tlo wykresu = tlo ekranu (bez jasniejszej karty do rogow); v1.5.15: ekran Wykres na caly ekran - legenda z trybem w jednym pasku u gory zamiast tytulu + 2 wierszy legendy, padding 4 px; opis w HISTORII ZMIAN. v1.5.14: kafelek ZEWN z paskiem pogody (deszcz/snieg/burza/mgla/wiatr/stan nieba) i 13 kolorowymi ikonami wg kodu WMO z centrali 3.58.16; opis w HISTORII ZMIAN. v1.5.13: kafelek ZEWN przy deszczu pokazuje temperature I napis o deszczu (drugi wiersz), nie zamiast niej; opis w HISTORII ZMIAN. v1.5.12: wykresy - seria POMPA jako pas pracy, etykiety osi obok wykresu, nazwy serii jak na pulpicie, WYCZYSC; opis w HISTORII ZMIAN. v1.5.11: ikony BOJLER (poziom i kolor wody wg temperatury) i PANEL (kolektor z 4 warstw) jak w aplikacji Android; opis w HISTORII ZMIAN. v1.5.10: galki suwakow jasnosci w calosci widoczne, zielone przyciski "wlaczone" ciemniejsze z bialym napisem; opis w HISTORII ZMIAN. v1.5.9: margines od obudowy na podpisach wykresow, luki serw i suwaki jasnosci bez przyciecia; opis w HISTORII ZMIAN. v1.5.8: os czasu wykresow podpisana przy kazdej kresce (rowne godziny), samotne punkty jako znaczniki, suwaki serw nie wchodza pod luk; opis w HISTORII ZMIAN
// [OTA-PANEL, ta sesja] MUSI byc identyczne z OTA_HASLO w main_centrala.cpp
// (patrz endpoint GET /panel-ota.bin) — celowo zdublowane jako stala tekstowa
// zamiast wspolnego naglowka, bo to dwa osobne projekty PlatformIO bez
// wspolnego katalogu include/. Zmieniajac jedno, zmien i drugie.
static const char* OTA_HASLO_PANEL = SECRET_OTA_HASLO;
// [v1.4.0 OTA] Limity czasu pobierania firmware'u sa w diagnostyka_panelu.h
// (diag::OTA_CISZA_MS: 15 s bez danych konczy probe; diag::OTA_CALOSC_MS:
// 5 minut na calosc). Dawny OTA_TIMEOUT_MS (20 s w HTTPUpdate) zniknal.

// [v1.3.0 WYKRESY] Limity czasu pobierania historii dla ekranu Wykresy sa od
// tej wersji w siec_nieblokujaca_LCD.h (SiecCfg::STRUMIEN_*): pobieranie nie
// blokuje juz petli, wiec dawny HISTORIA_TIMEOUT_MS (8 s na calosc) zniknal.

// [ETAP 5, PIEC_ESPNOW_PLAN.md] Reconnect WiFi z rosnacym opoznieniem miedzy
// probami (1s -> 2s -> 4s -> ... -> WIFI_RECONNECT_MAX_MS), zamiast probowac
// co iteracje loop() — research (watki forum ESP32/Arduino, artykuly o
// exponential backoff dla WiFi na ESP32) potwierdza, ze natychmiastowe,
// powtarzane WiFi.begin() po zerwaniu polaczenia "zasypuje" AP probami i jest
// niezalecane; a domyslny auto-reconnect biblioteki bywa niewystarczajaco
// niezawodny (arduino-esp32 issue #653) — stad wlasna, jawna logika nizej
// (wifiReconnectNonBlocking(), wywolywana w loop()).
static const uint32_t WIFI_RECONNECT_MAX_MS = 30000;
uint32_t wifiOpoznienieReconnectMs = 1000;
unsigned long wifiOstatniaProbaReconnectMs = 0;

// ----------------------------------------------------------------------------
// TFT + DOTYK
// ----------------------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();

Preferences kalibracja;   // NVS namespace "dotyk" — kalibracja XPT2046

// --- Dotyk: WLASNY sterownik, osobna magistrala SPI (patrz FIX-DOTYK-OSOBNA-MAGISTRALA) ---
#define TOUCH_SCK   25
#define TOUCH_MOSI  26
#define TOUCH_MISO  33
#define TOUCH_CS    27
// Prog "nacisniecia" na podstawie zlozonego wzoru Z1+Z2 (patrz funkcja
// touchOdczytajSurowy nizej) — inna skala niz sam Z1, wiec wartosc startowa
// do potwierdzenia/dostrojenia na podstawie live debug w Serial.
#define TOUCH_Z_PROG 400

// Jesli po kalibracji okaze sie, ze ruch pozioma/pionowy jest zamieniony
// albo odwrocony wzgledem oczekiwanego — przelacz te flagi (nie trzeba
// przelutowywac niczego, to tylko zamiana interpretacji odczytu w kodzie).
#define TOUCH_SWAP_XY   0
#define TOUCH_INVERT_X  0
#define TOUCH_INVERT_Y  0

// [v1.5.2] Orientacja ekranu (TFT_eSPI setRotation): 1 = pozioma 480x320 ze
// zlaczem po prawej (tak byl kalibrowany kazdy dotychczasowy panel), 3 = ta
// sama pozioma obrocona o 180 stopni. Panel w obudowie z ogniwem (24.11) ma
// zlacze po lewej, wiec 3. Kalibracja dotyku z NVS jest z orientacji, w
// ktorej ja zrobiono (klucz dotyk/obrot; brak klucza = 1) - przy innej
// orientacji ekranu dotykOdczytaj() odwraca obie osie, zeby nie trzeba
// bylo kalibrowac od nowa po samej zmianie tej stalej.
#define EKRAN_OBROT 3
static bool dotykOdwrocony = false;   // kalibracja z obrotu 1, ekran w obrocie 3 (albo odwrotnie)

struct KalibracjaDotyku {
  uint16_t rawXA, rawYA;  // surowy odczyt w punkcie A (gorny-lewy znacznik)
  uint16_t rawXB, rawYB;  // surowy odczyt w punkcie B (dolny-prawy znacznik)
};
KalibracjaDotyku kalDotyku;

// Wspolrzedne znacznikow kalibracyjnych na ekranie (landscape 480x320)
const int16_t KAL_A_X = 30,  KAL_A_Y = 30;
const int16_t KAL_B_X = 450, KAL_B_Y = 290;

void touchSpiDelay() { delayMicroseconds(3); }

uint8_t touchXferByte(uint8_t out) {
  uint8_t in = 0;
  for (int i = 7; i >= 0; i--) {
    digitalWrite(TOUCH_MOSI, (out >> i) & 0x01);
    digitalWrite(TOUCH_SCK, LOW);
    touchSpiDelay();
    in <<= 1;
    if (digitalRead(TOUCH_MISO)) in |= 0x01;
    digitalWrite(TOUCH_SCK, HIGH);
    touchSpiDelay();
  }
  return in;
}

uint16_t touchReadChannel(uint8_t cmd) {
  digitalWrite(TOUCH_CS, LOW);
  touchSpiDelay();
  touchXferByte(cmd);
  uint8_t hi = touchXferByte(0x00);
  uint8_t lo = touchXferByte(0x00);
  digitalWrite(TOUCH_CS, HIGH);
  return ((uint16_t)hi << 8 | lo) >> 3;   // 12-bitowy wynik
}


void touchInicjalizujPiny() {
  pinMode(TOUCH_SCK, OUTPUT);
  pinMode(TOUCH_MOSI, OUTPUT);
  pinMode(TOUCH_CS, OUTPUT);
  pinMode(TOUCH_MISO, INPUT);
  digitalWrite(TOUCH_CS, HIGH);
  digitalWrite(TOUCH_SCK, LOW);
  digitalWrite(TOUCH_MOSI, LOW);
}

// Surowy odczyt (bez kalibracji/mapowania) — zwraca true jesli wykryto nacisk.
// DEBOUNCE: wymaga kilku KOLEJNYCH probek nad progiem (nie jednej) - pojedyncze
// skoki Z ponizej progu 400 sa normalnym szumem tla nawet bez dotyku, wiec
// jedna probka nie wystarczy, zeby uznac to za realny dotyk.
#define TOUCH_WYMAGANE_PROBKI 4

bool touchOdczytajSurowy(uint16_t &rawX, uint16_t &rawY) {
  int licznik = 0;
  uint16_t x = 0, y = 0;
  for (int i = 0; i < TOUCH_WYMAGANE_PROBKI; i++) {
    uint16_t z1 = touchReadChannel(0xB0);
    uint16_t z2 = touchReadChannel(0xC0);
    int32_t z = (4095 - (int32_t)z2) + (int32_t)z1;
    x = touchReadChannel(0x90);
    y = touchReadChannel(0xD0);
    if (z >= TOUCH_Z_PROG) {
      licznik++;
    } else {
      licznik = 0;   // przerwij serie - potrzeba KOLEJNYCH probek, nie sumy
      break;
    }
  }
  rawX = x;
  rawY = y;
  return licznik >= TOUCH_WYMAGANE_PROBKI;
}

// Zbiera kilka probek POZYCJI w trakcie trwania dotyku i zwraca median —
// pojedynczy odczyt X/Y w momencie samego wykrycia dotyku bywa niestabilny
// (np. na styku "dotkniecie/puszczenie" panelu rezystancyjnego), wiec kilka
// probek + mediana daje dużo pewniejszy wynik. Jesli dotyk zniknie w trakcie
// zbierania (falszywy alarm), zwraca false.
bool touchPobierzStabilnaPozycje(uint16_t &outX, uint16_t &outY) {
  const int PROBKI = 5;
  uint16_t xs[PROBKI], ys[PROBKI];
  for (int i = 0; i < PROBKI; i++) {
    uint16_t rx, ry;
    if (!touchOdczytajSurowy(rx, ry)) return false;   // dotyk znikl - falszywy alarm
    xs[i] = rx; ys[i] = ry;
    delay(15);
  }
  // sortowanie babelkowe (tablica jest mala, wydajnosc bez znaczenia)
  for (int i = 0; i < PROBKI; i++)
    for (int j = i + 1; j < PROBKI; j++) {
      if (xs[j] < xs[i]) { uint16_t t = xs[i]; xs[i] = xs[j]; xs[j] = t; }
      if (ys[j] < ys[i]) { uint16_t t = ys[i]; ys[i] = ys[j]; ys[j] = t; }
    }
  outX = xs[PROBKI / 2];
  outY = ys[PROBKI / 2];
  return true;
}

// Odczyt z zastosowaniem kalibracji — zwraca wspolrzedne piksela ekranu
bool dotykOdczytaj(int16_t &px, int16_t &py) {
  // Zabezpieczenie: jesli kalibracja jest zdegenerowana (oba punkty
  // identyczne/za blisko), map() dzielilby przez zero - olej dotyk zamiast
  // zwracac smieci.
  if (abs((int)kalDotyku.rawXB - (int)kalDotyku.rawXA) < 50 ||
      abs((int)kalDotyku.rawYB - (int)kalDotyku.rawYA) < 50) {
    return false;
  }

  uint16_t rawX, rawY;
  if (!touchOdczytajSurowy(rawX, rawY)) return false;

  if (TOUCH_SWAP_XY) { uint16_t t = rawX; rawX = rawY; rawY = t; }

  long x = map(rawX, kalDotyku.rawXA, kalDotyku.rawXB, KAL_A_X, KAL_B_X);
  long y = map(rawY, kalDotyku.rawYA, kalDotyku.rawYB, KAL_A_Y, KAL_B_Y);

  if (TOUCH_INVERT_X) x = 480 - x;
  if (TOUCH_INVERT_Y) y = 320 - y;
  if (dotykOdwrocony) diag::dotykObroc180(x, y);   // [v1.5.2] ekran obrocony o 180 stopni wzgledem kalibracji

  px = constrain(x, 0, 479);
  py = constrain(y, 0, 319);
  return true;
}

// Podswietlenie (LED) na pinie TFT_BL (GPIO21, patrz platformio.ini) — PRZEZ
// REZYSTOR ~100-220R w szeregu miedzy GPIO a pinem LED na module, nie na styk.
// Sterowane PWM zamiast na sztywno do 3.3V, wiec mozna przygasic jesli za jasno.
// [v1.5.0 SEN] true = panel na baterii przyciemnil ekran po minucie bez dotyku
// (senKrok() nizej). Dopoki trwa, KAZDE ustawienie jasnosci z innych miejsc
// (harmonogram dzien/noc, suwak) jest przycinane do diag::SEN_JASNOSC_PROC -
// inaczej zmiana okna dzien/noc w srodku nocy rozjasnialaby uspiony ekran.
static bool senPrzyciemnione = false;
bool senEkranPrzyciemniony() { return senPrzyciemnione; }   // [v1.5.1] dla filtru dotyku w lvgl_port.cpp
// [v1.5.1] Indeksy w diag::SEN_PRZYCIEMNIJ_OPCJE_MS[] / SEN_USPIJ_OPCJE_MS[]
// (ekran WYSWIETLACZ, wiersz "Na baterii"); NVS wyswietlacz/senPrzyIdx, senSpijIdx.
static uint8_t senPrzyciemnijIdx = diag::SEN_PRZYCIEMNIJ_DOMYSLNY_IDX;
static uint8_t senUspijIdx       = diag::SEN_USPIJ_DOMYSLNY_IDX;
static_assert(diag::SEN_OPCJI == LVGL_SEN_OPCJI, "napisy na przyciskach \"Na baterii\" (dashboard_lvgl_poc.cpp) musza odpowiadac diag::SEN_*_OPCJE_MS[]");
// [v1.5.1] true = ten start to wybudzenie z glebokiego snu (ext0 przycisk /
// ext1 ladowarka): bez ekranu startowego, pulpit od razu (patrz polaczWifi()).
static bool startPoSnie = false;

void ustawJasnoscPodswietlenia(uint8_t procent) {
  procent = constrain(procent, (uint8_t)0, (uint8_t)100);
  if (senPrzyciemnione && procent > diag::SEN_JASNOSC_PROC) procent = diag::SEN_JASNOSC_PROC;
  ledcWrite(TFT_BL, map(procent, 0, 100, 0, 255));
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Jasnosc podswietlenia (dzien/
// noc) + automatyczne przyciemnienie w oknie godzinowym. W ODROZNIENIU od
// WSZYSTKICH innych ustawien w tym pliku (POLA[]/wybor/trybSerwa/
// antystopWlaczony) te wartosci NIE naleza do centrali - sa czysto lokalna
// wlasnoscia TEJ PLYTKI (fizyczne podswietlenie TFT_BL), wiec nie ma tu
// zadnego httpPost()/wyslijTrybPracy(). Zamiast tego zapisujemy je w lokalnej
// pamieci NVS (Preferences, DOKLADNIE ten sam mechanizm co `kalibracja` dla
// dotyku wyzej - patrz kalibrujDotykJesliTrzeba()) i stosujemy bezposrednio
// przez ustawJasnoscPodswietlenia() powyzej.
//
// Ta plytka NIE MA wlasnego RTC (patrz komentarz przy stan.godzina nizej) -
// "aktualna godzina" do porownania z oknem przyciemnienia jest wiec zawsze
// OSTATNIA znana godzina, badz to prawdziwa z centrali (stan.godzina w
// pobierzStatus()). Zamiast przekazywac ja jako parametr do kazdej
// funkcji ponizej, pobierzStatus() zapisuje ja tutaj, w DWOCH wspolnych
// zmiennych (wyswietlaczOstatniaGodzina/wyswietlaczGodzinaZnana) - patrz
// wywolanie wyswietlaczHarmonogramOdswiez() w pobierzStatus().
Preferences ustawieniaWyswietlacza;   // NVS namespace "wyswietlacz"
// [v1.4.1 C9] Dolna granica obu jasnosci. 0 % gasilo podswietlenie calkiem -
// takze po restarcie (wartosc jest w NVS) - i wtedy ekranu alarmu nie widac,
// a suwak trzeba przesuwac na slepo. Ta sama liczba jest w
// dashboard_lvgl_poc.h (JASNOSC_MIN_PROCENT_LVGL) jako dolny kraniec suwakow.
static const uint8_t JASNOSC_MIN_PROCENT = 5;
uint8_t jasnoscDziennaWartosc = 100;  // %, JASNOSC_MIN_PROCENT-100 - jasnosc POZA oknem przyciemnienia
uint8_t jasnoscNocnaWartosc   = 30;   // %, JASNOSC_MIN_PROCENT-100 - jasnosc WEWNATRZ okna przyciemnienia
uint8_t godzinaStartPrzyciemnienia  = 22; // 0-23 - godzina rozpoczecia okna
uint8_t godzinaKoniecPrzyciemnienia = 6;  // 0-23 - godzina zakonczenia okna
bool    harmonogramWlaczonyWartosc  = true; // false = zawsze jasnoscDzienna, godziny ignorowane

int  wyswietlaczOstatniaGodzina = 0;     // ostatnia znana godzina (real. lub sym.), patrz komentarz wyzej
bool wyswietlaczGodzinaZnana    = false; // false dopoki 1. pobierzStatus() nie ustawi
bool wyswietlaczPrzyciemnionyTeraz = false; // cache "co jest aktualnie zastosowane", zeby nie spamowac ledcWrite()

// ----------------------------------------------------------------------------
// [v1.4.7 ZASILANIE] Taktowanie procesora i moc nadajnika WiFi - do wyboru z
// ekranu ZASILANIE (drugi pulpit), pamietane w NVS. Audyt z 2026-11-18, tematy
// 11 (B3: "240 MHz na stale, a panel tylko rysuje i pyta co 2 s") i 18 (B7:
// pelna moc nadajnika do centrali stojacej czesto w tym samym pomieszczeniu).
//
// DLACZEGO PRZELACZNIK, A NIE STALA: nikt nie zmierzyl jeszcze tego panelu
// amperomierzem (szacunek z audytu: 120-160 mA przy pracy, 0,11 mA w deep
// sleep). Zamiast zgadywac, panel dostaje przelaczniki + zywy odczyt tego,
// co uklad naprawde robi (ekran ZASILANIE i pola cpu_mhz=/tx_dbm= w linii
// tag=PANEL co 30 s), a uzytkownik z testerem USB w przelotce porownuje
// pobor miedzy ustawieniami sam. Domyslnie indeks 0 = DOKLADNIE jak do
// v1.4.6 (240 MHz, 19,5 dBm) - nowa wersja niczego nie zmienia, dopoki ktos
// nie dotknie ekranu.
//
// CO WOLNO (esp32-hal-cpu.h, arduino-esp32 3.x): przy wlaczonym WiFi tylko
// 240/160/80 MHz - ponizej 80 MHz radio przestaje dzialac, wiec 40/20/10 MHz
// celowo nie ma na liscie. APB (80 MHz) nie zalezy od taktowania CPU w tym
// zakresie, wiec SPI ekranu (27 MHz), PWM podswietlenia/brzeczyka, UART i
// millis() dzialaja bez zmian; wolniejsze jest TYLKO liczenie - rysowanie
// LVGL (przy 80 MHz ok. 3x dluzej, dalej duzo ponizej 15 s watchdoga),
// parsowanie JSON i pobieranie OTA.
//
// MOC NADAJNIKA: WiFi.setTxPower() to esp_wifi_set_max_tx_power(), ktore
// ZWRACA BLAD przed esp_wifi_start() - dlatego wolane dopiero PO
// WiFi.mode(WIFI_STA) w polaczWifi() (temat 18 audytu), a potem ponownie po
// kazdym WiFi.begin() (tanie; na wypadek, gdyby sterownik kiedys wrocil do
// wartosci domyslnej przy ponownym starcie radia). Wartosci wifi_power_t to
// cwiartki dBm (78 = 19,5 dBm); 8,5 dBm to ok. 12x mniej mocy niz 19,5 dBm,
// przy dobrym sygnale (>= -65 dBm) wciaz z duzym zapasem. Przy slabym
// sygnale ekran ZASILANIE ostrzega, zeby mocy nie obnizac.
// ----------------------------------------------------------------------------
static const uint32_t ZASILANIE_CPU_MHZ[] = { 240, 160, 80 };
static const uint8_t  ZASILANIE_CPU_N = sizeof(ZASILANIE_CPU_MHZ) / sizeof(ZASILANIE_CPU_MHZ[0]);
static const wifi_power_t ZASILANIE_TX[] = { WIFI_POWER_19_5dBm, WIFI_POWER_15dBm, WIFI_POWER_11dBm, WIFI_POWER_8_5dBm };
static const uint8_t  ZASILANIE_TX_N = sizeof(ZASILANIE_TX) / sizeof(ZASILANIE_TX[0]);
static_assert(sizeof(ZASILANIE_CPU_MHZ) / sizeof(ZASILANIE_CPU_MHZ[0]) == LVGL_ZASILANIE_CPU_N, "napisy na przyciskach CPU (dashboard_lvgl_poc.cpp) musza odpowiadac ZASILANIE_CPU_MHZ[]");
static_assert(sizeof(ZASILANIE_TX) / sizeof(ZASILANIE_TX[0]) == LVGL_ZASILANIE_TX_N, "napisy na przyciskach nadajnika (dashboard_lvgl_poc.cpp) musza odpowiadac ZASILANIE_TX[]");
uint8_t zasilanieCpuIdx = 0;   // indeks w ZASILANIE_CPU_MHZ[]; NVS "cpuIdx"
uint8_t zasilanieTxIdx  = 0;   // indeks w ZASILANIE_TX[];      NVS "txIdx"
static bool zasilanieTxOstatniWynik = false;   // czy ostatnie WiFi.setTxPower() sie powiodlo (do linii tag=PANEL)

void zasilaniePomiarNaEkran();   // nizej
static float zasilanieTxDbm() { return (float)(int)WiFi.getTxPower() * 0.25f; }   // wifi_power_t -> dBm

// Taktowanie: przy starcie (koniec setup(), gdy ekran juz stoi - kalibracja
// dotyku, laczenie i pierwsze rysowanie ida zawsze na 240 MHz) i po kazdej
// zmianie z ekranu. Jedna linia logu z wynikiem - "cpu_mhz=" to odczyt
// getCpuFrequencyMhz() PO zmianie, czyli prawda, nie zyczenie.
static void zasilanieZastosujCpu(const char *kiedy) {
  const uint32_t cel = ZASILANIE_CPU_MHZ[zasilanieCpuIdx];
  bool ok = true;
  if (getCpuFrequencyMhz() != cel) ok = setCpuFrequencyMhz(cel);
  Serial.printf("tag=ZASILANIE zdarzenie=cpu kiedy=%s cel_mhz=%lu cpu_mhz=%lu wynik=%d\n",
                kiedy, (unsigned long)cel, (unsigned long)getCpuFrequencyMhz(), (int)ok);
}
// Moc nadajnika: `kiedy` == nullptr to ciche powtorzenie po WiFi.begin()
// (bez logu - laczenie potrafi byc ponawiane co kilka sekund).
static void zasilanieZastosujTx(const char *kiedy) {
  zasilanieTxOstatniWynik = WiFi.setTxPower(ZASILANIE_TX[zasilanieTxIdx]);
  if (kiedy == nullptr) return;
  Serial.printf("tag=ZASILANIE zdarzenie=nadajnik kiedy=%s cel_dbm=%.1f tx_dbm=%.1f wynik=%d\n",
                kiedy, (double)((int)ZASILANIE_TX[zasilanieTxIdx] * 0.25f), (double)zasilanieTxDbm(), (int)zasilanieTxOstatniWynik);
}
// Callback z ekranu ZASILANIE (lvgl_dashboard_poc_ustaw_callback_zasilanie()).
void zasilanieWyslij(const char *pole, int indeks) {
  if (strcmp(pole, "cpu") == 0) {
    if (indeks < 0 || indeks >= ZASILANIE_CPU_N) return;
    zasilanieCpuIdx = (uint8_t)indeks;
    ustawieniaWyswietlacza.putUChar("cpuIdx", zasilanieCpuIdx);
    zasilanieZastosujCpu("ekran");
  } else if (strcmp(pole, "nadajnik") == 0) {
    if (indeks < 0 || indeks >= ZASILANIE_TX_N) return;
    zasilanieTxIdx = (uint8_t)indeks;
    ustawieniaWyswietlacza.putUChar("txIdx", zasilanieTxIdx);
    zasilanieZastosujTx("ekran");
  }
  zasilaniePomiarNaEkran();
}
// Zywy odczyt dla ekranu ZASILANIE - co status (2 s) i po kazdej zmianie.
void zasilaniePomiarNaEkran() {
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  lvgl_dashboard_poc_aktualizuj_zasilanie_pomiar(wifiOk, wifiOk ? (int)WiFi.RSSI() : 0,
                                                  (int)getCpuFrequencyMhz(), zasilanieTxDbm());
}

void wyswietlaczWczytajZPamieci() {
  // [ROZNICA wzgledem kalibracji] `kalibracja` (wyzej) robi .end() zaraz po
  // jednorazowym uzyciu w setup() - my celowo NIE wolamy .end() tutaj, bo w
  // odroznieniu od kalibracji dotyku (ktora pisze tylko RAZ, w trakcie
  // interaktywnej kalibracji) ten obiekt musi zostac otwarty przez CALY czas
  // dzialania programu - wyswietlaczJasnoscWyslij()/..._ustawieniaWyslij()/
  // ..._harmonogramWyslij() moga dopisac do niego w KAZDEJ chwili, gdy user
  // dotknie panelu WYSWIETLACZ (minuty czy godziny po starcie).
  ustawieniaWyswietlacza.begin("wyswietlacz", false);
  jasnoscDziennaWartosc = ustawieniaWyswietlacza.getUChar("jasnDzien", 100);
  jasnoscNocnaWartosc   = ustawieniaWyswietlacza.getUChar("jasnNoc", 30);
  godzinaStartPrzyciemnienia  = ustawieniaWyswietlacza.getUChar("godzStart", 22);
  godzinaKoniecPrzyciemnienia = ustawieniaWyswietlacza.getUChar("godzKoniec", 6);
  harmonogramWlaczonyWartosc  = ustawieniaWyswietlacza.getBool("harmonWlacz", true);
  // [v1.4.7 ZASILANIE] Ta sama przestrzen NVS - klucze "cpuIdx"/"txIdx" to
  // INDEKSY w ZASILANIE_CPU_MHZ[]/ZASILANIE_TX[] (nizej), nie wartosci: gdyby
  // tabela sie kiedys zmienila, nieznany indeks spada na 0 (= jak do v1.4.6),
  // a nie na jakas przypadkowa czestotliwosc.
  zasilanieCpuIdx = ustawieniaWyswietlacza.getUChar("cpuIdx", 0);
  zasilanieTxIdx  = ustawieniaWyswietlacza.getUChar("txIdx", 0);
  if (zasilanieCpuIdx >= ZASILANIE_CPU_N) zasilanieCpuIdx = 0;
  if (zasilanieTxIdx  >= ZASILANIE_TX_N)  zasilanieTxIdx  = 0;
  // [v1.5.1 SEN] Tak samo indeksy, nie czasy; nieznany indeks = domyslny (1 min / 5 min).
  senPrzyciemnijIdx = diag::senIndeksPoprawny(ustawieniaWyswietlacza.getUChar("senPrzyIdx", diag::SEN_PRZYCIEMNIJ_DOMYSLNY_IDX), diag::SEN_PRZYCIEMNIJ_DOMYSLNY_IDX);
  senUspijIdx       = diag::senIndeksPoprawny(ustawieniaWyswietlacza.getUChar("senSpijIdx", diag::SEN_USPIJ_DOMYSLNY_IDX), diag::SEN_USPIJ_DOMYSLNY_IDX);
  // Bezpieczne klipsy na wypadek uszkodzonych/przedawnionych danych w NVS -
  // te same zakresy co suwaki/spinboxy w dashboard_lvgl_poc.cpp.
  // [v1.4.1 C9] Dolna granica JASNOSC_MIN_PROCENT: wartosc 0 % zapisana przez
  // starszy program (do v1.4.0 suwak pozwalal na 0) po tej zmianie daje 5 %.
  jasnoscDziennaWartosc = constrain(jasnoscDziennaWartosc, JASNOSC_MIN_PROCENT, (uint8_t)100);
  jasnoscNocnaWartosc   = constrain(jasnoscNocnaWartosc,   JASNOSC_MIN_PROCENT, (uint8_t)100);
  godzinaStartPrzyciemnienia  = constrain(godzinaStartPrzyciemnienia,  (uint8_t)0, (uint8_t)23);
  godzinaKoniecPrzyciemnienia = constrain(godzinaKoniecPrzyciemnienia, (uint8_t)0, (uint8_t)23);
}

// Czy podana godzina (0-23) miesci sie w oknie od start do koniec, koniec
// WYLACZNIE (standardowy polprzedzial czasu). Okno MOZE "zawijac sie" przez
// polnoc (start > koniec, np. 22..6, najbardziej typowy przypadek "wieczor do
// rana") - wtedy godzina jest W OKNIE gdy jest >=start LUB <koniec (suma, nie
// przeciecie przedzialow). Gdy start==koniec, okno obejmuje CALA dobe (24/24)
// - najprostsza, przewidywalna konwencja dla tego brzegowego przypadku.
bool wyswietlaczCzyPrzyciemniona(int godzina) {
  int s = godzinaStartPrzyciemnienia, k = godzinaKoniecPrzyciemnienia;
  if (s == k) return true;
  if (s < k) return (godzina >= s) && (godzina < k);
  return (godzina >= s) || (godzina < k);
}

// Wolane po kazdej swiezej wartosci godziny (patrz wywolania w
// pobierzStatus(), oraz z "mostow" ponizej po kazdej
// recznej zmianie na panelu). Idempotentne i tanie (ledcWrite tylko przy
// realnej ZMIANIE stanu dzien/noc, patrz wyswietlaczPrzyciemnionyTeraz), wiec
// bezpieczne do wolania co kilka sekund. Godzina nieznana
// (!wyswietlaczGodzinaZnana, np. tuz po starcie zanim przyjdzie 1. odpowiedz
// centrali) -> nie zgadujemy, zostaw jasnosc jak jest.
void wyswietlaczHarmonogramOdswiez() {
  if (!harmonogramWlaczonyWartosc || !wyswietlaczGodzinaZnana) {
    if (wyswietlaczPrzyciemnionyTeraz) { // wracamy z ew. przyciemnienia (np. user wlasnie wylaczyl harmonogram)
      wyswietlaczPrzyciemnionyTeraz = false;
      ustawJasnoscPodswietlenia(jasnoscDziennaWartosc);
    }
    return;
  }
  bool przyciemniona = wyswietlaczCzyPrzyciemniona(wyswietlaczOstatniaGodzina);
  if (przyciemniona != wyswietlaczPrzyciemnionyTeraz) {
    wyswietlaczPrzyciemnionyTeraz = przyciemniona;
    ustawJasnoscPodswietlenia(przyciemniona ? jasnoscNocnaWartosc : jasnoscDziennaWartosc);
  }
}

// [LVGL-API] Lokalny "most" dla panelu WYSWIETLACZ - w odroznieniu od
// WSZYSTKICH innych mostow w tym pliku (pompaManualWyslij() i dalej) NIE ma
// tu HTTP: zmiana trafia prosto do NVS + na sprzet (PWM), efekt jest wiec
// natychmiastowy i zawsze "prawdziwy" (nic nie moze "nie dojsc" jak przy
// POST do centrali, wiec nie ma czego czekac/potwierdzac kolejnym
// pobierzStatus()). Te same 3 funkcje sa rejestrowane w OBU galeziach
// setup() (symulacja i prawdziwa centrala) - w odroznieniu od reszty
// callbackow w tym pliku, tu nie ma czego symulowac osobno, bo podswietlenie
// tej plytki dziala identycznie niezaleznie od tego, czy centrala jest
// akurat podlaczona.
//
// `id`: 0=jasnoscDzien, 1=jasnoscNoc (ten sam ksztalt co serwoPozycjaWyslij
// dla klapa/syberek).
void wyswietlaczJasnoscWyslij(int id, int procent, bool koncowa) {
  procent = constrain(procent, (int)JASNOSC_MIN_PROCENT, 100);   // [v1.4.1 C9] dolna granica, patrz JASNOSC_MIN_PROCENT
  if (id == 0) jasnoscDziennaWartosc = (uint8_t)procent;
  else         jasnoscNocnaWartosc   = (uint8_t)procent;
  // [FIX, ta sesja - "jasnosc widoczna dopiero po puszczeniu"] PWM ponizej
  // wykonuje sie przy KAZDYM wywolaniu (rowniez w trakcie przeciagania) -
  // tanie. Zapis do NVS (patrz `if (koncowa)` ponizej) TYLKO na koncu gestu -
  // flash ma ograniczona liczbe cykli kasowania sektora.
  //
  // Zastosuj NATYCHMIAST tylko jesli edytowany poziom jest AKURAT aktywny -
  // inaczej user zobaczylby chwilowy, mylacy "przeskok" jasnosci przy edycji
  // poziomu, ktory teraz i tak nie jest uzywany (np. ustawia jasnosc nocna w
  // srodku slonecznego dnia).
  bool celujeWAktywny = (id == 0) ? !wyswietlaczPrzyciemnionyTeraz : wyswietlaczPrzyciemnionyTeraz;
  if (celujeWAktywny) ustawJasnoscPodswietlenia((uint8_t)procent);
  if (koncowa) {
    ustawieniaWyswietlacza.putUChar(id == 0 ? "jasnDzien" : "jasnNoc", (uint8_t)procent);
  }
}

void wyswietlaczHarmonogramWyslij(int wlaczony) {
  harmonogramWlaczonyWartosc = (wlaczony != 0);
  ustawieniaWyswietlacza.putBool("harmonWlacz", harmonogramWlaczonyWartosc);
  wyswietlaczHarmonogramOdswiez(); // przelicz i zastosuj od razu wzgledem ostatniej znanej godziny
}

void wyswietlaczUstawieniaWyslij(const char *pole, long wartosc) {
  uint8_t godz = (uint8_t)constrain(wartosc, 0L, 23L);
  if (strcmp(pole, "godzStart") == 0) {
    godzinaStartPrzyciemnienia = godz;
    ustawieniaWyswietlacza.putUChar("godzStart", godz);
  } else if (strcmp(pole, "godzKoniec") == 0) {
    godzinaKoniecPrzyciemnienia = godz;
    ustawieniaWyswietlacza.putUChar("godzKoniec", godz);
  } else if (strcmp(pole, "senPrzyciemnij") == 0 || strcmp(pole, "senUspij") == 0) {
    // [v1.5.1 SEN] Wiersz "Na baterii" - indeks w diag::SEN_*_OPCJE_MS[].
    if (pole[3] == 'P') {
      senPrzyciemnijIdx = diag::senIndeksPoprawny(wartosc, diag::SEN_PRZYCIEMNIJ_DOMYSLNY_IDX);
      ustawieniaWyswietlacza.putUChar("senPrzyIdx", senPrzyciemnijIdx);
    } else {
      senUspijIdx = diag::senIndeksPoprawny(wartosc, diag::SEN_USPIJ_DOMYSLNY_IDX);
      ustawieniaWyswietlacza.putUChar("senSpijIdx", senUspijIdx);
    }
    Serial.printf("tag=SEN zdarzenie=ustawienie przyciemnij_ms=%lu uspij_ms=%lu\n",
                  (unsigned long)diag::senPrzyciemnijMs(senPrzyciemnijIdx), (unsigned long)diag::senUspijMs(senUspijIdx));
    return;   // harmonogramu dzien/noc to nie dotyczy
  }
  wyswietlaczHarmonogramOdswiez(); // granice okna sie zmienily - przelicz i zastosuj od razu
}

// ----------------------------------------------------------------------------
// [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Piezo (najlepiej PASYWNY -
// patrz "UWAGA SPRZETOWA" nizej) na pinie 13, sterowany PWM tak samo jak
// podswietlenie (ledcAttach/ledcWriteTone), tylko ton zamiast jasnosci. Gra
// dwa RZECZYWISCIE UZYWANE, rozpoznawalne wzorce alarmowe zamiast jednego
// monotonnego piknia - research (na prosbe z tej sesji) ponizej.
//
// UWAGA SPRZETOWA: kod zaklada brzeczyk PASYWNY (bez wlasnego oscylatora -
// gra dokladnie te czestotliwosc, ktora dostanie z ledcWriteTone()). Z
// brzeczykiem AKTYWNYM (ma wlasny oscylator, ignoruje czestotliwosc, gra
// zawsze swoim jednym stalym tonem) kod i tak zadziala - RYTM obu wzorcow
// (patrz nizej) zostaje w pelni slyszalny, ginie tylko roznica WYSOKOSCI
// dzwieku miedzy POZAREM a OGRZEWANIEM. Pasywny da pelny efekt.
//
// RESEARCH - skad te konkretne wzorce (nie wymyslone, dwa prawdziwe,
// standaryzowane sygnaly alarmowe o swiadomie innym "charakterze"):
// 1. POZAR/DYM = "Temporal-Three" (T3) - miedzynarodowy standard alarmow
//    pozarowych (NFPA 72 / ISO 8201 / ANSI S3.41): 3x 0.5s ton + 0.5s cisza,
//    potem 1.5s ciszy, cykl 4.0s. To DOKLADNIE ten rytm z prawdziwych czujek
//    dymu - rozpoznawalny instynktownie, bez zastanowienia. Oficjalna norma
//    ustala tez czestotliwosc na 520Hz (nizsze tony skuteczniej budza ze
//    snu), ale to zalozenie dla dedykowanych syren - maly, tani brzeczyk
//    piezo (jak tutaj) jest znacznie glosniejszy w swoim naturalnym pasmie
//    rezonansu ok. 3-4kHz, wiec zostaje sam RYTM T3 (najbardziej
//    rozpoznawalna czesc wzorca), a czestotliwosc jest dobrana praktycznie
//    pod ten konkretny, tani podzespol.
// 2. OGRZEWANIE/PANEL = dwuton "hi-lo" (europejska syrena typu "nee-naw") -
//    ciagle, plynne przelaczanie miedzy dwiema stalymi czestotliwosciami co
//    ok. 0.5s, BEZ przerw w dzwieku. Celowo skrajnie INNY charakter niz
//    pozar (ciagly zamiast urywanego, nizszy zakres tonow) - zeby od razu,
//    bez patrzenia na ekran, slyszec "to NIE pozar, ale cos wymaga
//    sprawdzenia".
//
// Priorytet gdy kilka alarmow naraz: POZAR (dymAlarm) > OGRZEWANIE/PANEL >
// cisza - patrz brzeczykOdswiezZAlarmow() nizej.
#define BRZECZYK_PIN 13

struct BrzeczykFaza { uint16_t czestotliwoscHz; uint16_t czasMs; }; // czestotliwoscHz=0 -> cisza w tej fazie

static const BrzeczykFaza WZORZEC_POZAR[] = {
  {3000, 500}, {0, 500}, {3000, 500}, {0, 500}, {3000, 500}, {0, 1500},
};
static const BrzeczykFaza WZORZEC_OGRZEWANIE[] = {
  {1200, 500}, {1600, 500},
};

enum BrzeczykAlarmTyp { BRZECZYK_BRAK, BRZECZYK_POZAR, BRZECZYK_OGRZEWANIE };
BrzeczykAlarmTyp brzeczykAktywnyTyp = BRZECZYK_BRAK;

// [EKRAN ALARMOWY, ta sesja] Wyciszenie z przycisku WYCISZ (dashboard_lvgl_poc.cpp) -
// patrz alarmWyciszObsluga()/brzeczykOdswiezZAlarmow() nizej i
// PLAN_EKRAN_ALARMOWY.md. Powiazane z KONKRETNYM typem, nie globalnym
// przelacznikiem - g_brzeczykWyciszonyTyp pamieta, KTORY typ user wyciszyl,
// zeby wyciszenie samo przestalo obowiazywac, gdy chciany typ sie zmieni
// (alarm ustapi albo eskaluje na inny/gorszy).
static bool g_brzeczykWyciszony = false;
static BrzeczykAlarmTyp g_brzeczykWyciszonyTyp = BRZECZYK_BRAK;
int  brzeczykFazaIdx = 0;
unsigned long brzeczykFazaStartMs = 0;

const BrzeczykFaza *brzeczykWzorzec(BrzeczykAlarmTyp typ, int &liczbaFaz) {
  if (typ == BRZECZYK_POZAR) {
    liczbaFaz = sizeof(WZORZEC_POZAR) / sizeof(WZORZEC_POZAR[0]);
    return WZORZEC_POZAR;
  }
  liczbaFaz = sizeof(WZORZEC_OGRZEWANIE) / sizeof(WZORZEC_OGRZEWANIE[0]);
  return WZORZEC_OGRZEWANIE;
}

void brzeczykZastosujFaze() {
  int liczbaFaz;
  const BrzeczykFaza *wzorzec = brzeczykWzorzec(brzeczykAktywnyTyp, liczbaFaz);
  ledcWriteTone(BRZECZYK_PIN, wzorzec[brzeczykFazaIdx].czestotliwoscHz);
}

void brzeczykUstawTyp(BrzeczykAlarmTyp nowy) {
  if (nowy == brzeczykAktywnyTyp) return; // bez zmian - nie przerywaj w polowie juz granej frazy
  brzeczykAktywnyTyp = nowy;
  brzeczykFazaIdx = 0;
  brzeczykFazaStartMs = millis();
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) ledcWriteTone(BRZECZYK_PIN, 0);
  else brzeczykZastosujFaze(); // wejdz w faze 0 NATYCHMIAST, nie czekaj na 1. obrot obsluzBrzeczyk()
}

// Wolane po kazdej swiezej wartosci alarmow (patrz wywolania w
// pobierzStatus()) - decyduje KTORY wzorzec (jesli
// ktorykolwiek) powinien teraz grac. Idempotentne - brzeczykUstawTyp() sam
// pilnuje, zeby nie przerywac juz granego wzorca bez potrzeby.
void brzeczykOdswiezZAlarmow(bool dym, bool ogrzewanie, bool panel) {
  BrzeczykAlarmTyp chciany = dym ? BRZECZYK_POZAR
                            : (ogrzewanie || panel) ? BRZECZYK_OGRZEWANIE
                            : BRZECZYK_BRAK;
  // [EKRAN ALARMOWY, ta sesja] Jesli user wyciszyl DOKLADNIE ten typ, ktory
  // wlasnie chcemy zagrac - podstaw cisze. W chwili, gdy `chciany` przestanie
  // byc rowne zapamietanemu g_brzeczykWyciszonyTyp (alarm ustapil albo
  // eskalowal na inny typ), ten warunek sam przestaje byc prawdziwy - zero
  // recznego zerowania flagi gdziekolwiek indziej. Patrz alarmWyciszObsluga().
  // [v1.2.1] Wyciszenie dotyczy TEGO wystapienia alarmu. Do v1.2.0 obie
  // zmienne nie byly nigdy zerowane, wiec raz wyciszony typ alarmu zostawal
  // niemy takze przy KAZDYM nastepnym wystapieniu - az do restartu panelu
  // (alarm ustapil, wrocil po godzinie: cisza). Od v1.2.1 stan alarmu
  // przechodzi tez przez samoczynny restart, wiec bez tej poprawki takie
  // wyciszenie nie konczyloby sie juz nigdy.
  if (g_brzeczykWyciszony && chciany != g_brzeczykWyciszonyTyp) {
    g_brzeczykWyciszony = false;
    g_brzeczykWyciszonyTyp = BRZECZYK_BRAK;
  }
  if (g_brzeczykWyciszony && chciany == g_brzeczykWyciszonyTyp) {
    chciany = BRZECZYK_BRAK;
  }
  brzeczykUstawTyp(chciany);
}

// [EKRAN ALARMOWY, ta sesja] Zarejestrowana jako AlarmWyciszCb (patrz setup())
// - wolana z przycisku WYCISZ na nowym
// ekranie alarmowym w dashboard_lvgl_poc.cpp. W 100% lokalna: brzeczyk to
// GPIO na TEJ plytce, wiec zero HTTP do centrali - w odroznieniu od
// wiekszosci pozostalych callbackow w tym pliku.
// [v1.2.1] Cisza zapada OD RAZU. Do v1.2.0 ta funkcja tylko ustawiala dwie
// zmienne, a brzeczyk milkl dopiero w brzeczykOdswiezZAlarmow(), czyli po
// nastepnym poprawnym statusie z centrali. Skutki: (1) bez danych z centrali
// WYCISZ nie dzialal wcale - a wlasnie wtedy alarm zostaje "zamrozony" i gra
// dalej, takze po samoczynnym restarcie (alarmPrzywrocPoAutorestarcie());
// (2) drugie nacisniecie WYCISZ, gdy juz bylo cicho, zapamietywalo "wyciszony
// typ: zaden" i przy kolejnym statusie alarm zaczynal grac od nowa.
void alarmWyciszObsluga() {
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) return;   // nic nie gra - poprzednie wyciszenie zostaje bez zmian
  g_brzeczykWyciszony = true;
  g_brzeczykWyciszonyTyp = brzeczykAktywnyTyp;
  brzeczykUstawTyp(BRZECZYK_BRAK);
}

// Wolane co obrot loop() - awansuje
// wzorzec do kolejnej fazy, gdy czas biezacej fazy uplynal. ledcWriteTone()
// wolane TYLKO przy realnej zmianie fazy, NIE w kazdym obrocie petli (patrz
// arduino-esp32 issue #3617 - zbyt czeste wywolania ledcWriteTone() daja
// niedokladny dzwiek).
void obsluzBrzeczyk() {
  if (brzeczykAktywnyTyp == BRZECZYK_BRAK) return;
  int liczbaFaz;
  const BrzeczykFaza *wzorzec = brzeczykWzorzec(brzeczykAktywnyTyp, liczbaFaz);
  if (millis() - brzeczykFazaStartMs >= wzorzec[brzeczykFazaIdx].czasMs) {
    brzeczykFazaIdx = (brzeczykFazaIdx + 1) % liczbaFaz;
    brzeczykFazaStartMs = millis();
    brzeczykZastosujFaze();
  }
}

void brzeczykZainicjuj() {
  ledcAttach(BRZECZYK_PIN, 3000, 8); // czestotliwosc startowa/rozdzielczosc - ledcWriteTone() i tak zmienia freq w locie
  ledcWriteTone(BRZECZYK_PIN, 0);    // cisza na starcie
}

// Motyw kolorow — spojny z panelem WWW centrali (ciemne tlo, akcent ognisty)
#define KOL_TLO       0x0801   // ciemny granat/czern
#define KOL_KARTA     0x10A3
#define KOL_RAMKA     0x2A8B
#define KOL_TEKST     0xC638
#define KOL_TEKST_DIM 0x4A69
#define KOL_OGIEN     0x07FF   // cyan (jak --flame w panelu WWW)
#define KOL_OK        0x4726   // zielony
#define KOL_WARN      0xFDA0   // bursztyn
#define KOL_ERR       0xF8B2   // czerwony
#define KOL_SNIEG     0xAEDC   // jasny blekit — ikona sniezynki (zima)
#define KOL_KROPLA    0x4C9F   // niebieski — ikona kropli (pompa)
#define KOL_SUWAK     0xFD20   // pomaranczowy — ikony suwakow (klapa/syberka), jak w makiecie referencyjnej
#define KOL_ZAR       0xFA80   // czerwono-pomaranczowy — ikony plomienia/termometru (bojler, ogrzewanie, panel)
#define KOL_CHMURKA   0x2E7F   // turkusowy — ikona chmury/sniegu (zewn)

// ----------------------------------------------------------------------------
// STAN POBIERANY Z CENTRALI
// ----------------------------------------------------------------------------
struct StanPieca {
  bool  polaczono = false;
  String ip;
  int   rssi = 0;
  float tZewn = 0, tBojler = 0, tOgrz = 0, tOgrzSr = 0, tPanel = 0, tPokoj = 0, tTrociny = 0, tPowrot = 0;
  float wilgotnosc = 0, cisnienie = 0;
  int   dym = 0;
  bool  dymAlarm = false;
  int   klapa = 0, syberka = 0;
  bool  pompa = false;
  // [LVGL-API] Minuty do cichego powrotu pompy do AUTO (0 = brak override,
  // automatyka centrali decyduje) - centrala liczy to sama (pompaManualOverrideUntil),
  // LCD tylko odczytuje, zeby nie miec wlasnego, rozjezdzajacego sie timera.
  int   pompaOverrideMin = 0;
  bool  alarmOgrzewanie = false, alarmPanel = false;
  // [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Z centrali (/api/status pola
  // deszcz_stan/deszcz_min, v2.13.2) - 0=brak opadu w widocznym oknie,
  // 1=nadciaga (deszczMinuty=za ile zacznie), 2=pada teraz (deszczMinuty=za
  // ile skonczy, -1=nieznane w oknie 6h). Domyslne 0/-1 dopasowane do tego,
  // co zwraca centrala, gdy jeszcze nie zdazyla policzyc pierwszej prognozy
  // (identyczny duch co reszta domyslnych wartosci w tej strukturze).
  int   deszczStan = 0, deszczMinuty = -1;
  // [v1.5.14 POGODA-ZEWN] Z centrali 3.58.16: kod WMO pogody teraz, kod WMO
  // w slocie, w ktorym zaczyna sie opad z deszczStan (zeby napisac "śnieg za
  // 2 h" a nie "deszcz"), dzien/noc (1/0) i porywy wiatru w km/h. -1 = brak
  // (starsza centrala, brak internetu) -> kafelek ZEWN jak dotad.
  int   pogodaKod = -1, pogodaKodOpad = -1, pogodaDzien = -1;
  float wiatrPorywy = -1.0f;
  float pogodaTemp = -99.0f;   // [v1.5.17] pogoda_temp z centrali 3.58.17 (°C prognozy), -99 = brak
  // [LCD-SEZON] Z centrali (RTC sprzetowy tam, nie tutaj — wyswietlacz nie
  // ma wlasnego RTC z podtrzymaniem, dziala na baterii/deep sleep) — patrz
  // aktualizujSezon() nizej.
  int   miesiac = 0;      // 1-12, albo 0 gdy centrala zglasza brak RTC
  bool  rtcOk = false;
  // [LVGL-DATA-CZAS, ta sesja] Kafelek DATA I CZAS - godzina/minuta/dzien/rok,
  // ten sam zrodlo (RTC centrali) i ten sam fallback (0 gdy !rtcOk) co miesiac.
  int   godzina = 0, minuta = 0, dzien = 0, rok = 0;
  // [UI-INTERNET, ta sesja] Dostepnosc internetu (fbAppReady na centrali) -
  // odrozniona od `polaczono` (to ostatnie oznacza tylko "LCD ma kontakt z
  // centrala przez lokalny WiFi", nie mowi nic o tym, czy sama centrala ma
  // internet). Ten sam wzorzec co rssi: centrala liczy/mierzy, LCD tylko
  // odczytuje z /api/status.
  bool  internet = false;
  // [OTA-PANEL, ta sesja] Oferta aktualizacji z centrali (/api/status) — patrz
  // otaKrok() nizej i TG_UPDATE_PANEL w main_centrala.cpp.
  // [v1.4.0 OTA] Stale tablice zamiast String (bez przydzialu pamieci co 2 s)
  // i dwa nowe pola od centrali 3.58.6.
  bool     panelOtaDostepna = false;
  char     panelOtaWersja[32] = "";   // "panel_ota_wersja": tag wydania, z ktorego jest plik
  char     panelOtaObraz[17] = "";    // "panel_ota_obraz": identyfikator pliku; "" = centrala do 3.58.5 (nie podaje)
  uint32_t panelOtaNr = 0;            // "panel_ota_nr": numer oferty (centrala 3.58.6+)
  // [v1.4.6 WERSJE] Wersja programu centrali z pola "fw" statusu
  // ("v3.58.6"); "" = centrala jeszcze nie odpowiedziala albo pola nie ma.
  // Pokazywana na ekranie startowym i na ekranie WYSWIETLACZ, w logu
  // "tag=PANEL" jako centrala_fw=.
  char     centralaFw[16] = "";
};
StanPieca stan;

// [OTA-PANEL, ta sesja] Blokuje wejscie w deep sleep na czas pobierania/
// flashowania panelu (prompt #8 — patrz idzSpac() nizej). Zadeklarowane TUTAJ
// (a nie przy funkcjach OTA, blisko setup()) bo idzSpac() jest zdefiniowane
// wczesniej w pliku i musi juz ja widziec.
// [v1.4.0 OTA] Ustawiana przez otaZacznij(), zdejmowana przez
// otaSprzatnijPoProbie(). Wstrzymuje tez panelRatunekKrok().
bool trwaAktualizacjaOtaPanelu = false;

// [v1.4.0 OTA] Stan aktualizacji przez centrale. Zmienne sa TUTAJ (a nie przy
// funkcjach OTA blizej setup()), bo pobierzStatus() i odczyt odpowiedzi
// centrali, zdefiniowane wczesniej w pliku, juz z nich korzystaja.
static char otaObrazWlasny[17] = "";        // identyfikator programu, z ktorego panel pracuje ("" = nie dalo sie odczytac)
static bool otaObrazZatwierdzony = true;    // false = pierwszy start po wgraniu przez OTA, partycja jeszcze nie zatwierdzona
static diag::OtaZapis otaZapis = {};        // co panel zrobil z ostatnia oferta (kopia zapisu z NVS)
// To, co panel dokleja do adresu zapytan /api/status (pobierzStatus()):
static bool     otaZglosOdmowe = false;     // &panel_ota_odmowa=<nr>-<powod>
static uint32_t otaOdmowaNr = 0;
static uint8_t  otaOdmowaPowod = diag::OTA_POWOD_BRAK;
static char     otaCzekaPowod[16] = "";     // &panel_ota_czeka=<powod>; "" = bez tego parametru
// Trwajace pobieranie.
struct OtaPobieranie {
  bool          trwa;
  diag::OtaSkan skan;               // pierwsze 24 i ostatnie 32 bajty pliku
  uint8_t       blad;               // diag::OtaPowod; OTA_POWOD_BRAK = dotad bez bledu
  bool          ostateczny;         // blad, przy ktorym ponawianie nie ma sensu
  const char   *opisBledu;          // do logu
  bool          zapisZaczety;       // Update.begin() sie udal
  uint32_t      zapisano;           // bajty oddane do Update.write()
  uint32_t      rozmiar;            // rozmiar pliku zapowiedziany przez centrale (Content-Length)
  char          klucz[diag::OTA_KLUCZ_MAKS];   // ktora oferte panel pobiera
  char          obrazOferty[17];    // identyfikator pliku z oferty ("" = starsza centrala go nie podaje)
  bool          nowaCentrala;
  int           procentNaEkranie;
};
static OtaPobieranie otaP = {};
static bool otaBylaProba = false;           // od startu panelu byla juz proba pobierania...
static unsigned long otaKoniecProbyMs = 0;  // ...i wtedy sie skonczyla (albo zostala przerwana)

// [RESET-DIAG, ta sesja] Wypelniane RAZ, na samym poczatku setup() —
// patrz ponizej. Wysylane centrali w kazdym /api/status (analogicznie do
// FIRMWARE_VERSION_PANEL), zeby centrala mogla odroznic normalne
// wybudzenie z deep sleep od czegos, co wymaga uwagi.
String globalnyPowodResetu;

// Podzbior /api/ustawienia-get, ktory ten panel pozwala PODGLADAC i EDYTOWAC.
// Zeby dodac kolejne pole: dopisz wpis do tablicy POLA[] nizej — nic wiecej.
struct PoleUstawien {
  const char* klucz;     // dokladnie jak w JSON-ie i w ustawParametrPieca()
  const char* etykieta;  // nazwa wyswietlana na ekranie
  long   krok;           // wielkosc kroku +/-
  long   minZakres, maxZakres; // TYLKO informacyjnie/klips lokalny — centrala i tak waliduje
  long   wartosc;        // ostatnia znana wartosc (z GET /api/ustawienia-get)
};

PoleUstawien POLA[] = {
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] czasOn/czasOff
  // brakowalo tu mimo ze centrala je juz waliduje/zwraca od dawna
  // (ustawParametrPieca()/"/api/ustawienia-get") - ten sam powod co
  // odchylTemp/mnoznik/antystopDni nizej: ekran LVGL USTAWIENIA POMPY
  // (patrz lvgl_dashboard_poc_aktualizuj_ustawienia_pompy() nizej) potrzebuje
  // ich w POLA[], bo to jedyna sciezka odczytu z centrali.
  { "czasOn",         "Pompa ON [min]",       1,   1, 180, 0 },
  { "czasOff",        "Pompa OFF [min]",      1,   1, 180, 0 },
  { "tempOn",         "Zal. pompe pow. [C]",  1,   0, 100, 0 },
  { "tempOff",        "Wyl. pompe pon. [C]",  1,   0, 100, 0 },
  { "tempZadServo",   "Temp. zadana serwo",   1,   0, 100, 0 },
  { "histerServo",    "Histereza serwo",      1,   0,  50, 0 },
  { "skokKlapy",      "Skok klapy [%]",       1,   1, 100, 0 },
  { "skokSyberka",    "Skok syberka [%]",     1,   1, 100, 0 },
  // [ROZSZERZENIE, ta sesja] Brakowalo tych dwoch mimo ze centrala je juz
  // waliduje/zwraca od dawna (ustawParametrPieca()/"/api/ustawienia-get") -
  // ekran LVGL USTAWIENIA SERWA (patrz lvgl_dashboard_poc_aktualizuj_ustawienia_serwa()
  // nizej) potrzebuje ich w POLA[], bo to jedyna sciezka odczytu z centrali.
  { "odchylTemp",     "Prog odchylenia [C]",  1,   1,  50, 0 },
  { "mnoznik",        "Mnoznik korekty",      1,   1,  10, 0 },
  { "antystopDni",    "Antystop co [dni]",    1,   1,  45, 0 },
  { "mieszadloCzasOn","Mieszadlo ON [s]",     5,   1, 255, 0 },
  { "mieszadloCzasOff","Mieszadlo OFF [s]",   5,   1, 180, 0 },
  { "progAlarmTemp",  "Prog alarmu temp.",    1,   0, 100, 0 },
  { "progAlarmDym",   "Prog alarmu dymu",    10,   0,4095, 0 },
  // [ROZSZERZENIE, ta sesja - PANEL DYM] Ekran LVGL DYM (patrz
  // lvgl_dashboard_poc_aktualizuj_ustawienia_dymu() nizej) potrzebuje ich w
  // POLA[], ten sam powod co reszta pol dopisanych w poprzednich sesjach -
  // centrala je juz waliduje/zwraca od dawna (main_centrala.cpp,
  // ustawParametrPieca()), tylko nie byly dotad tu wystawione.
  { "dymCzasOn",           "Czas WL. dymu [s]",     5, 20, 600, 0 },
  { "dymCzasOff",          "Czas WYL. dymu [s]",    5, 10, 600, 0 },
  { "dymCzasStabilizacji", "Stabilizacja dymu [s]", 5,  5, 590, 0 },
};
const int LICZBA_POL = sizeof(POLA) / sizeof(POLA[0]);
bool ustawieniaWczytane = false;

// [LCD-DYM-PANEL] Uzywane przez panel czulosci dymu (rysujPanelDym() nizej)
// zeby nie trzymac osobnego, twardo zakodowanego indeksu do POLA[] — jesli
// ktos przestawi kolejnosc pol w POLA[], to i tak dalej dziala.
int znajdzIndeksPola(const char* klucz) {
  for (int i = 0; i < LICZBA_POL; i++) if (strcmp(POLA[i].klucz, klucz) == 0) return i;
  return -1;
}

// [v1.4.1 C5] Wartosc pola po nazwie - do przekazywania ustawien ekranom LVGL.
// Dawniej: POLA[znajdzIndeksPola("nazwa")].wartosc, czyli przy literowce w
// nazwie (indeks -1) czytanie pamieci sprzed tabeli, bez zadnego sladu w logu.
// Teraz nieznana nazwa daje 0 i linie bledu (najwyzej raz na 60 s, zeby nie
// zalac logu przy odswiezaniu ustawien co kilka sekund).
long polaWartosc(const char* klucz) {
  const int i = znajdzIndeksPola(klucz);
  if (i >= 0) return POLA[i].wartosc;
  static unsigned long ostatnioMs = 0;
  if (ostatnioMs == 0 || millis() - ostatnioMs >= 60000UL) {
    ostatnioMs = millis() ? millis() : 1;
    Serial.printf("tag=USTAWIENIA lvl=ERR zdarzenie=nieznane_pole pole=%s\n", klucz);
  }
  return 0;
}

// [LCD-TRYBY] "wybor" i "trybSerwa" to enumy (nie liczby +/-), wiec zyja
// OSOBNO od POLA[] — ten sam JSON /api/ustawienia-get, ten sam generyczny
// /api/ustawienia-set (pole=wybor&wartosc=..), ale inny widget na ekranie
// (segmentowany przelacznik na ekranie LVGL, dashboard_lvgl_poc.cpp).
// Wartosci i zakresy 1:1 z main.cpp centrali (ustawParametrPieca()):
//   wybor:     1=Trociniak  2=Kopciuch  3=Automatyczny
//   trybSerwa: 1=Auto       2=Reczny    3=Bezpieczna
long trybWyboru  = 1;
long trybSerwaWartosc = 1;
// [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] `antystopWlaczony`
// to bool (glowny wylacznik funkcji antystop pompy - patrz PompaAntystopCb w
// dashboard_lvgl_poc.h), nie liczba +/-, wiec ten sam powod jak przy
// wybor/trybSerwa powyzej: zyje OSOBNO od POLA[]. Domyslnie true - ten sam
// domyslny stan co centrala uzywa przy pierwszym uruchomieniu
// (main_centrala.cpp, antystopWlaczony=true).
bool antystopWlaczonyWartosc = true;

// ----------------------------------------------------------------------------
// [LCD-SEZON] Automatyczne przelaczanie widoku Lato/Zima (dashboard chowa
// wtedy kafelki niepotrzebne sezonowo — lvgl_dashboard_poc_aktualizuj()
// dostaje aktualnySezon). Zrodlo prawdy: "miesiac" z /api/status (RTC centrali,
// patrz [LCD-SEZON] w main.cpp centrali, v1.16.0), z fallbackiem na
// histereze t_panel gdy centrala zglasza brak RTC (miesiac==0) — ORAZ jako
// naklada sie NA kalendarz w miesiacach sezonu grzewczego: goracy panel w
// dzien sloneczny (np. marzec) czasowo przelacza widok na Lato, bo wtedy to
// panel/bojler sa istotne, a nie piec — ustalone z uzytkownikiem wprost.
// ----------------------------------------------------------------------------
enum Sezon { SEZON_ZIMA, SEZON_LATO };
Sezon aktualnySezon = SEZON_ZIMA;
bool  sezonZainicjalizowany = false;

// Sezon grzewczy = wrzesien-maj (IX-V) — najbardziej typowy dla PL, ustalone
// z uzytkownikiem. Miesiace VI-VIII to zawsze Lato bez wzgledu na t_panel.
bool miesiacWSezonieGrzewczym(int m) { return (m >= 9 || (m >= 1 && m <= 5)); }

// Histereza na t_panel, zeby przelaczanie nie "migalo" przy wahaniach wokol
// progu (np. chmura na chwile zasloni slonce) — proste podejscie znane z
// termostatow: inny prog wejscia niz wyjscia.
const float SEZON_PANEL_PROG_LATO = 25.0f;  // powyzej -> (albo utrzymaj) Lato
const float SEZON_PANEL_PROG_ZIMA = 20.0f;  // ponizej -> (albo utrzymaj) Zima

// Wywolywane po kazdym pobierzStatus() (setup() i loop()). Zwraca true, gdy
// sezon sie realnie zmienil w tym wywolaniu — wywolujacy wtedy wymusza pelne
// przerysowanie dashboardu (uklad kafelkow sie zmienia).
bool aktualizujSezon() {
  Sezon poprzedni = aktualnySezon;

  if (stan.miesiac != 0 && !miesiacWSezonieGrzewczym(stan.miesiac)) {
    // Czerwiec-sierpien — kalendarz rozstrzyga jednoznacznie, zawsze Lato.
    aktualnySezon = SEZON_LATO;
  } else {
    // Miesiac sezonu grzewczego (domyslnie Zima, ale goracy panel czasowo
    // przelacza na Lato, np. sloneczny dzien w marcu) ALBO brak RTC w centrali
    // (miesiac == 0, jedyne zrodlo to t_panel) - w obu przypadkach ta sama
    // czysta histereza na t_panel. [v1.4.4 C7] Do v1.4.3 dwie identyczne
    // galezie; zachowanie bez zmian.
    if (!sezonZainicjalizowany) {
      aktualnySezon = (stan.tPanel > SEZON_PANEL_PROG_LATO) ? SEZON_LATO : SEZON_ZIMA;
    } else if (aktualnySezon == SEZON_ZIMA && stan.tPanel > SEZON_PANEL_PROG_LATO) {
      aktualnySezon = SEZON_LATO;
    } else if (aktualnySezon == SEZON_LATO && stan.tPanel < SEZON_PANEL_PROG_ZIMA) {
      aktualnySezon = SEZON_ZIMA;
    }
  }

  sezonZainicjalizowany = true;
  return aktualnySezon != poprzedni;
}

// ----------------------------------------------------------------------------
// HTTP — pomocnicze
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja] httpGet()/httpPost() (synchroniczne
// HTTPClient::GET/POST) USUNIETE z tego miejsca - zastapione przez
// statusFetch/ustawieniaFetch/komendaFetch (siec_nieblokujaca_LCD.h,
// instancje globalne wyzej). Odpowiedniki:
//   httpGet("/api/status?...")   -> statusFetch.startGet(...)     + obslugaStatusFetch() nizej
//   httpGet("/api/ustawienia-get")-> ustawieniaFetch.startGet(...) + obslugaUstawieniaFetch() nizej
//   httpPost(sciezka, body, blad) -> komendaFetch.startPostFormUrlEncoded(...) + obslugaKomendaFetch() nizej
// [v1.3.0 WYKRESY] Ekran Wykresy tez juz nie uzywa HTTPClient - patrz nizej.

// ----------------------------------------------------------------------------
// [v1.3.0 WYKRESY] Dane do ekranu Wykresy: od centrali, bez blokowania petli
// ----------------------------------------------------------------------------
// Panel nie ma internetu ani wlasnej historii - wszystko do wykresow daje mu
// centrala, ktora trzyma historie u siebie (pamiec + pliki /hist_short.csv i
// /hist_long.csv na jej flashu).
//
// Jak to idzie po dotknieciu "POKAZ WYKRES" (dashboard_lvgl_poc.cpp wola
// wykresyFetchStart() przez WykresyFetchCb):
//   1. GET /api/history/panel?tryb=godz|dzien|tydz&pkt=96 - centrala od wersji
//      3.58.4 sama wybiera zakres i odsyla gotowe punkty (ok. 20 kB, pierwsza
//      linia "#panel-hist ..."). Panel tylko rysuje.
//   2. Centrala starsza niz 3.58.4 tego adresu nie zna (odpowiada
//      przekierowaniem 302). Wtedy - i tylko wtedy - panel pobiera po staremu
//      caly CSV (/api/history albo /api/history/long, ponad 1 MB) i sam sciska
//      go do 96 punktow, jak w v1.2.2.
// W obu przypadkach odpowiedz jest czytana linia po linii przez wykresyFetch
// (StrumienHttpGet), w kolejnych obrotach loop() - ekran, dotyk i odpytywanie
// /api/status pracuja w tym czasie normalnie. Do v1.2.2 robil to blokujacy
// HTTPClient: ekran stal do konca pobierania (limit 8 s, po ktorym wykres
// tygodniowy byl po cichu rysowany z samego poczatku danych), a watchdog nie
// byl karmiony.
//
// Kazde pobranie zostawia w logu jedna linie "tag=WYKRESY" z wynikiem:
//   zdarzenie=pobrano       wykres narysowany (liczba punktow, bajtow, czas)
//   zdarzenie=pobrano_czesc wykres z czesci danych, na ekranie dopisek NIEPELNE
//                           (tylko starsza centrala: caly CSV nie zdazyl dojsc)
//   zdarzenie=brak_danych   centrala odpowiedziala, ale nie ma jeszcze historii
//   zdarzenie=blad          z przyczyna - ta sama przyczyna jest na ekranie
//                           zamiast dawnego ogolnego "Brak danych"
//   zdarzenie=przerwano     uzytkownik wyszedl z ekranu przed koncem
static const char* const WYKRESY_TRYB_ADRES[3] = { "godz", "dzien", "tydz" };   // indeks = LVGL_WYKRESY_TRYB_*

struct WykresyPobieranie {
  bool aktywne = false;
  bool nowySposob = true;        // true = pytamy o gotowe punkty; false = stary CSV (centrala < 3.58.4)
  uint8_t tryb = LVGL_WYKRESY_TRYB_DZIEN;
  int kodNowego = 0;             // kod HTTP odpowiedzi na /api/history/panel (do logu)
  unsigned long startMs = 0;
  unsigned long postepMs = 0;
  // [v1.5.6] Ponawianie, gdy centrala w ogole nie przyjela zapytania (patrz
  // wykresyMoznaPonowic()). Zgloszenie z plytki: "klikam wykres, pisze
  // 'Centrala nie przyjela polaczenia', dopiero za ktoryms razem sie
  // pojawia" - to te same czkawki AP co przy poleceniach (v1.5.5).
  uint8_t proba = 1;             // numer biezacej proby 1..WYKRESY_PROB_MAKS
  unsigned long ponowOdMs = 0;   // != 0: czekamy z kolejna proba do tej chwili millis()
  int ostatniWynik = 0;          // przyczyna poprzedniej proby (do logu ponowienia)
};
static WykresyPobieranie wykresyPob;
static const uint8_t  WYKRESY_PROB_MAKS = 3;
static const uint32_t WYKRESY_ODSTEP_PONOWIENIA_MS = 500;
static const char* wykresySciezka(bool nowySposob, bool dlugi, char* bufor, size_t n) {
  if (nowySposob) snprintf(bufor, n, "/api/history/panel?tryb=%s&pkt=%d", WYKRESY_TRYB_ADRES[wykresyPob.tryb], LVGL_WYKRESY_PUNKTOW);
  else            snprintf(bufor, n, "%s", dlugi ? "/api/history/long" : "/api/history");
  return bufor;
}

static void wykresyLinia(const char* linia) { lvgl_dashboard_poc_wykresy_strumien_linia(linia); }

// Koniec pobierania (udany albo nie): przekazuje wynik ekranowi, pisze linie
// logu, zwalnia gniazdo. `wynik` to to, co wie warstwa sieci; ekran moze go
// jeszcze zmienic (np. odpowiedz poprawna, ale bez danych) - do logu idzie
// to, co faktycznie widac na ekranie.
static bool wykresyMoznaPonowic(int wynik);   // nizej
static void wykresyZakoncz(int wynik, int szczegol) {
  const bool dlugi = (wykresyPob.tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  if (wykresyMoznaPonowic(wynik)) {
    // [v1.5.6] Centrala nic nie odpowiedziala - jeszcze raz za chwile, ekran
    // dalej czeka ("Ladowanie..."), log jedna linia. Faktyczny start w
    // obslugaWykresyFetch(), gdy minie odstep.
    logBezPamieci("tag=WYKRESY zdarzenie=ponowienie tryb=%s sposob=%s proba=%u/%u powod=%s etap=%c po_ms=%lu blad=\"%s\"\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], wykresyPob.nowySposob ? "centrala" : "panel",
                  (unsigned)(wykresyPob.proba + 1), (unsigned)WYKRESY_PROB_MAKS,
                  lvgl_dashboard_poc_wykresy_wynik_nazwa(wynik), wykresyFetch.etapBledu(),
                  (unsigned long)(millis() - wykresyPob.startMs), wykresyFetch.ostatniBlad());
    wykresyFetch.skwituj();
    wykresyPob.proba++;
    wykresyPob.ostatniWynik = wynik;
    wykresyPob.ponowOdMs = millis() + WYKRESY_ODSTEP_PONOWIENIA_MS;
    if (wykresyPob.ponowOdMs == 0) wykresyPob.ponowOdMs = 1;
    lvgl_dashboard_poc_wykresy_ponowienie(wykresyPob.proba, WYKRESY_PROB_MAKS);
    return;
  }
  const int naEkranie = lvgl_dashboard_poc_wykresy_strumien_koniec(dlugi, wykresyFetch.liniiCiala(), wynik, szczegol);
  LvglWykresyStatystyka st;
  lvgl_dashboard_poc_wykresy_statystyka(&st);
  const char* sposob = wykresyPob.nowySposob ? "centrala" : "panel";
  const unsigned long ms = (unsigned long)(millis() - wykresyPob.startMs);
  if (naEkranie == LVGL_WYKRESY_OK) {
    // sposob=centrala: punkty policzyla centrala (3.58.4+); sposob=panel: panel
    // sam scisnal caly CSV starszej centrali. wierszy_centrali: ile wierszy
    // historii centrala wziela pod uwage (-1 = nie podala, stary CSV).
    logBezPamieci("tag=WYKRESY zdarzenie=pobrano tryb=%s sposob=%s punktow=%d przerw=%d wierszy_centrali=%ld linii=%lu ucietych_linii=%lu bajtow=%lu ms=%lu ram=%lu ram_min=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, st.punktow, st.przerw, st.wierszyZrodla,
                  (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.zaDlugichLinii(),
                  (unsigned long)wykresyFetch.bajtowCiala(), ms, (unsigned long)ramWolny(), (unsigned long)ramMin());
  } else if (naEkranie == LVGL_WYKRESY_NIEPELNE) {
    // Tylko przy starszej centrali: caly CSV nie doszedl (powod=limit_czasu
    // albo urwana), wykres narysowany z tego, co doszlo, z dopiskiem NIEPELNE.
    logBezPamieci("tag=WYKRESY lvl=WARN zdarzenie=pobrano_czesc tryb=%s sposob=%s powod=%s etap=%c punktow=%d linii=%lu bajtow=%lu z_zapowiedzianych=%ld ms=%lu msg=\"%s\"\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(wynik),
                  wykresyFetch.etapBledu(), st.punktow, (unsigned long)wykresyFetch.liniiCiala(),
                  (unsigned long)wykresyFetch.bajtowCiala(), wykresyFetch.dlugoscZapowiedziana(), ms, wykresyFetch.ostatniBlad());
  } else if (naEkranie == LVGL_WYKRESY_PUSTO || naEkranie == LVGL_WYKRESY_ZA_MALO) {
    // Odpowiedz poprawna, tylko centrala nie ma jeszcze z czego zrobic wykresu
    // (swiezo po pierwszym uruchomieniu albo po wyczyszczeniu historii).
    logBezPamieci("tag=WYKRESY zdarzenie=brak_danych tryb=%s sposob=%s przyczyna=%s punktow=%d wierszy_centrali=%ld linii=%lu bajtow=%lu ms=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(naEkranie), st.punktow,
                  st.wierszyZrodla, (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.bajtowCiala(), ms);
  } else {
    logBezPamieci("tag=WYKRESY lvl=WARN zdarzenie=blad tryb=%s sposob=%s przyczyna=%s kod=%d etap=%c errno=%d prob=%u punktow=%d linii=%lu bajtow=%lu ms=%lu ram=%lu ram_blok=%lu msg=\"%s\"\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], sposob, lvgl_dashboard_poc_wykresy_wynik_nazwa(naEkranie),
                  wykresyFetch.kodHttp(), wykresyFetch.etapBledu(), wykresyFetch.numerBledu(), (unsigned)wykresyPob.proba, st.punktow,
                  (unsigned long)wykresyFetch.liniiCiala(), (unsigned long)wykresyFetch.bajtowCiala(), ms,
                  (unsigned long)ramWolny(), (unsigned long)ramBlok(), wykresyFetch.ostatniBlad());
  }
  wykresyFetch.skwituj();
  wykresyPob.aktywne = false;
  wykresyPob.ponowOdMs = 0;
}

// [v1.5.6] Czy po tym bledzie warto zapytac jeszcze raz: tak, gdy centrala
// NIE PRZYJELA polaczenia (limit laczenia 1,5 s, connect() odrzucony,
// gniazdo zamkniete bez bajtu odpowiedzi) - to czkawka AP, nie stan
// centrali; na plytce konczy sie najczesciej w 1,5 s. Nie ponawiamy:
// zapytanie przyjete, ale bez odpowiedzi przez 6 s (etap h - centrala
// pracuje, tylko wolno; kolejne 6 s x2 nic nie da), odpowiedz urwana w
// trakcie, limit calosci, brak pamieci panelu (nie pomoze), kod HTTP (to
// jest odpowiedz).
static bool wykresyMoznaPonowic(int wynik) {
  if (wykresyPob.proba >= WYKRESY_PROB_MAKS) return false;
  if (wynik != LVGL_WYKRESY_BRAK_POLACZENIA) return false;
  return wykresyFetch.kodHttp() == 0 && wykresyFetch.bajtowCiala() == 0;
}

// Blad warstwy sieci -> przyczyna dla ekranu i logu.
static int wykresyWynikZBledu() {
  if (diag::bladGniazdaToBrakPamieci(wykresyFetch.numerBledu())) return LVGL_WYKRESY_BRAK_PAMIECI;
  switch (wykresyFetch.etapBledu()) {
    case 'c': case 'h': case 'p': return LVGL_WYKRESY_LIMIT_CZASU;       // centrala nie zdazyla
    case 'z':                     return LVGL_WYKRESY_BRAK_POLACZENIA;   // [v1.5.6] zamkniete bez bajtu odpowiedzi = centrala nie przyjela (dotad "zla odpowiedz")
    case 'k':                     return LVGL_WYKRESY_ZLA_ODPOWIEDZ;     // to nie byla odpowiedz HTTP
    case 'r':                     return (wykresyFetch.bajtowCiala() > 0) ? LVGL_WYKRESY_URWANA : LVGL_WYKRESY_BRAK_POLACZENIA;
    default:                      return LVGL_WYKRESY_BRAK_POLACZENIA;   // g n l a s: nie udalo sie polaczyc / wyslac
  }
}

// Centrala nie zna /api/history/panel: ta sama prosba po staremu.
static void wykresyPrzejdzNaStarySposob(int kod) {
  const bool dlugi = (wykresyPob.tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  logBezPamieci("tag=WYKRESY zdarzenie=stara_centrala kod=%d msg=\"centrala nie zna /api/history/panel (firmware starszy niz 3.58.4) - pobieram cala historie po staremu\"\n", kod);
  wykresyPob.nowySposob = false;
  wykresyPob.kodNowego = kod;
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);   // od nowa: to, co przyszlo do tej pory, nie bylo danymi
  if (!wykresyFetch.start(dlugi ? "/api/history/long" : "/api/history", wykresyLinia)) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
  }
}

// Wolane z dashboard_lvgl_poc.cpp (WykresyFetchCb), gdy uzytkownik chce
// zobaczyc wykres. Tylko STARTUJE pobieranie i od razu wraca; reszta dzieje
// sie w obslugaWykresyFetch() w kolejnych obrotach loop().
void wykresyFetchStart(int tryb) {
  if (tryb < 0 || tryb > 2) tryb = LVGL_WYKRESY_TRYB_DZIEN;
  if (wykresyPob.aktywne) {
    // Nowa prosba w trakcie poprzedniej (zmiana zakresu): stara przepada.
    logBezPamieci("tag=WYKRESY zdarzenie=przerwano powod=nowa_prosba tryb=%s po_ms=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], (unsigned long)(millis() - wykresyPob.startMs));
  }
  wykresyFetch.wyczysc();   // takze: porzuca poprzednie zadanie, jesli jeszcze trwalo
  wykresyPob.aktywne = true;
  wykresyPob.nowySposob = true;
  wykresyPob.tryb = (uint8_t)tryb;
  wykresyPob.kodNowego = 0;
  wykresyPob.startMs = millis();
  wykresyPob.postepMs = wykresyPob.startMs;
  wykresyPob.proba = 1;          // [v1.5.6]
  wykresyPob.ponowOdMs = 0;
  wykresyPob.ostatniWynik = 0;
  const bool dlugi = (tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
  // Przed kazda mozliwoscia wczesnego wyjscia: ekran czeka na _koniec().
  lvgl_dashboard_poc_wykresy_strumien_start(dlugi);
  Serial.printf("tag=WYKRESY zdarzenie=start tryb=%s ram=%lu\n", WYKRESY_TRYB_ADRES[tryb], (unsigned long)ramWolny());

  if (WiFi.status() != WL_CONNECTED) {
    wykresyZakoncz(LVGL_WYKRESY_BRAK_WIFI, 0);
    return;
  }
  char sciezka[56];
  snprintf(sciezka, sizeof(sciezka), "/api/history/panel?tryb=%s&pkt=%d", WYKRESY_TRYB_ADRES[tryb], LVGL_WYKRESY_PUNKTOW);
  if (!wykresyFetch.start(sciezka, wykresyLinia)) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
  }
}

// Co obrot loop(): popycha pobieranie historii i konczy je, gdy przyszlo
// wszystko albo cos poszlo nie tak.
void obslugaWykresyFetch() {
  if (!wykresyPob.aktywne) return;

  // Uzytkownik wyszedl z podstrony Wykres (strzalka, X, gest, bezczynnosc,
  // ekran alarmu) - nie ma dla kogo pobierac.
  if (!lvgl_dashboard_poc_wykresy_czeka_na_dane()) {
    logBezPamieci("tag=WYKRESY zdarzenie=przerwano powod=wyjscie_z_ekranu tryb=%s po_ms=%lu linii=%lu\n",
                  WYKRESY_TRYB_ADRES[wykresyPob.tryb], (unsigned long)(millis() - wykresyPob.startMs),
                  (unsigned long)wykresyFetch.liniiCiala());
    wykresyFetch.przerwij();
    wykresyPob.aktywne = false;
    wykresyPob.ponowOdMs = 0;
    return;
  }

  // [v1.5.6] Czekamy z kolejna proba (po czkawce AP); gdy minie odstep -
  // to samo zapytanie, tym samym sposobem. WiFi mogl w miedzyczasie pasc.
  if (wykresyPob.ponowOdMs != 0) {
    if ((long)(millis() - wykresyPob.ponowOdMs) < 0) return;
    wykresyPob.ponowOdMs = 0;
    if (WiFi.status() != WL_CONNECTED) {
      wykresyZakoncz(LVGL_WYKRESY_BRAK_WIFI, 0);
      return;
    }
    const bool dlugi = (wykresyPob.tryb == LVGL_WYKRESY_TRYB_TYDZIEN);
    char sciezka[56];
    wykresySciezka(wykresyPob.nowySposob, dlugi, sciezka, sizeof(sciezka));
    lvgl_dashboard_poc_wykresy_strumien_start(dlugi);   // od nowa - z poprzedniej proby nic nie przyszlo
    if (!wykresyFetch.start(sciezka, wykresyLinia)) {
      wykresyZakoncz(wykresyWynikZBledu(), 0);
    }
    return;
  }

  const uint32_t bajtowPrzed = wykresyFetch.bajtowCiala();
  wykresyFetch.poll();
  // [v1.5.7 RATUNEK] Przyszly nowe bajty historii: centrala zyje, nie
  // wymuszaj ponownego polaczenia WiFi tylko dlatego, ze status sie spoznia
  // (patrz diag::Ratunek::centralaOdezwalaSie()).
  if (wykresyFetch.bajtowCiala() != bajtowPrzed) panelRatunek.centralaOdezwalaSie();

  if (wykresyFetch.wTrakcie()) {
    if (wykresyPob.nowySposob) {
      const int kodTeraz = wykresyFetch.kodHttp();   // 0, dopoki nie przyszla linia statusu
      // Kod inny niz 200 i 503: centrala nie zna nowego adresu (wersje do
      // 3.58.3 odpowiadaja przekierowaniem 302). Tresci takiej odpowiedzi nie
      // czytamy - od razu pytamy po staremu.
      if (kodTeraz != 0 && kodTeraz != 200 && kodTeraz != 503) {
        wykresyPrzejdzNaStarySposob(kodTeraz);
        return;
      }
      // Kod 200, ale pierwsza linia tresci to nie "#panel-hist": to nie jest
      // centrala 3.58.4 (bardzo stare wersje oddawaly pod kazdym adresem
      // /api/history/... caly CSV). Tego tez nie czytamy do konca.
      if (kodTeraz == 200 && wykresyFetch.liniiCiala() >= 1 && !lvgl_dashboard_poc_wykresy_strumien_zredukowany()) {
        wykresyPrzejdzNaStarySposob(kodTeraz);
        return;
      }
    }
    const unsigned long teraz = millis();
    if (teraz - wykresyPob.postepMs >= 300) {
      wykresyPob.postepMs = teraz;
      lvgl_dashboard_poc_wykresy_postep(wykresyFetch.liniiCiala(), wykresyFetch.bajtowCiala());
    }
    return;
  }
  if (!wykresyFetch.gotowyWynik()) {   // nie powinno sie zdarzyc: aktywne pobieranie bez zadania
    wykresyPob.aktywne = false;
    return;
  }

  if (!wykresyFetch.bylSukces()) {
    wykresyZakoncz(wykresyWynikZBledu(), 0);
    return;
  }
  const int kod = wykresyFetch.kodHttp();
  if (wykresyPob.nowySposob) {
    if (kod == 200 && lvgl_dashboard_poc_wykresy_strumien_zredukowany()) {
      wykresyZakoncz(wykresyFetch.kompletna() ? LVGL_WYKRESY_OK : LVGL_WYKRESY_URWANA, 0);
    } else if (kod == 503) {
      wykresyZakoncz(LVGL_WYKRESY_HTTP, kod);   // centrala zna adres, ale zabraklo jej pamieci
    } else {
      wykresyPrzejdzNaStarySposob(kod);         // 302 / 404 / cos innego: centrala < 3.58.4
    }
    return;
  }
  if (kod != 200) {
    wykresyZakoncz(LVGL_WYKRESY_HTTP, kod);
    return;
  }
  wykresyZakoncz(wykresyFetch.kompletna() ? LVGL_WYKRESY_OK : LVGL_WYKRESY_URWANA, 0);
}

void pobierzStatus(); // forward declaration - pompaManualWyslij() wola ja przed jej definicja nizej
void pobierzUstawienia(); // forward declaration - serwoUstawienieWyslij() (ponizej) wola ja przed jej definicja (linia ~959)
// [v1.4.2 C4] Trzeci parametr: co odswiezyc po zakonczeniu polecenia (domyslnie
// USTAWIENIA - patrz komentarz przy definicji wyslijTrybPracy()).
void wyslijTrybPracy(const char* pole, long wartosc, PoKomendzie po = PoKomendzie::USTAWIENIA); // forward declaration - pompaWyborWyslij() (ponizej) wola ja przed jej definicja; BLAD KOMPILACJI bez tego: "'wyslijTrybPracy' was not declared in this scope"
// [POPRAWKA, ta sesja - panel SERWO v2] Ten sam powod co wyzej -
// serwoTrybWyslij()/serwoPozycjaWyslij() (ponizej) wolaja wyslijServoManualny()
// przed jej definicja.
void wyslijServoManualny(const char* os, int procent, PoKomendzie po = PoKomendzie::BRAK);
// [v1.4.2 C4] Kolejka polecen - definicje przy wyslijTrybPracy() nizej.
void poleceniePlanuj(const char* sciezka, const char* klucz, const char* cialo, PoKomendzie po);
void poleceniaKrok();

// [LVGL-API] Lokalny odpowiednik ustawSerwo()/ustawTryb() dla pompy - jedna
// funkcja ktora dashboard_lvgl_poc.cpp woła przez callback (patrz
// lvgl_dashboard_poc_ustaw_callback_pompy() w setup()), zeby ten plik nie
// musial wiedziec NIC o HTTPClient/WiFi. `stanTekst` to dokladnie to, czego
// oczekuje centrala: "auto" / "wl" / "wyl" (patrz /api/pompa-manual).
// Po udanym POST od razu odswieza stan (pobierzStatus() + przekazanie do
// LVGL) - zeby uzytkownik nie czekal do kolejnego cyklu STATUS_INTERWAL_MS
// (2s) na potwierdzenie, ze dotkniecie przycisku faktycznie cos zmienilo.
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] POST + nastepujacy po nim GET
// statusu NIE MOGA juz byc jednym synchronicznym wywolaniem (patrz komentarz
// przy pendingPoKomendzie wyzej) - ta funkcja tylko STARTUJE komende, wynik
// (sukces/porazka) i nastepujacy po nim pobierzStatus()+aktualizacja LVGL
// obsluzy obslugaKomendaFetch() nizej, gdy komendaFetch faktycznie sie
// zakonczy (jedna-dwie iteracje loop() pozniej, nie w tym samym wywolaniu).
void pompaManualWyslij(const char* stanTekst) {
  // [v1.4.2 C4] Do kolejki - patrz poleceniePlanuj(). Celowo NIC nie
  // zmieniamy tu lokalnie w UI - po zakonczeniu polecenia pobierzStatus()
  // pokaze prawdziwy stan, a przy porazce kafelek nie "klamie".
  String cialo = String("stan=") + stanTekst;
  poleceniePlanuj("/api/pompa-manual", "stan", cialo.c_str(), PoKomendzie::STATUS); // ten kafelek czerpie z /api/status
}

// [ROZBUDOWA, ta sesja - strona POMPA / wybor strategii] Analogiczny
// "lokalny most" dla wyboru Trociniak/Kopciuch/Automatyczny. Aktualizuje
// `trybWyboru` PRZED wyslaniem, zeby lokalny stan byl od razu spojny z tym,
// co user wybral.
// [v1.4.2 C4] pendingPoKomendzie=STATUS, bo ten ekran czerpie trybWyboru z
// /api/status, nie z POLA[] - flaga wedruje teraz razem z poleceniem w kolejce.
void pompaWyborWyslij(int wartosc) {
  trybWyboru = wartosc;
  wyslijTrybPracy("wybor", wartosc, PoKomendzie::STATUS);
}

// [POPRAWKA, ta sesja - panel SERWO v2] Dwa nowe "lokalne mosty", dokladnie
// ten sam ksztalt co pompaWyborWyslij() wyzej. Rejestrowane w setup() przez
// lvgl_dashboard_poc_ustaw_callback_serwo_tryb()/..._pozycja().
void serwoTrybWyslij(int wartosc) {
  trybSerwaWartosc = wartosc;
  wyslijTrybPracy("trybSerwa", wartosc, PoKomendzie::STATUS); // trybSerwaWartosc tez czerpiemy z /api/status
}

// `id`: 0=klapa, 1=syberka (tak jak dashboard_lvgl_poc.cpp go uzywa) ->
// mapowane na string, ktorego oczekuje centrala. UWAGA: centrala oczekuje
// "syberek", NIE "syberka" (patrz main_centrala.cpp ~4777-4779) - dokladnie
// ta literowka byla przyczyna bledu z poprzedniej sesji.
void serwoPozycjaWyslij(int id, int pozycja) {
  const char *os = (id == 0) ? "klapa" : "syberek";
  wyslijServoManualny(os, pozycja, PoKomendzie::STATUS); // klapa/syberka wracaja przez /api/status
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] Jeden generyczny "most",
// dokladnie ten sam ksztalt co pompaWyborWyslij()/serwoTrybWyslij() wyzej -
// woła JUZ ISTNIEJACA wyslijTrybPracy(pole, wartosc) (dokladnie ta sama
// funkcja/endpoint /api/ustawienia-set co dla POLA[]) i od razu odswieza POLA[] z centrali, zeby ekran LVGL pokazal
// PRAWDZIWA (zwalidowana przez centrale) wartosc, nie tylko to co user
// klepnal lokalnie - ten sam "nie klam po nieudanym poleceniu" wzorzec co
// pompaManualWyslij(). Rejestrowana w setup() przez
// lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia().
void serwoUstawienieWyslij(const char *pole, long wartosc) {
  // wyslijTrybPracy() samodzielnie sprawdza komendaFetch.wTrakcie(), ustawia
  // pendingPoKomendzie=USTAWIENIA i startuje POST; obslugaKomendaFetch()
  // zrobi pobierzUstawienia() (odswieza POLA[] + lvgl_dashboard_poc_aktualizuj_ustawienia_serwa())
  // dopiero PO faktycznym zakonczeniu POST-a, nie od razu tutaj.
  wyslijTrybPracy(pole, wartosc);
}

// [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY] Dokladnie ten sam ksztalt
// co serwoUstawienieWyslij() powyzej - jeden generyczny "most" dla 5 pol
// numerycznych (czasOn/czasOff/tempOn/tempOff/antystopDni). Rejestrowana w
// setup() przez lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia().
void pompaUstawienieWyslij(const char *pole, long wartosc) {
  wyslijTrybPracy(pole, wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej - ten sam wzorzec
}

// Osobny "most" dla przelacznika antystopWlaczony (bool, poza POLA[] - ten
// sam powod co trybWyboru/trybSerwaWartosc, patrz komentarz przy deklaracji
// antystopWlaczonyWartosc). Rejestrowana przez
// lvgl_dashboard_poc_ustaw_callback_pompa_antystop().
void pompaAntystopWyslij(int wartosc) {
  antystopWlaczonyWartosc = (wartosc != 0);
  wyslijTrybPracy("antystopWlaczony", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}

// [ROZSZERZENIE, ta sesja - PANEL DYM] Dwa "mosty", dokladnie ten sam ksztalt
// co serwoUstawienieWyslij()/pompaUstawienieWyslij() powyzej - progAlarmDym
// (suwak) i 3 pola czasowe uzywaja tego samego generycznego
// /api/ustawienia-set (wyslijTrybPracy()) co reszta ekranow. Rejestrowane w
// setup() przez lvgl_dashboard_poc_ustaw_callback_dym_prog()/
// ..._dym_ustawienia().
void dymProgWyslij(int wartosc) {
  wyslijTrybPracy("progAlarmDym", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}
void dymUstawienieWyslij(const char *pole, long wartosc) {
  wyslijTrybPracy(pole, wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}

// [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] Ten sam wzorzec co
// dymProgWyslij() powyzej - jeden wspolny prog dla kafelkow PIEC CO i
// PANEL. progAlarmTemp juz jest w POLA[] (patrz main.cpp linia ~466), wiec
// wyslijTrybPracy()/pobierzUstawienia() dzialaja bez zmian.
void alarmTempProgWyslij(int wartosc) {
  wyslijTrybPracy("progAlarmTemp", wartosc); // patrz komentarz w serwoUstawienieWyslij() wyzej
}


// ----------------------------------------------------------------------------
// [v1.2.0 DIAG] DIAGNOSTYKA: co przyszlo z centrali i co trafilo na ekran
// ----------------------------------------------------------------------------
// Loguje odpowiedz centrali, ktorej NIE dalo sie uzyc - z przyczyna (kod HTTP
// / brak ciala / blad JSON), rozmiarem oraz poczatkiem i koncem tresci. W
// v1.1.0 ten przypadek konczyl sie cichym `return`: warstwa sieciowa pisala
// "sukces" (bo gniazdo sie zamknelo), a ekran po prostu przestawal sie
// odswiezac. Przy bledzie JSON dochodzi `podejrzane=` - pierwsze pole, ktorego
// wartosc nie jest JSON-em (typowo `"cisnienie":nan`, patrz
// diag::znajdzZlaWartosc()); puste, gdy takiego pola nie znaleziono.
// Dlawik: najwyzej jeden wpis na 10 s na endpoint, z licznikiem pominietych.
static void zglosZlaOdpowiedz(const char *co, diag::Dlawik &dlawik, diag::Wynik wynik, int kodHttp,
                               const char *bladJson, const String &surowa, const char *cialo) {
  if (!dlawik.pozwol(millis(), 10000UL)) return;
  const char *tresc = (cialo != nullptr) ? cialo : surowa.c_str();
  char poczatek[41], koniec[33], podejrzane[49];
  diag::skrotDoLogu(tresc, poczatek, sizeof(poczatek));
  diag::skrotDoLogu(diag::ogon(tresc, sizeof(koniec) - 1), koniec, sizeof(koniec));
  diag::skrotDoLogu((wynik == diag::WYNIK_JSON) ? diag::znajdzZlaWartosc(tresc) : nullptr, podejrzane, sizeof(podejrzane));
  // ram/ram_blok: json=NoMemory albo ucieta odpowiedz (IncompleteInput) przy
  // malym ram_blok to brak pamieci w panelu, nie blad centrali. [v1.2.2] Te
  // dwie liczby to pamiec, z ktorej korzysta malloc() - patrz ramWolny();
  // v1.2.0/v1.2.1 pisaly tu "heap=/blok_max=" zawyzone o ok. 39 kB.
  // logBezPamieci(): ta linia musi sie wypisac wlasnie wtedy, gdy pamieci nie ma.
  logBezPamieci("tag=%s lvl=ERR zdarzenie=odpowiedz_nieczytelna przyczyna=%s http=%d json=%s bajtow=%u cialo_bajtow=%u pominieto=%lu ram=%lu ram_blok=%lu poczatek=\"%s\" koniec=\"%s\" podejrzane=\"%s\"\n",
                co, diag::wynikTekst(wynik), kodHttp, bladJson,
                (unsigned)surowa.length(), (unsigned)(cialo != nullptr ? strlen(cialo) : 0),
                (unsigned long)dlawik.zabierzPominiete(),
                (unsigned long)ramWolny(), (unsigned long)ramBlok(),
                poczatek, koniec, podejrzane);
}

// Biezacy `stan` -> dashboard LVGL. Ta sama lista argumentow co w
// obslugaStatusFetch() i setup() (tamte dwa wywolania zostaly bez zmian).
static void lvglPokazStan() {
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 diag::serwoStopnieNaProcent(stan.klapa, diag::KLAPA_KAT_MAKS), diag::serwoStopnieNaProcent(stan.syberka, diag::SYBERKA_KAT_MAKS), (int)trybWyboru,   // [v1.5.6] centrala podaje stopnie
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty,
                                 stan.pogodaKod, stan.pogodaKodOpad, stan.pogodaDzien, stan.wiatrPorywy,   // [v1.5.14]
                                 stan.pogodaTemp);   // [v1.5.17]
}

// Znacznik na srodku naglowka dashboardu - Z KTORA centrala rozmawia panel
// i czy pokazywane wartosci sa pomiarem:
//   "SYMULATOR"              - odpowiada centrala-testowa-c3,
//   "SYMULACJA: zewn, pokoj" - prawdziwa centrala podaje te czujniki jako
//                              symulowane (wlaczone z jej panelu WWW),
//   "2 CENTRALE (A4:1C)"     - przy starcie w zasiegu byly 2 sieci o nazwie
//                              WIFI_SSID; w nawiasie dwa ostatnie bajty BSSID
//                              tej, z ktora panel jest polaczony (pelny BSSID
//                              jest w logu: tag=WIFI zdarzenie=polaczono).
// Pusty = jedna, zwykla centrala bez symulacji (nic sie nie wyswietla).
// [v1.2.1 RATUNEK] Gdy panel NIE MA danych (kafelki pokazuja "--"), w tym
// samym miejscu stoi powod i licznik sekund, np. "CENTRALA MILCZY 14 s", a w
// ostatnich 10 s przed samoczynnym restartem "RESTART ZA 7 s: BRAK WIFI"
// (patrz diag::znacznikBrakuDanych() i panelRatunekKrok()).
static void lvglPokazZnacznikCentrali() {
  char znacznik[48];
  if (straznikSwiezosci.oznaczoneJakoStare) {
    const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
    const uint32_t progRestartu = diag::progRestartuMs(rtcAutorestart.kolejnych);
    diag::znacznikBrakuDanych(znacznik, sizeof(znacznik), przyczyna,
                              panelRatunek.bezDanychMs / 1000UL,
                              diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)
                                  ? (panelRatunek.doRestartuMs(progRestartu) + 999UL) / 1000UL
                                  : diag::BEZ_RESTARTU);
  } else {
    diag::znacznikCentrali(znacznik, sizeof(znacznik),
                           centralaRozpoznana && centralaToSymulator,
                           wifiCentralPrzyStarcie,
                           (WiFi.status() == WL_CONNECTED) ? WiFi.BSSID() : nullptr,
                           centralaSymulacjaMaska);
  }
  lvgl_dashboard_poc_aktualizuj_centrale(znacznik);
}

// [v1.2.0 DANE-NIEAKTUALNE] Wolane RAZ, gdy od ostatniego poprawnie
// odczytanego /api/status minelo PROG_NIEAKTUALNYCH_DANYCH_MS (patrz loop()).
// W v1.1.0 po utracie danych kafelki pokazywaly ostatnie znane wartosci bez
// zadnego znaku, ze sa stare - na ekranie sterownika pieca to grozniejsze
// niz brak wartosci. lvgl_dashboard_poc_aktualizuj(polaczono=false) pokazuje
// "--" na kafelkach i "X" na wskazniku WiFi; ta galaz istniala od dawna, ale
// v1.1.0 docieral do niej wylacznie z setup() (gdy nie udal sie PIERWSZY
// odczyt) - w trakcie pracy nieudany odczyt konczyl sie `return` przed
// jakimkolwiek rysowaniem.
void panelOznaczDaneJakoNieaktualne() {
  stan.polaczono = false;
  // wiek_s=-1: od wlaczenia panelu nie bylo jeszcze ANI JEDNEGO poprawnego
  // statusu. wifi=1 przy braku danych znaczy: siec jest, ale centrala nie
  // odpowiada albo jej odpowiedzi sa nieczytelne (patrz tag=STATUS lvl=ERR).
  // [v1.2.1] przyczyna=wifi|milczy|odp|pam - to samo, co panel pokazuje w naglowku
  // (BRAK WIFI / CENTRALA MILCZY / ZLA ODPOWIEDZ / BRAK PAMIECI), patrz diag::Przyczyna.
  // restart_za_s=-1: przy tej przyczynie restartu juz nie bedzie (diag::restartMaSens()).
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
  daneDobreSeria = false;   // koniec nieprzerwanie poprawnych danych
  logBezPamieci("tag=PANEL lvl=WARN zdarzenie=dane_nieaktualne wiek_s=%ld prog_s=%lu wifi=%d przyczyna=%s restart_za_s=%ld\n",
                swiezoscStatusu.byloKiedykolwiek ? (long)swiezoscStatusu.wiekSekund() : -1L,
                (unsigned long)(PROG_NIEAKTUALNYCH_DANYCH_MS / 1000UL),
                (int)wifiOk,
                diag::przyczynaSkrot(przyczyna),
                diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)
                    ? (long)(panelRatunek.doRestartuMs(diag::progRestartuMs(rtcAutorestart.kolejnych)) / 1000UL) : -1L);
  lvglPokazStan();   // polaczono=false -> "--" na wszystkich kafelkach
  lvglPokazZnacznikCentrali();   // [v1.2.1] naglowek: czego brakuje
}

// ----------------------------------------------------------------------------
// [v1.2.1 RATUNEK] Panel bez danych ratuje sie sam
// ----------------------------------------------------------------------------
// Zgloszenie z 2026-11-12 (wg uzytkownika na plytce byla v1.2.0): panel nie
// pokazywal swiezych danych, az uzytkownik go zresetowal - po resecie dane
// byly od razu. Logu panelu z tej chwili nie ma, wiec PRZYCZYNA NIE JEST
// USTALONA. Wiadomo tylko tyle: (1) w logu z 2026-11-11 (starszy build), tuz
// przed takim samym resetem, kazda proba polaczenia z centrala konczyla sie
// od razu bledem "connect() natychmiastowy blad"; (2) log centrali z
// 2026-11-12 wyglada na zebrany ok. 5 minut po JEJ starcie, czyli centrala
// byla zapewne krotko wczesniej restartowana. v1.2.1 robi wiec dwie rzeczy: automatyzuje
// to, co pomaga (restart), i zapisuje to, czego brakuje do ustalenia przyczyny.
//
// Kolejnosc (czas liczony od ostatniego poprawnie odczytanego statusu):
//   10 s  kafelki "--" (od v1.2.0) + w naglowku powod: BRAK WIFI /
//         CENTRALA MILCZY / ZLA ODPOWIEDZ i licznik sekund,
//   12 s  WiFi zglasza polaczenie, a centrala milczy -> rozlacz i polacz od
//         nowa (wifiWymusPolaczenie()); po kolejnych 12 s milczenia jeszcze
//         raz, juz bez zapamietanego BSSID. v1.1.0 i v1.2.0 w tym stanie nie
//         robily NIC - reconnect ruszal tylko wtedy, gdy WiFi samo zglosilo
//         rozlaczenie,
//   45 s  restart panelu (panelAutorestart()). Kolejne restarty z rzedu: po
//         90 s, 3 min i dalej co 5 min (diag::progRestartuMs()); "z rzedu"
//         konczy sie po 2 min nieprzerwanie poprawnych danych. Przy
//         nieczytelnych odpowiedziach centrali najwyzej 2 restarty
//         (diag::restartMaSens()).
// Po takim restarcie panel zglasza centrali powod (reset_powod=auto_...), a
// centrala wpisuje go do SWOJEGO logu: "tag=RESET-DIAG ... powod=auto_...".
// Zgloszenie idzie raz (do pierwszej odpowiedzi centrali) i nie po kazdym
// restarcie z serii - patrz autorestartOdczytajPrzyStarcie().
void wifiWymusPolaczenie(bool bezBssid, uint8_t nr);   // definicja w sekcji WIFI

// Zapisuje w pamieci RTC to, co ma przetrwac samoczynny restart (powod dla
// centrali, liczniki, stan alarmow). Bez przydzialow pamieci i bez dlugich
// linii logu - wolane takze wtedy, gdy pamieci juz nie ma.
static void autorestartZapiszPamiec(diag::Przyczyna przyczyna, uint8_t etap, int numer, uint32_t ram, uint32_t blok) {
  const uint32_t bezDanychS = panelRatunek.bezDanychMs / 1000UL;
  rtcAutorestart.przyczyna = (uint8_t)przyczyna;
  rtcAutorestart.kodRozlaczenia = wifiOstatniKodRozlaczenia;
  rtcAutorestart.odnotujRestart();   // liczniki: z rzedu, w serii, lacznie
  rtcAutorestart.bezDanychS = (bezDanychS > 65535UL) ? 65535 : (uint16_t)bezDanychS;
  rtcAutorestart.heapKb = (uint16_t)(ram / 1024UL);
  rtcAutorestart.blokKb = (uint16_t)(blok / 1024UL);
  rtcAutorestart.bladEtap = etap;
  rtcAutorestart.bladNumer = (numer < 0 || numer > 65535) ? 65535 : (uint16_t)numer;
  // Ostatni znany stan alarmow jedzie przez restart - patrz diag::spakujAlarmy()
  // i alarmPrzywrocPoAutorestarcie(). `stan` trzyma ostatnie wartosci z
  // centrali takze wtedy, gdy danych juz nie ma.
  const int alarmOdrzuconyPoziom = lvgl_dashboard_poc_alarm_odrzucony();
  rtcAutorestart.alarmy = diag::spakujAlarmy(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel,
                                             g_brzeczykWyciszony, (int)g_brzeczykWyciszonyTyp, alarmOdrzuconyPoziom);
  rtcAutorestart.zapieczetuj();
}

void panelAutorestart(diag::Przyczyna przyczyna) {
  const uint32_t bezDanychS = panelRatunek.bezDanychMs / 1000UL;
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  const uint8_t kodRozlaczenia = wifiOstatniKodRozlaczenia;
  const uint32_t ram = ramWolny();
  const uint32_t blok = ramBlok();
  autorestartZapiszPamiec(przyczyna, (uint8_t)ostatniBladStatusuEtap, ostatniBladStatusuNumer, ram, blok);

  logBezPamieci("tag=PANEL lvl=ERR zdarzenie=autorestart przyczyna=%s bez_danych_s=%lu nr_z_rzedu=%u w_serii=%u lacznie=%u wifi=%d rssi=%d "
                "rozl_kod=%u rozl_nazwa=%s rozl_n=%lu ostatni_blad=\"%s\" ram=%lu ram_blok=%lu ram_min=%lu alarmy=0x%02X "
                "msg=\"brak danych z centrali - restart panelu\"\n",
                diag::przyczynaSkrot(przyczyna), (unsigned long)bezDanychS,
                (unsigned)rtcAutorestart.kolejnych, (unsigned)rtcAutorestart.seria, (unsigned)rtcAutorestart.lacznie,
                (int)wifiOk, wifiOk ? (int)WiFi.RSSI() : 0,
                (unsigned)kodRozlaczenia, diag::nazwaKoduRozlaczenia(kodRozlaczenia), (unsigned long)wifiLiczbaRozlaczen,
                ostatniBladStatusuTekst, (unsigned long)ram, (unsigned long)blok, (unsigned long)ramMin(),
                (unsigned)rtcAutorestart.alarmy);
  Serial.flush();
  delay(100);
  ESP.restart();
}

// [v1.2.2 PAMIEC] Blad wewnetrzny LVGL (LV_ASSERT_HANDLER w include/lv_conf.h).
// W praktyce: LVGL poprosil o pamiec w trakcie rysowania i jej nie dostal.
// Domyslnie LVGL 8 staje wtedy w pustej petli "while(1);" - ekran zamiera, a
// po 15 s panel restartuje watchdog (log z 2026-11-12: "task_wdt ... loopTask",
// slad wywolan konczy sie w rysowaniu LVGL), bez sladu, co sie stalo. Tutaj:
// napis na ekranie, wpis w logu, powod zapisany dla centrali
// ("auto_lvgl_...", z iloscia wolnej pamieci) i restart od razu.
// Funkcja nie moze wrocic (LVGL uzylby pustego wskaznika) i nie moze prosic
// o pamiec (stad logBezPamieci()).
extern "C" void panel_lvgl_assert(void) {
  static bool wTrakcie = false;
  if (wTrakcie) ESP.restart();   // blad w trakcie obslugi bledu - nie probuj drugi raz
  wTrakcie = true;
  const uint32_t ram = ramWolny();
  const uint32_t blok = ramBlok();
  logBezPamieci("tag=PANEL lvl=ERR zdarzenie=lvgl_assert ram=%lu ram_blok=%lu ram_min=%lu msg=\"blad wewnetrzny LVGL (zwykle brak pamieci) - restart panelu\"\n",
                (unsigned long)ram, (unsigned long)blok, (unsigned long)ramMin());
  autorestartZapiszPamiec(diag::PRZYCZYNA_LVGL, 'v', 0, ram, blok);
  // Napis wprost przez sterownik ekranu (LVGL juz nic nie narysuje). LVGL
  // wola nas spoza funkcji wysylajacej piksele, wiec magistrala jest wolna.
  tft.fillRect(0, 0, 480, 44, TFT_RED);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(blok < 4096 ? "BRAK PAMIECI - RESTART" : "BLAD EKRANU - RESTART", 240, 22, 4);
  Serial.flush();
  delay(2500);   // zeby napis dalo sie przeczytac (watchdog zadania ma 15 s)
  ESP.restart();
}

// Wolane w kazdym obrocie loop().
void panelRatunekKrok() {
  // [v1.4.0 OTA] W trakcie pobierania nowego firmware'u drabina ratunkowa stoi:
  // ponowne laczenie WiFi albo restart panelu w srodku zapisu do flash nie
  // pomoglyby, a pobieranie ma wlasne limity (15 s ciszy, 5 minut calosci) i
  // samo sie konczy, gdy centrala zamilknie. Panel w tym czasie nadal pyta o
  // status (do v1.3.0 nie pytal - pobieranie zatrzymywalo petle).
  if (trwaAktualizacjaOtaPanelu) return;
  const diag::Przyczyna przyczyna = panelPrzyczynaTeraz();
  const uint32_t progRestartu = diag::progRestartuMs(rtcAutorestart.kolejnych);
  switch (panelRatunek.krok(millis(), przyczyna == diag::PRZYCZYNA_MILCZY, progRestartu)) {
    case diag::Ratunek::POLACZ_PONOWNIE:           wifiWymusPolaczenie(false, panelRatunek.wymuszen); break;
    case diag::Ratunek::POLACZ_PONOWNIE_BEZ_BSSID: wifiWymusPolaczenie(true, panelRatunek.wymuszen);  break;
    case diag::Ratunek::RESTART:
      if (diag::restartMaSens(przyczyna, rtcAutorestart.kolejnych)) {
        panelAutorestart(przyczyna);
      } else {
        // Odpowiedzi centrali sa nieczytelne mimo dwoch restartow z rzedu:
        // kolejny nic nie da (patrz diag::restartMaSens()). krok() zglosi
        // RESTART ponownie przy nastepnym obrocie - gdy przyczyna sie zmieni
        // (np. zniknie WiFi), restart nastapi od razu.
        panelRatunek.restartOdrzucony();
        static bool zalogowane = false;
        if (!zalogowane) {
          zalogowane = true;
          Serial.printf("tag=PANEL lvl=WARN zdarzenie=autorestart_pominiety przyczyna=%s nr_z_rzedu=%u ostatni_blad=\"%s\" "
                        "msg=\"odpowiedzi centrali nadal nieczytelne - kolejny restart panelu nic nie da\"\n",
                        diag::przyczynaSkrot(przyczyna), (unsigned)rtcAutorestart.kolejnych, ostatniBladStatusuTekst);
        }
      }
      break;
    case diag::Ratunek::NIC:                       break;
  }
  // Dane poprawne nieprzerwanie od 2 minut: nastepny samoczynny restart znow
  // po pierwszym, najkrotszym progu. Od godziny: koniec serii restartow -
  // numeracja zgloszen dla centrali zaczyna sie od nowa
  // (diag::zglosicRestart()).
  if (daneDobreSeria) {
    const unsigned bylo = rtcAutorestart.kolejnych;
    const unsigned byloWSerii = rtcAutorestart.seria;
    if (rtcAutorestart.poDobrychDanych(millis() - daneDobreOdMs)) {
      Serial.printf("tag=PANEL zdarzenie=autorestart_licznik_wyzerowany z_rzedu=%u->%u w_serii=%u->%u dobre_dane_s=%lu\n",
                    bylo, (unsigned)rtcAutorestart.kolejnych, byloWSerii, (unsigned)rtcAutorestart.seria,
                    (unsigned long)((millis() - daneDobreOdMs) / 1000UL));
    }
  }
  // Licznik sekund w naglowku ("BRAK WIFI 14 s") - raz na sekunde i tylko
  // wtedy, gdy kafelki pokazuja "--".
  if (straznikSwiezosci.oznaczoneJakoStare) {
    static uint32_t pokazanaSekunda = 0xFFFFFFFFUL;
    const uint32_t sekunda = panelRatunek.bezDanychMs / 1000UL;
    if (sekunda != pokazanaSekunda) {
      pokazanaSekunda = sekunda;
      lvglPokazZnacznikCentrali();
    }
  }
}

// [v1.2.2 PAMIEC] Raz na sekunde: gdy wolnego RAM-u robi sie malo, wpis w
// logu (najwyzej raz na minute) - zanim zabraknie go na odczytanie
// odpowiedzi centrali.
void panelPamiecKrok() {
  static unsigned long ostatnio = 0;
  const unsigned long teraz = millis();
  if (teraz - ostatnio < 1000UL) return;
  ostatnio = teraz;
  const uint32_t ram = ramWolny();
  if (ram >= diag::RAM_PROG_OSTRZEZENIA_B) return;
  if (!dlawikMaloPamieci.pozwol(teraz, 60000UL)) return;
  dlawikMaloPamieci.zabierzPominiete();
  logBezPamieci("tag=PAMIEC lvl=WARN zdarzenie=malo_pamieci ram=%lu ram_blok=%lu ram_min=%lu prog=%lu\n",
                (unsigned long)ram, (unsigned long)ramBlok(), (unsigned long)ramMin(), (unsigned long)diag::RAM_PROG_OSTRZEZENIA_B);
}

// Jedna linia co PANEL_DIAG_INTERWAL_MS z odpowiedzia na trzy pytania, na
// ktore log v1.1.0 nie odpowiadal: (1) z KTORYM punktem dostepowym panel jest
// polaczony, (2) ile odpowiedzi udalo sie odczytac i ile razy dane poszly na
// ekran, (3) jakie wartosci panel ma teraz w pamieci. Zastepuje trzy linie
// [NET-DIAG] na kazde zapytanie (patrz SiecCfg::POZIOM_LOGU).
void panelDiagOkresowo() {
  static unsigned long ostatnio = 0;
  static bool pierwszaLinia = true;
  const unsigned long teraz = millis();
  if (teraz - ostatnio < (pierwszaLinia ? PANEL_DIAG_PIERWSZA_MS : PANEL_DIAG_INTERWAL_MS)) return;
  ostatnio = teraz;
  pierwszaLinia = false;

  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  char bssid[18];
  diag::formatBssid(wifiOk ? WiFi.BSSID() : nullptr, bssid, sizeof(bssid));
  char sym[72];
  diag::listaSymulowanych(centralaSymulacjaMaska, sym, sizeof(sym));
  // Wartosci t_*/wilg/cisn/dym/klapa/syberka/pompa to DOKLADNIE to, co panel
  // ma w pamieci i podaje kafelkom - do porownania z linia "tag=STATUS" w
  // logu centrali.
  // [v1.2.2 PAMIEC] ram= wolny RAM, ram_min= najmniej wolnego od startu,
  // ram_blok= najwiekszy ciagly wolny blok - wszystko dla pamieci, z ktorej
  // korzysta malloc() (patrz ramWolny()). Zastepuja heap=/heap_min=/blok_max=
  // z v1.2.0 i v1.2.1, ktore byly zawyzone o ok. 39 kB. Zdrowy panel ma tu
  // kilkadziesiat kB; ponizej ok. 16 kB zaczynaja sie klopoty.
  // logBezPamieci(): ta linia ma sie wypisac takze wtedy, gdy pamieci brakuje.
  // [v1.4.7 ZASILANIE] cpu_mhz= i tx_dbm= to ODCZYT (getCpuFrequencyMhz(),
  // WiFi.getTxPower()), tx_ok= wynik ostatniego WiFi.setTxPower() - para
  // linii tag=PANEL przed i po przelaczeniu na ekranie ZASILANIE + odczyt z
  // testera USB daje porownanie poboru pradu bez amperomierza w obwodzie.
  logBezPamieci("tag=PANEL fw=%s wifi=%d bssid=%s kanal=%d rssi=%d cpu_mhz=%lu tx_dbm=%.1f tx_ok=%d ap_przy_starcie=%d centrala=%s centrala_fw=%s sym=%s "
                "status_ok=%lu status_blad=%lu ust_ok=%lu ust_blad=%lu ekran=%lu wiek_danych_s=%ld "
                "t_zewn=%.1f t_pokoj=%.1f t_ogrz=%.1f t_ogrz_sr=%.1f t_bojler=%.1f t_panel=%.1f "
                "wilg=%.0f cisn=%.0f dym=%d klapa=%d syberka=%d pompa=%d czas=%02d:%02d "
                "ram=%lu ram_min=%lu ram_blok=%lu stos_min=%lu alok_nieudane=%lu alok_nieudana_b=%lu "
                "bez_danych_s=%lu przyczyna=%s wymuszen=%u rozl_kod=%u rozl_n=%lu autorestartow=%u\n",
                FIRMWARE_VERSION_PANEL, (int)wifiOk, bssid,
                wifiOk ? (int)WiFi.channel() : 0, wifiOk ? (int)WiFi.RSSI() : 0,
                (unsigned long)getCpuFrequencyMhz(), (double)zasilanieTxDbm(), (int)zasilanieTxOstatniWynik,
                wifiCentralPrzyStarcie,
                !centralaRozpoznana ? "nieznana" : (centralaToSymulator ? "symulator" : "sterownik"),
                stan.centralaFw[0] != '\0' ? stan.centralaFw : "-",   // [v1.4.6] pole "fw" statusu
                sym[0] != '\0' ? sym : "brak",
                (unsigned long)panelLiczniki.statusOk, (unsigned long)panelLiczniki.statusBlad,
                (unsigned long)panelLiczniki.ustawieniaOk, (unsigned long)panelLiczniki.ustawieniaBlad,
                (unsigned long)panelLiczniki.ekranOdswiezen,
                swiezoscStatusu.byloKiedykolwiek ? (long)swiezoscStatusu.wiekSekund() : -1L,
                stan.tZewn, stan.tPokoj, stan.tOgrz, stan.tOgrzSr, stan.tBojler, stan.tPanel,
                stan.wilgotnosc, stan.cisnienie, (int)stan.dym,
                stan.klapa, stan.syberka, (int)stan.pompa,
                stan.godzina, stan.minuta,
                (unsigned long)ramWolny(), (unsigned long)ramMin(), (unsigned long)ramBlok(),
                // [v1.4.1] zapas stosu petli i odmowy pamieci - opis przy alokNieudanaHak()
                (unsigned long)stosZapasMin(), (unsigned long)alokNieudanych, (unsigned long)alokNieudanaOstatniaB,
                // [v1.2.1 RATUNEK] od ilu sekund bez poprawnego statusu; czego brakuje ("-" = nic,
                // wifi | milczy | odp | pam); ile razy wymuszono ponowne polaczenie; kod i liczba rozlaczen
                // WiFi od startu; ile samoczynnych restartow od wlaczenia zasilania.
                (unsigned long)(panelRatunek.bezDanychMs / 1000UL),
                straznikSwiezosci.oznaczoneJakoStare ? diag::przyczynaSkrot(panelPrzyczynaTeraz()) : "-",
                (unsigned)panelRatunek.wymuszen,
                (unsigned)wifiOstatniKodRozlaczenia, (unsigned long)wifiLiczbaRozlaczen,
                (unsigned)rtcAutorestart.lacznie);
  panelLiczniki.zeruj();
}

// ----------------------------------------------------------------------------
// POBIERANIE DANYCH
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Tylko STARTUJE zadanie (albo nic
// nie robi, jesli poprzednie jeszcze trwa - kolejne wywolanie z loop() i tak
// nadejdzie za STATUS_INTERWAL_MS). Parsowanie odpowiedzi i caly "ogon"
// (aktualizacja sezonu/ekranu) przeniesione do obslugaStatusFetch() nizej,
// bo GET juz NIE konczy sie zanim ta funkcja wroci (patrz komentarz przy
// pendingPoKomendzie na poczatku pliku).
void pobierzStatus() {
  if (statusFetch.wTrakcie()) return;
  // [OTA-PANEL, ta sesja] Zgloszenie wlasnej wersji przy kazdym odpytaniu —
  // jedyny sposob, w jaki centrala (panel nie ma internetu) dowiaduje sie,
  // co panel realnie ma wgrane. Patrz FIRMWARE_VERSION_PANEL wyzej.
  // [RESET-DIAG, ta sesja] Doklejony tym samym mechanizmem — powod
  // ostatniego restartu, wypelniony raz na starcie setup(). Wartosci to
  // stale, bezpieczne tokeny (patrz powodResetuTekst()), bez potrzeby
  // url-encode.
  // [v1.4.0 OTA] Dochodza: identyfikator programu, z ktorego panel pracuje
  // (panel_obraz / panel_obraz_proba), a gdy centrala cos oferuje - odmowa
  // albo powod czekania. Adres sklada diag::otaSciezkaStatusu() (opis i
  // testy: diagnostyka_panelu.h); najdluzszy mozliwy ma 164 znaki.
  char sciezka[200];
  if (!diag::otaSciezkaStatusu(sciezka, sizeof(sciezka), FIRMWARE_VERSION_PANEL, globalnyPowodResetu.c_str(),
                               otaObrazWlasny, otaObrazZatwierdzony, otaZglosOdmowe, otaOdmowaNr, otaOdmowaPowod, otaCzekaPowod)) {
    // Nie zmiescilo sie (nie powinno): adres w postaci sprzed v1.4.0.
    snprintf(sciezka, sizeof(sciezka), "/api/status?panel_fw=%s&reset_powod=%.60s", FIRMWARE_VERSION_PANEL, globalnyPowodResetu.c_str());
  }
  statusGetStartMs = millis();     // [v1.4.3 B4] patrz poleZmieniane() przy "wybor"/"tryb_serwa" w obslugaStatusFetch()
  statusFetch.startGet(sciezka);   // startGet() od razu kopiuje adres do wlasnego pola
}

// [v1.4.3 A2] Filtr rozpakowywania /api/status - budowany raz, przy pierwszym
// uzyciu, potem tylko czytany. ArduinoJson 7 kopiuje do wlasnej pamieci nazwe
// kazdego pola i kazdy napis (osobna prosba o pamiec na kazde); z filtrem
// (DeserializationOption::Filter) pola spoza listy sa pomijane juz w trakcie
// czytania, bez alokacji. Lista pol jest w diagnostyka_panelu.h (test na PC
// pilnuje, ze kazde doc["..."] z obslugaStatusFetch() na niej jest).
// Koszt i zysk (testy_host/pomiar_pamieci.sh, model 32-bit): filtr trzyma na
// stale ok. 2,4 kB sterty (56 nazw pol), a szczyt pamieci przy jednym
// odczycie statusu spada z 5,3 kB do 2,7 kB - najgorsza chwila (wykres na
// ekranie + odczyt) wychodzi wiec na to samo (+0,3 kB), a prosb o pamiec
// jest o polowe mniej (5 506 -> 2 760 na minute). Odpowiedz /api/ustawienia-get celowo BEZ
// filtra: od v1.4.3 przychodzi raz na minute (co 5 s tylko na ekranie
// ustawien), wiec drugi filtr (ok. 1 kB na stale) by sie nie zwrocil.
static JsonDocument& filtrStatusu() {
  static JsonDocument filtr;
  static bool gotowy = false;
  if (!gotowy) {
    for (int i = 0; i < diag::LICZBA_POL_STATUSU_CZYTANYCH; i++) filtr[diag::POLA_STATUSU_CZYTANE[i]] = true;
    for (int i = 0; i < diag::LICZBA_CZUJNIKOW_SYMULOWANYCH; i++) filtr[diag::CZUJNIKI_SYMULOWANE[i].pole] = true;
    gotowy = true;
  }
  return filtr;
}

// [SIEC-NIEBLOKUJACA, ta sesja] Wywolywana z loop() PRZY KAZDEJ iteracji
// (tanio, gdy statusFetch bezczynny - patrz NonBlockingHttp::poll() w
// siec_nieblokujaca_LCD.h). Cialo parsowania ponizej jest DOKLADNIE dawna
// "druga polowa" pobierzStatus() - tylko teraz uruchamiana gdy GET faktycznie
// sie zakonczy, moze wiele iteracji loop() po jego wystartowaniu.
void obslugaStatusFetch() {
  statusFetch.poll();
  if (!statusFetch.gotowyWynik()) return;
  bool sukces = statusFetch.bylSukces();
  if (!sukces) {
    stan.polaczono = false;
    panelLiczniki.statusBlad++;
    ostatniBladStatusuToTresc = false;   // [v1.2.1 RATUNEK] centrala nie odpowiedziala (albo nie ma do niej drogi)
    snprintf(ostatniBladStatusuTekst, sizeof(ostatniBladStatusuTekst), "%s", statusFetch.ostatniBlad().c_str());
    ostatniBladStatusuEtap = statusFetch.etapBledu();
    ostatniBladStatusuNumer = statusFetch.numerBledu();
    // [v1.2.2 PAMIEC] socket()/connect()/send() odrzucone z ENOMEM, ENOBUFS
    // albo ENFILE: to panelowi zabraklo pamieci (albo gniazd), a nie
    // centrala milczy. Przy limitach czasu numer bledu to 0.
    ostatniBladStatusuToPamiec = diag::bladGniazdaToBrakPamieci(ostatniBladStatusuNumer);
    Serial.printf("[STATUS] GET nieudany: %s\n", ostatniBladStatusuTekst);
    // [FIX-LOG-SPAM, ta sesja] Bez tego stan_ zostawal w ERROR_ na stale -
    // gotowyWynik() (a wiec i ten log) powtarzalby sie na KAZDEJ iteracji
    // loop() az do kolejnej proby, zamiast raz na niepowodzenie. Patrz
    // obszerny komentarz przy skwitujBlad() w siec_nieblokujaca_LCD.h.
    statusFetch.skwitujBlad();
    return;
  }
  // [v1.2.0 DIAG] "sukces" warstwy sieciowej znaczy tylko, ze centrala
  // zamknela gniazdo. Zanim uznamy dane za dobre, sprawdzamy kod HTTP i
  // obecnosc ciala (diag::sprawdzOdpowiedz), a potem sam JSON - i KAZDA
  // porazke logujemy z przyczyna (v1.1.0: ciche `return`, ekran przestawal
  // sie odswiezac bez sladu w logu). JSON parsowany wprost ze wskaznika na
  // cialo w `surowa` (bez dodatkowej kopii substring() jak dotad) - `surowa`
  // zyje do konca tej funkcji, a wszystkie pola sa przepisywane do `stan`
  // ponizej, wiec nic nie wskazuje na nia po wyjsciu.
  // [v1.2.2 PAMIEC] Bez kopii odpowiedzi (bylo: pobierzOdpowiedz(), czyli
  // drugie 1,8 kB na kazde odpytanie): `surowa` to odwolanie do bufora w
  // statusFetch; zadanie wraca do stanu IDLE przy wyjsciu z tej funkcji.
  OdpowiedzDoKonca odpowiedzStatusu(statusFetch);
  const String &surowa = odpowiedzStatusu.tekst();
  int kodHttp = 0;
  const char *cialo = nullptr;
  diag::Wynik wynik = diag::sprawdzOdpowiedz(surowa.c_str(), &kodHttp, &cialo);
  JsonDocument doc;
  DeserializationError bladJson = DeserializationError::Ok;
  if (wynik == diag::WYNIK_OK) {
    bladJson = deserializeJson(doc, cialo, DeserializationOption::Filter(filtrStatusu()));   // [v1.4.3 A2]
    // is<JsonObject>(): poprawny JSON, ktory nie jest obiektem (np. sama
    // liczba), dalby same zera we wszystkich polach - to tez blad.
    if (bladJson != DeserializationError::Ok || !doc.is<JsonObject>()) wynik = diag::WYNIK_JSON;
  }
  if (wynik != diag::WYNIK_OK) {
    stan.polaczono = false;
    panelLiczniki.statusBlad++;
    ostatniBladStatusuToTresc = true;    // [v1.2.1 RATUNEK] odpowiedz przyszla, ale jest nieczytelna
    // [v1.2.2 PAMIEC] ArduinoJson 7 zwraca NoMemory tylko wtedy, gdy malloc()
    // odmowil pamieci na rozpakowanie odpowiedzi - czyli odpowiedz centrali
    // jest w porzadku, a zabraklo RAM-u w panelu (log z 2026-11-12).
    ostatniBladStatusuToPamiec = (bladJson == DeserializationError::NoMemory);
    snprintf(ostatniBladStatusuTekst, sizeof(ostatniBladStatusuTekst), "%s: %s http=%d json=%s",
             ostatniBladStatusuToPamiec ? "brak pamieci" : "odpowiedz nieczytelna",
             diag::wynikTekst(wynik), kodHttp, bladJson.c_str());
    ostatniBladStatusuEtap = ostatniBladStatusuToPamiec ? 'm' : 'o';
    ostatniBladStatusuNumer = kodHttp;
    zglosZlaOdpowiedz("STATUS", dlawikBleduStatusu, wynik, kodHttp, bladJson.c_str(), surowa, cialo);
    return;
  }
  stan.polaczono   = true;
  stan.ip          = doc["ip"].as<String>();
  stan.rssi        = doc["rssi"].as<int>();
  stan.internet    = doc["internet"].as<bool>(); // [UI-INTERNET, ta sesja]
  stan.tZewn       = doc["t_zewn"].as<float>();
  stan.tBojler     = doc["t_bojler"].as<float>();
  stan.tOgrz       = doc["t_ogrz"].as<float>();
  stan.tOgrzSr     = doc["t_ogrz_sr"].as<float>();
  stan.tPanel      = doc["t_panel"].as<float>();
  stan.tPokoj      = doc["t_pokoj"].as<float>();
  stan.tTrociny    = doc["t_trociny"].as<float>();
  stan.tPowrot     = doc["t_powrot"].as<float>();
  stan.wilgotnosc  = doc["wilgotnosc"].as<float>();
  stan.cisnienie   = doc["cisnienie"].as<float>();
  stan.dym         = doc["dym"].as<int>();
  stan.dymAlarm    = doc["dym_alarm"].as<bool>();
  stan.klapa       = doc["klapa"].as<int>();
  stan.syberka     = doc["syberka"].as<int>();
  stan.pompa       = doc["pompa"].as<bool>();
  // [v1.4.3 B4] Tryb pompy i tryb serwa sa w /api/status od dawna ("wybor",
  // "tryb_serwa") - do v1.4.2 panel bral je wylacznie z /api/ustawienia-get.
  // Teraz, gdy ustawienia w tle ida co 60 s, to status (co 2 s) daje pulpitowi
  // szybka reakcje na zmiane z telefonu / strony WWW. isNull(): starsza
  // centrala bez tych pol zostawia wartosc z ustawien. poleZmieniane(): to
  // samo co dla ustawien - odpowiedz sprzed konca wlasnego polecenia nie cofa
  // ekranu (klucze polecen: "wybor" i "trybSerwa", jak w /api/ustawienia-set).
  if (!doc["wybor"].isNull()      && !kolejkaPolecen.poleZmieniane("wybor", (uint32_t)statusGetStartMs))     trybWyboru       = doc["wybor"].as<long>();
  if (!doc["tryb_serwa"].isNull() && !kolejkaPolecen.poleZmieniane("trybSerwa", (uint32_t)statusGetStartMs)) trybSerwaWartosc = doc["tryb_serwa"].as<long>();
  // [LVGL-API] Moze nie istniec w JSON starszej centrali (przed dodaniu
  // /api/pompa-manual) - isNull() zabezpiecza tak samo jak miesiac/rtc_ok nizej.
  stan.pompaOverrideMin = doc["pompa_override_min"].isNull() ? 0 : doc["pompa_override_min"].as<int>();
  stan.alarmOgrzewanie = doc["alarm_ogrzewanie"].as<bool>();
  stan.alarmPanel      = doc["alarm_panel"].as<bool>();
  // [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Patrz obszerny komentarz przy polu
  // stan.deszczStan/deszczMinuty (definicja struct wyzej).
  stan.deszczStan    = doc["deszcz_stan"].isNull() ? 0  : doc["deszcz_stan"].as<int>();
  stan.deszczMinuty  = doc["deszcz_min"].isNull()  ? -1 : doc["deszcz_min"].as<int>();
  // [v1.5.14 POGODA-ZEWN] Centrala od 3.58.16 - patrz komentarz przy polach w struct.
  stan.pogodaKod     = doc["pogoda_kod"].isNull()      ? -1 : doc["pogoda_kod"].as<int>();
  stan.pogodaKodOpad = doc["pogoda_kod_opad"].isNull() ? -1 : doc["pogoda_kod_opad"].as<int>();
  stan.pogodaDzien   = doc["pogoda_dzien"].isNull()    ? -1 : doc["pogoda_dzien"].as<int>();
  stan.wiatrPorywy   = doc["wiatr_porywy"].isNull()    ? -1.0f : doc["wiatr_porywy"].as<float>();
  // [v1.5.17] pogoda_temp (centrala 3.58.17): liczba albo null/brak pola -> -99 (brak).
  // Nie -1: -1 °C to prawidlowa temperatura.
  stan.pogodaTemp    = doc["pogoda_temp"].isNull()     ? -99.0f : doc["pogoda_temp"].as<float>();
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Swiezy stan wszystkich 3
  // alarmow jest juz znany - przelicz, ktory wzorzec (jesli ktorykolwiek)
  // powinien teraz grac. Tanie/idempotentne, bezpieczne przy kazdym udanym
  // pobierzStatus() (patrz obszerny komentarz przy brzeczykOdswiezZAlarmow()).
  brzeczykOdswiezZAlarmow(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel);
  // [LCD-SEZON] Pola moga nie istniec w JSON-ie starszej centrali (przed
  // v1.16.0) — isNull() zabezpiecza przed nadpisaniem sensownych domyslnych
  // wartosci smieciami z ArduinoJson (0/false to i tak bezpieczne domyslne:
  // miesiac=0 => "brak RTC", wchodzi wprost w fallback na t_panel nizej).
  stan.miesiac = doc["miesiac"].isNull() ? 0 : doc["miesiac"].as<int>();
  // [LVGL-DATA-CZAS, ta sesja] Analogiczne pola dla kafelka DATA I CZAS -
  // ten sam wzorzec isNull() co miesiac/rtc_ok (centrala starsza niz ten
  // FIX po prostu nie bedzie mala tych kluczy w JSON, wiec zostana 0, co
  // dashboard_lvgl_poc.cpp i tak pokazuje jako "--" dopoki stan.rtcOk==false).
  stan.godzina = doc["godzina"].isNull() ? 0 : doc["godzina"].as<int>();
  stan.minuta  = doc["minuta"].isNull() ? 0 : doc["minuta"].as<int>();
  stan.dzien   = doc["dzien"].isNull() ? 0 : doc["dzien"].as<int>();
  stan.rok     = doc["rok"].isNull() ? 0 : doc["rok"].as<int>();
  stan.rtcOk   = doc["rtc_ok"].isNull() ? false : doc["rtc_ok"].as<bool>();
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Swieza godzina jest juz
  // znana - zapisz ja do wspolnych zmiennych i przelicz harmonogram
  // przyciemnienia (patrz obszerny komentarz przy wyswietlaczHarmonogramOdswiez()
  // wyzej). Tanie/idempotentne, bezpieczne przy kazdym udanym pobierzStatus().
  wyswietlaczOstatniaGodzina = stan.godzina;
  wyswietlaczGodzinaZnana    = stan.rtcOk;
  wyswietlaczHarmonogramOdswiez();
  // [OTA-PANEL, ta sesja] isNull() zabezpiecza tak samo jak miesiac/rtc_ok
  // wyzej — starsza centrala (przed tym dodatkiem) po prostu nie bedzie
  // mial tych pol, panel wtedy nigdy nie sprobuje sciagac OTA.
  stan.panelOtaDostepna = doc["panel_ota_dostepna"].isNull() ? false : doc["panel_ota_dostepna"].as<bool>();
  {
    // [v1.4.0 OTA] Tag wydania, identyfikator pliku i numer oferty. Pola,
    // ktorych centrala nie podaje (albo podaje w innej postaci), daja "" i 0.
    // Identyfikator jest przyjmowany tylko w poprawnej postaci (16 znakow
    // 0-9 a-f) - inaczej oferta jest traktowana jak od starszej centrali.
    const char *tagOferty = doc["panel_ota_wersja"] | "";
    const char *obrazOferty = doc["panel_ota_obraz"] | "";
    snprintf(stan.panelOtaWersja, sizeof(stan.panelOtaWersja), "%s", tagOferty);
    snprintf(stan.panelOtaObraz, sizeof(stan.panelOtaObraz), "%s", diag::otaObrazPoprawny(obrazOferty) ? obrazOferty : "");
    stan.panelOtaNr = doc["panel_ota_nr"] | (uint32_t)0;
  }

  // [v1.2.0 CENTRALA-ID] Pole "symulator":true wysyla wylacznie centrala
  // testowa (centrala-testowa-c3); prawdziwa centrala go nie ma. Patrz
  // komentarz przy centralaToSymulator na gorze pliku.
  const bool symulatorTeraz = doc["symulator"].isNull() ? false : doc["symulator"].as<bool>();
  // [v1.4.6 WERSJE] Wersja programu centrali - pole "fw" (np. "v3.58.6");
  // centrala bez tego pola daje "" (ekran: "Centrala -"). Podmiana etykiety
  // na ekranie WYSWIETLACZ tylko przy zmianie tekstu (ustawNapis()).
  {
    const char *fwCentrali = doc["fw"] | "";
    snprintf(stan.centralaFw, sizeof(stan.centralaFw), "%s", fwCentrali);
    lvgl_dashboard_poc_ustaw_wersje(FIRMWARE_VERSION_PANEL, stan.centralaFw);
  }
  bool centralaZmieniona = !centralaRozpoznana || (symulatorTeraz != centralaToSymulator);
  centralaRozpoznana = true;
  centralaToSymulator = symulatorTeraz;
  if (centralaZmieniona) {
    Serial.printf("tag=PANEL zdarzenie=centrala_rozpoznana typ=%s\n", symulatorTeraz ? "symulator" : "sterownik");
  }
  // Prawdziwa centrala: ktore czujniki sa teraz symulowane (patrz komentarz
  // przy centralaSymulacjaMaska). Wpis w logu i znacznik w naglowku TYLKO
  // przy zmianie - to takze najprostszy dowod, ze panel odczytal swiezy
  // status: po wlaczeniu symulacji w centrali napis pojawia sie tutaj w
  // ciagu jednego okresu odpytywania.
  uint16_t maskaSym = 0;
  for (int i = 0; i < diag::LICZBA_CZUJNIKOW_SYMULOWANYCH; i++) {
    // "| false": brak pola (starsza centrala) albo wartosc innego typu = nie symulowany
    if (doc[diag::CZUJNIKI_SYMULOWANE[i].pole] | false) maskaSym |= (uint16_t)(1u << i);
  }
  if (maskaSym != centralaSymulacjaMaska) {
    centralaSymulacjaMaska = maskaSym;
    centralaZmieniona = true;
    char lista[72];
    diag::listaSymulowanych(maskaSym, lista, sizeof(lista));
    Serial.printf("tag=PANEL zdarzenie=symulacja_centrali czujniki=%s\n", lista[0] != '\0' ? lista : "brak");
  }

  // [v1.2.0 DANE-NIEAKTUALNE] Dopiero TU dane sa "swieze" - po sprawdzeniu
  // kodu HTTP i JSON-a. W v1.1.0 swiezoscStatusu.odnotujSukces() bylo wolane
  // juz po samym zamknieciu gniazda, czyli takze dla odpowiedzi, ktorej nie
  // dalo sie odczytac (a sam wskaznik nie byl nigdzie uzywany).
  swiezoscStatusu.odnotujSukces();
  panelLiczniki.statusOk++;
  // [v1.2.1 RATUNEK] Sa dane - drabina ratunkowa od poczatku. Licznik
  // samoczynnych restartow z rzedu zeruje panelRatunekKrok(), dopiero gdy
  // dane sa poprawne nieprzerwanie przez diag::CZAS_STABILNYCH_DANYCH_MS.
  panelRatunek.daneSwieze();
  if (!daneDobreSeria) {
    daneDobreSeria = true;
    daneDobreOdMs = millis();
  }
  // [v1.2.1 RATUNEK] Kazde zapytanie od startu nioslo powod "auto_...", wiec
  // ta odpowiedz znaczy, ze centrala go dostala (zapisuje go i oglasza,
  // zanim odpowie). Od teraz panel zglasza zwykle "sw": centrala nie oglosi
  // tego samego restartu drugi raz po WLASNYM restarcie - chocby za tydzien.
  if (autorestartDoZgloszenia) {
    autorestartDoZgloszenia = false;
    Serial.printf("tag=PANEL zdarzenie=autorestart_zgloszony powod=%s msg=\"centrala dostala powod restartu - dalej zglaszam sw\"\n",
                  globalnyPowodResetu.c_str());
    rtcAutorestart.zgloszenieDotarlo();
    globalnyPowodResetu = "sw";
  }
  if (straznikSwiezosci.odnotujSwiezeDane()) {
    Serial.println(F("tag=PANEL zdarzenie=dane_wrocily msg=\"kafelki znow pokazuja wartosci z centrali\""));
    centralaZmieniona = true;   // [v1.2.1] naglowek: zamiast "CENTRALA MILCZY 14 s" znow zwykly znacznik centrali
  }

  // [SIEC-NIEBLOKUJACA, ta sesja] Ponizszy "ogon" byl dawniej doklejany
  // OSOBNO przez KAZDEGO wywolujacego pobierzStatus() (petla loop() w obu
  // galeziach + 4 mosty LVGL: pompaManualWyslij/pompaWyborWyslij/
  // serwoTrybWyslij/serwoPozycjaWyslij) - scentralizowany tutaj RAZ, bo byl
  // identyczny wszedzie w danej galezi, a aktualizujSezon()/
  // lvgl_dashboard_poc_aktualizuj() sa tanie/idempotentne
  // (bezpieczne przy kazdym udanym odswiezeniu, niezaleznie kto je sprowokowal).
  aktualizujSezon(); // patrz komentarz [FIX, Partia 2] przy pierwotnym wywolaniu w setup()
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 diag::serwoStopnieNaProcent(stan.klapa, diag::KLAPA_KAT_MAKS), diag::serwoStopnieNaProcent(stan.syberka, diag::SYBERKA_KAT_MAKS), (int)trybWyboru,   // [v1.5.6] centrala podaje stopnie
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty,
                                 stan.pogodaKod, stan.pogodaKodOpad, stan.pogodaDzien, stan.wiatrPorywy,   // [v1.5.14]
                                 stan.pogodaTemp);   // [v1.5.17]
  panelLiczniki.ekranOdswiezen++;
  if (centralaZmieniona) lvglPokazZnacznikCentrali(); // [v1.2.0 CENTRALA-ID] "SYMULATOR" / "2 CENTRALE (..)" w naglowku
}

// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Tylko STARTUJE zadanie - patrz
// komentarz przy pobierzStatus() wyzej, ten sam wzorzec.
void pobierzUstawienia() {
  if (ustawieniaFetch.wTrakcie()) {
    ustawieniaGetPonow = true;   // [v1.4.2 C4] nie gub prosby - GET ruszy zaraz po biezacym
    return;
  }
  ustawieniaGetStartMs = millis();
  rytmUstawien.odnotujGet((uint32_t)ustawieniaGetStartMs);   // [v1.4.3 B4] okres w tle liczony od tego GET-a
  ustawieniaFetch.startGet("/api/ustawienia-get");
}

// Wywolywana z loop() przy kazdej iteracji - patrz obslugaStatusFetch() wyzej.
// [v1.4.2 C4] Cienka otoczka: po KAZDYM zakonczonym GET (udanym czy nie),
// jesli w miedzyczasie ktos prosil o kolejny (pobierzUstawienia() w trakcie
// GET-a), startuje go od razu.
static void obslugaUstawieniaFetchWlasciwa();
void obslugaUstawieniaFetch() {
  ustawieniaFetch.poll();
  if (!ustawieniaFetch.gotowyWynik()) return;
  obslugaUstawieniaFetchWlasciwa();
  if (ustawieniaGetPonow && !ustawieniaFetch.wTrakcie()) {
    ustawieniaGetPonow = false;
    pobierzUstawienia();
  }
}

static void obslugaUstawieniaFetchWlasciwa() {
  bool sukces = ustawieniaFetch.bylSukces();
  if (!sukces) {
    panelLiczniki.ustawieniaBlad++;
    Serial.printf("[USTAWIENIA] GET nieudany: %s\n", ustawieniaFetch.ostatniBlad().c_str());
    // [FIX-LOG-SPAM, ta sesja] Patrz identyczny komentarz w
    // obslugaStatusFetch() wyzej / skwitujBlad() w siec_nieblokujaca_LCD.h.
    ustawieniaFetch.skwitujBlad();
    return;   // zostaw ostatnie znane wartosci
  }
  // [v1.2.0 DIAG] Ten sam wzorzec co w obslugaStatusFetch() wyzej: kod HTTP
  // + cialo + JSON, kazda porazka z przyczyna w logu (bylo: ciche `return`).
  // [v1.2.2 PAMIEC] Bez kopii odpowiedzi - jak w obslugaStatusFetch().
  OdpowiedzDoKonca odpowiedzUstawien(ustawieniaFetch);
  const String &surowa = odpowiedzUstawien.tekst();
  int kodHttp = 0;
  const char *cialo = nullptr;
  diag::Wynik wynik = diag::sprawdzOdpowiedz(surowa.c_str(), &kodHttp, &cialo);
  JsonDocument doc;
  DeserializationError bladJson = DeserializationError::Ok;
  if (wynik == diag::WYNIK_OK) {
    bladJson = deserializeJson(doc, cialo);   // bez filtra - celowo, patrz komentarz przy filtrStatusu()
    if (bladJson != DeserializationError::Ok || !doc.is<JsonObject>()) wynik = diag::WYNIK_JSON;
  }
  if (wynik != diag::WYNIK_OK) {
    panelLiczniki.ustawieniaBlad++;
    zglosZlaOdpowiedz("USTAWIENIA", dlawikBleduUstawien, wynik, kodHttp, bladJson.c_str(), surowa, cialo);
    return;   // zostaw ostatnie znane wartosci
  }
  swiezoscUstawien.odnotujSukces();
  panelLiczniki.ustawieniaOk++;
  // [v1.4.2 C4] Pole, ktore uzytkownik wlasnie zmienia (polecenie w kolejce,
  // w locie albo zakonczone juz po starcie tego GET-a), zostaje z wartoscia
  // z ekranu - odpowiedz niesie dla niego stara wartosc. Po ostatnim
  // poleceniu serii rusza nowy GET i wtedy wartosc centrali jest przyjmowana.
  char pominiete[64] = "";
  int pominietych = 0;
  for (int i = 0; i < LICZBA_POL; i++) {
    if (doc[POLA[i].klucz].isNull()) continue;
    if (kolejkaPolecen.poleZmieniane(POLA[i].klucz, (uint32_t)ustawieniaGetStartMs)) {
      if (pominietych++ < 3) snprintf(pominiete + strlen(pominiete), sizeof(pominiete) - strlen(pominiete), "%s%s", pominietych > 1 ? "," : "", POLA[i].klucz);
      continue;
    }
    POLA[i].wartosc = doc[POLA[i].klucz].as<long>();
  }
  // [LCD-TRYBY] Poza POLA[] — patrz komentarz przy deklaracji trybWyboru/
  // trybSerwaWartosc wyzej.
  if (!doc["wybor"].isNull()     && !kolejkaPolecen.poleZmieniane("wybor", (uint32_t)ustawieniaGetStartMs))     trybWyboru      = doc["wybor"].as<long>();
  if (!doc["trybSerwa"].isNull() && !kolejkaPolecen.poleZmieniane("trybSerwa", (uint32_t)ustawieniaGetStartMs)) trybSerwaWartosc = doc["trybSerwa"].as<long>();
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] Ten sam wzorzec
  // co wybor/trybSerwa powyzej - bool, wiec poza POLA[].
  if (!doc["antystopWlaczony"].isNull() && !kolejkaPolecen.poleZmieniane("antystopWlaczony", (uint32_t)ustawieniaGetStartMs))
    antystopWlaczonyWartosc = doc["antystopWlaczony"].as<bool>();
  if (pominietych > 0) {
    Serial.printf("tag=KOMENDA zdarzenie=stara_wartosc_pominieta pol=%d pola=%s msg=\"GET ustawien wystartowal przed koncem polecenia - pola zmieniane zostaja z ekranu\"\n",
                  pominietych, pominiete);
  }
  ustawieniaWczytane = true;
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA SERWA] Przekaz swiezo
  // pobrane wartosci dalej do dashboard_lvgl_poc.cpp - ten sam wzorzec co
  // lvgl_dashboard_poc_aktualizuj() dla /api/status, tylko osobna funkcja
  // (patrz obszerny komentarz w dashboard_lvgl_poc.h) bo to inne zrodlo
  // (/api/ustawienia-get, nie /api/status) i inna czestotliwosc odswiezania.
  lvgl_dashboard_poc_aktualizuj_ustawienia_serwa(
      (int)polaWartosc("tempZadServo"),
      (int)polaWartosc("histerServo"),
      (int)polaWartosc("skokKlapy"),
      (int)polaWartosc("skokSyberka"),
      (int)polaWartosc("odchylTemp"),
      (int)polaWartosc("mnoznik"));
  // [ROZSZERZENIE, ta sesja - ekran LVGL USTAWIENIA POMPY] Ten sam wzorzec co
  // wywolanie serwa powyzej, tylko dla dashboard_lvgl_poc.h ustawien pompy -
  // dwie osobne funkcje (5 pol numerycznych + osobno bool antystopWlaczony),
  // patrz uzasadnienie w dashboard_lvgl_poc.h.
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy(
      (int)polaWartosc("czasOn"),
      (int)polaWartosc("czasOff"),
      (int)polaWartosc("tempOn"),
      (int)polaWartosc("tempOff"),
      (int)polaWartosc("antystopDni"));
  lvgl_dashboard_poc_aktualizuj_ustawienia_pompy_antystop(antystopWlaczonyWartosc);
  // [ROZSZERZENIE, ta sesja - PANEL DYM] Ten sam wzorzec co wywolania
  // serwa/pompy powyzej. progAlarmDym juz byl w POLA[] (stary EKRAN_TRYBY go
  // uzywal), dymCzasOn/Off/Stabilizacji dopisane wyzej w tej sesji.
  lvgl_dashboard_poc_aktualizuj_ustawienia_dymu(
      (int)polaWartosc("progAlarmDym"),
      (int)polaWartosc("dymCzasOn"),
      (int)polaWartosc("dymCzasOff"),
      (int)polaWartosc("dymCzasStabilizacji"));
  // [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA] progAlarmTemp juz byl
  // w POLA[] od dawna (stary EKRAN_TRYBY go uzywal) - tylko dopisujemy
  // przekazanie dalej do LVGL, ten sam wzorzec co progAlarmDym powyzej.
  lvgl_dashboard_poc_aktualizuj_ustawienia_alarmu_temp(
      (int)polaWartosc("progAlarmTemp"));
  // Nie pobieramy tu klapy/syberka recznych procentow celowo — te sa juz
  // widoczne w /api/status (klapa/syberka), a suwak na ekranie Serwo
  // zaczyna od tych samych wartosci co status przy pierwszym wejsciu na ekran.
}

// ----------------------------------------------------------------------------
// WYSYLANIE ZMIAN DO CENTRALI (mosty LVGL -> kolejka polecen -> HTTP POST)
// ----------------------------------------------------------------------------
// Wysyla zmiane trybu tym samym generycznym /api/ustawienia-set co POLA[]
// — patrz siec_nieblokujaca_LCD.h.
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Woluje ja 8 mostow LVGL (patrz
// komentarze tam) - WSZYSCY oprocz pompaWyborWyslij()/serwoTrybWyslij() chca
// po zakonczeniu POST-a dokladnie tego samego: pobierzUstawienia() ("pokaz
// PRAWDZIWA, zwalidowana przez centrale wartosc") - wiec USTAWIENIA jest
// domyslna wartoscia `po`; te dwa wyjatki
// podaja STATUS jawnie. [v1.4.2 C4] Do v1.4.1 nadpisywaly globalna flage
// pendingPoKomendzie zaraz po wywolaniu - z kolejka flaga wedruje razem z
// poleceniem (diag::Polecenie::po), wiec musi byc parametrem.
void wyslijTrybPracy(const char* pole, long wartosc, PoKomendzie po) {
  // [v1.4.2 C4] Do kolejki zamiast "pomijam" - patrz poleceniePlanuj().
  // Dawny czerwony pasek na TFT + delay(1200) na porazke ZNIKNAL juz
  // wczesniej (blokowal petle); od v1.4.2 porazka jest pokazywana paskiem
  // LVGL przez 3 s (lvgl_dashboard_poc_pokaz_blad_polecenia()), a ekran i
  // tak zaraz pokaze prawdziwa wartosc, bo pobierzUstawienia() leci po
  // ostatnim poleceniu serii w obu przypadkach - patrz obslugaKomendaFetch().
  // [v1.4.2 C4] Kopia ustawien w panelu (POLA[]) idzie za palcem od razu.
  // Bez tego odczyt ustawien, ktory pominie to pole jako "zmieniane"
  // (obslugaUstawieniaFetch()), przekazalby ekranom STARA wartosc z POLA[]
  // i cofnal liczbe pod palcem - dokladnie to, co ta poprawka ma usunac.
  // Po ostatnim poleceniu serii swiezy odczyt i tak nadpisze POLA[] tym, co
  // centrala naprawde zapisala.
  const int idx = znajdzIndeksPola(pole);
  if (idx >= 0) POLA[idx].wartosc = wartosc;
  String cialo = String("pole=") + pole + "&wartosc=" + String(wartosc);
  poleceniePlanuj("/api/ustawienia-set", pole, cialo.c_str(), po);
}

// [v1.4.2 C4] Jedyne miejsce, ktore dodaje polecenia do kolejki. Gdy kanal
// jest wolny, a dla tego pola nic nie czeka, POST rusza od razu (jak do
// v1.4.1) - opoznienie dotyczy tylko sytuacji, w ktorych do v1.4.1
// polecenie by PRZEPADLO. Pelna kolejka (6 roznych pol naraz, w praktyce
// nieosiagalne) = polecenie odrzucone z komunikatem na ekranie.
void poleceniePlanuj(const char* sciezka, const char* klucz, const char* cialo, PoKomendzie po) {
  if (!kolejkaPolecen.dodaj(sciezka, klucz, cialo, (uint8_t)po, millis())) {
    Serial.printf("tag=KOMENDA lvl=WARN zdarzenie=kolejka_pelna pole=%s cialo=\"%s\" odrzuconych=%lu\n",
                  klucz, cialo, (unsigned long)kolejkaPolecen.odrzuconych);
    lvgl_dashboard_poc_pokaz_blad_polecenia("ZA DUZO POLECEN NARAZ - POWTORZ");
    return;
  }
  poleceniaKrok();
}

// [v1.4.2 C4] Wysyla pierwsze polecenie z kolejki, gdy kanal HTTP jest wolny
// i polecenie jest "gotowe" (diag::KolejkaPolecen::gotowa(): wartosc
// zmieniana w kolejce czeka 200 ms ciszy - przytrzymany przycisk konczy sie
// jednym POST-em). Wolane z loop() co obrot i z poleceniePlanuj().
void poleceniaKrok() {
  if (komendaFetch.wTrakcie()) return;
  if (!kolejkaPolecen.gotowa(millis())) return;
  const diag::Polecenie& p = kolejkaPolecen.wystartuj();
  pendingPoKomendzie = (PoKomendzie)p.po;
  komendaFetch.startPostFormUrlEncoded(p.sciezka, String(p.cialo));
}

// ----------------------------------------------------------------------------
// WYSYLANIE ZMIAN
// ----------------------------------------------------------------------------
// [SIEC-NIEBLOKUJACA, ta sesja - PRZEPISANE] Wolana przez serwoPozycjaWyslij()
// z mostow LVGL (ktora podaje STATUS); domyslnie (po = BRAK) NIC nie odswieza
// po zakonczeniu - w odroznieniu od wyslijTrybPracy().
void wyslijServoManualny(const char* os, int procent, PoKomendzie po) {
  // Blad tutaj czesto oznacza po prostu "tryb serwa != Reczny" — nie
  // przerywamy uzytkownikowi obslugi suwaka (dokladnie tak jak panel WWW,
  // ktory suwak i tak pokazuje tylko w trybie recznym; tutaj panel jest
  // zawsze widoczny, wiec blad jest oczekiwany). [v1.4.2 C4] Kolejka scala
  // kolejne pozycje tego samego suwaka - przeciagniecie konczy sie jednym
  // POST-em z pozycja koncowa.
  String cialo = String("os=") + os + "&procent=" + String(procent);
  poleceniePlanuj("/api/servo-manual", os, cialo.c_str(), po);
}

// [SIEC-NIEBLOKUJACA, ta sesja] Wywolywana z loop() przy kazdej iteracji -
// patrz obslugaStatusFetch() wyzej. Tu POST i "nastepujacy po nim GET"
// (dawniej jedno synchroniczne wywolanie) faktycznie spinaja sie z powrotem
// w jedna calosc - patrz obszerny komentarz przy pendingPoKomendzie na
// poczatku pliku co do PRZYCZYNY, dla ktorej to nie moze juz byc proste.
void obslugaKomendaFetch() {
  komendaFetch.poll();
  if (!komendaFetch.gotowyWynik()) return;
  bool centralaOk = false;
  const bool odpowiedziala = komendaFetch.bylSukces();   // [v1.4.2 C4] zanim pobierzOdpowiedz()/skwitujBlad() wyzeruja stan
  char powodOdrzucenia[40] = "";
  if (odpowiedziala) {
    panelRatunek.centralaOdezwalaSie();   // [v1.5.7 RATUNEK] odpowiedz na polecenie = centrala zyje

    // centrala odpowiada {"ok":true,...} albo {"ok":false,"error":"..."} -
    // dokladnie ten sam kontrakt, ktory dawny httpPost() sprawdzal.
    String odp = NonBlockingHttp::wyodrebnijCialo(komendaFetch.pobierzOdpowiedz());
    JsonDocument doc;
    if (deserializeJson(doc, odp) == DeserializationError::Ok) {
      centralaOk = doc["ok"].as<bool>();
      if (!centralaOk) {
        const char* blad = doc["error"].as<const char*>();
        Serial.printf("[KOMENDA] centrala odrzucila: %s\n", blad != nullptr ? blad : "");
        // [v1.4.2 C4] Powod z centrali na ekran, jesli krotki i bez polskich
        // liter (czcionka LVGL ich nie ma): np. "tryb serwa nie jest reczny".
        snprintf(powodOdrzucenia, sizeof(powodOdrzucenia), "%s", blad != nullptr ? blad : "");
        for (const char* c = powodOdrzucenia; *c != '\0'; c++) if ((unsigned char)*c >= 0x80) { powodOdrzucenia[0] = '\0'; break; }
      }
    }
  } else {
    Serial.printf("[KOMENDA] POST nieudany: %s\n", komendaFetch.ostatniBlad().c_str());
    // [v1.2.0 FIX-LOG-SPAM-KOMENDA] Ten sam blad co naprawiony wczesniej w
    // obslugaStatusFetch()/obslugaUstawieniaFetch() (patrz FIX-LOG-SPAM przy
    // skwitujBlad() w siec_nieblokujaca_LCD.h) - tutaj zostal przeoczony.
    // Bez skwitujBlad() komendaFetch zostawal w stanie ERROR_ az do NASTEPNEGO
    // dotkniecia przycisku, wiec gotowyWynik() byl prawda na kazdym obrocie
    // loop() i powyzsza linia szla na Serial przy kazdym z nich, czyli wiele
    // razy na sekunde - po JEDNYM nieudanym POST, np. po dotknieciu przycisku
    // w chwili zaniku WiFi.
    komendaFetch.skwitujBlad();
  }

  // [v1.4.2 C4] Zakonczone polecenie (udane czy nie) zapamietane z czasem -
  // GET ustawien, ktory wystartowal wczesniej, nie nadpisze tego pola stara
  // wartoscia (kolejkaPolecen.poleZmieniane() w obslugaUstawieniaFetch()).
  const diag::Polecenie zakonczone = kolejkaPolecen.wLocie;
  // [v1.5.5] Bez odpowiedzi centrali (nie: odrzucone) - jeszcze raz, do
  // PROB_MAKS prob; porazke pokazujemy dopiero po ostatniej.
  if (!odpowiedziala && kolejkaPolecen.ponow(millis())) {
    Serial.printf("tag=KOMENDA zdarzenie=ponowienie pole=%s cialo=\"%s\" proba=%u/%u powod=%s\n",
                  zakonczone.klucz, zakonczone.cialo, (unsigned)(zakonczone.proba + 2), (unsigned)diag::KolejkaPolecen::PROB_MAKS,
                  komendaFetch.ostatniBlad().c_str());
    return;   // pendingPoKomendzie zostaje - GET-y pojda po ostatniej probie
  }
  kolejkaPolecen.zakoncz(millis());
  if (!centralaOk) {
    // Uzytkownik musi to zobaczyc: ekran pokazuje wartosc, ktorej centrala
    // nie ma. Do v1.4.1 porazka szla tylko do logu, a ekran "cofal" liczbe
    // przy nastepnym GET bez slowa wyjasnienia.
    Serial.printf("tag=KOMENDA lvl=WARN zdarzenie=polecenie_nieudane pole=%s cialo=\"%s\" powod=%s prob=%u\n",
                  zakonczone.klucz, zakonczone.cialo, odpowiedziala ? "odrzucone" : "brak_odpowiedzi", (unsigned)(zakonczone.proba + 1));
    char komunikat[56];
    if (!odpowiedziala)                 snprintf(komunikat, sizeof(komunikat), "POLECENIE NIE DOSZŁO DO CENTRALI");
    else if (powodOdrzucenia[0] != '\0') snprintf(komunikat, sizeof(komunikat), "ODRZUCONE: %s", powodOdrzucenia);
    else                                snprintf(komunikat, sizeof(komunikat), "CENTRALA ODRZUCIŁA POLECENIE");
    lvgl_dashboard_poc_pokaz_blad_polecenia(komunikat);
  }

  PoKomendzie co = pendingPoKomendzie;
  pendingPoKomendzie = PoKomendzie::BRAK;
  if (co == PoKomendzie::USTAWIENIA) partiaUstawienia = true;
  if (co == PoKomendzie::STATUS) partiaStatus = true;
  // [v1.4.2 C4] Nastepne polecenie z kolejki od razu (jesli gotowe) - GET-y
  // dopiero po ostatnim poleceniu serii. Kolejka moze byc niepusta, a
  // polecenie niegotowe (czeka na cisze po przytrzymanym przycisku) - wtedy
  // odswiezenie tez czeka: pole i tak byloby pominiete jako "zmieniane".
  poleceniaKrok();
  if (!kolejkaPolecen.pusta() || komendaFetch.wTrakcie()) return;
  if (partiaUstawienia) {
    // Zawsze, niezaleznie od centralaOk - sukces potwierdza nowa wartosc,
    // porazka przywraca ostatnia znana (oba dawne zachowania na raz, patrz
    // komentarze w wyslijTrybPracy() wyzej).
    partiaUstawienia = false;
    pobierzUstawienia();
  }
  if (partiaStatus) {
    // STATUS: do v1.4.1 tylko po sukcesie; od v1.4.2 tez po porazce - ekran
    // ma wtedy tym bardziej pokazac prawdziwy stan, a GET statusu jest tani
    // (i tak leci co STATUS_INTERWAL_MS).
    partiaStatus = false;
    pobierzStatus();
  }
}

// [SIEC-NIEBLOKUJACA, ta sesja] JEDYNE miejsce w tym pliku, gdzie SWIADOMIE
// czekamy na siec - wywolywana WYLACZNIE raz, z setup() (w obu galeziach),
// zaraz po polaczWifi()+pierwszym pobierzStatus()/pobierzUstawienia().
// Na starcie nie ma jeszcze zadnego dotyku do zamrozenia, wiec krotkie,
// OGRANICZONE oczekiwanie tutaj jest bezpieczne - w przeciwienstwie do
// dawnego httpGet()/httpPost(), ktore blokowaly tak samo w KAZDEJ iteracji
// loop() (czyli tez w trakcie normalnej pracy, z dotykiem czekajacym).
// Bez tego pierwszy lvgl_dashboard_poc_aktualizuj()
// dostalby same zera/domyslne wartosci zamiast prawdziwych danych z centrali
// (do v1.3.0 takze sprawdzIWykonajOtaPanelu(); od v1.4.0 oferte aktualizacji
// sprawdza otaKrok() w loop()). Pompuje poll() tak samo
// jak zrobilaby to petla loop(), tylko w petli while zamiast czekac na
// kolejna iteracje - do PIERWSZA_PROBKA_TIMEOUT_MS, zeby calkiem
// nieosiagalna centrala (np. pierwsze uruchomienie bez pary) nie zawiesila
// setup() na dluzej niz ten jeden, jawny limit.
static const uint32_t PIERWSZA_PROBKA_TIMEOUT_MS = 2500; // troche wiecej niz SiecCfg::CALKOWITY_TIMEOUT_MS (1500) w siec_nieblokujaca_LCD.h - status i ustawienia leca rownolegle, ale dajemy zapas
void poczekajNaPierwszeDane() {
  unsigned long start = millis();
  while ((statusFetch.wTrakcie() || ustawieniaFetch.wTrakcie()) &&
         millis() - start < PIERWSZA_PROBKA_TIMEOUT_MS) {
    obslugaStatusFetch();
    obslugaUstawieniaFetch();
    delay(5);
  }
}

// ----------------------------------------------------------------------------
// OBSLUGA DOTYKU
// ----------------------------------------------------------------------------
void kalibrujDotykJesliTrzeba() {
  kalibracja.begin("dotyk", false);
  bool zapisana = kalibracja.isKey("rawXA");
  if (zapisana) {
    kalDotyku.rawXA = kalibracja.getUShort("rawXA", 0);
    kalDotyku.rawYA = kalibracja.getUShort("rawYA", 0);
    kalDotyku.rawXB = kalibracja.getUShort("rawXB", 0);
    kalDotyku.rawYB = kalibracja.getUShort("rawYB", 0);
    // [v1.5.2] Orientacja, w ktorej zrobiono te kalibracje (panele sprzed
    // v1.5.2 nie maja klucza - byly kalibrowane w obrocie 1).
    const uint8_t obrotKalibracji = kalibracja.getUChar("obrot", 1);
    dotykOdwrocony = diag::dotykTrzebaOdwrocic(obrotKalibracji, EKRAN_OBROT);
    Serial.printf("tag=DOTYK zdarzenie=kalibracja_z_pamieci obrot_kalibracji=%u obrot_ekranu=%d odwrocony=%d\n",
                  obrotKalibracji, EKRAN_OBROT, dotykOdwrocony ? 1 : 0);
  } else {
    tft.fillScreen(KOL_TLO);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(KOL_TEKST, KOL_TLO);
    tft.setTextFont(2);
    tft.drawString("Kalibracja dotyku", 240, 160);
    tft.drawString("Dotknij i przytrzymaj znacznik", 240, 185);
    Serial.println("[kalibracja] start - punkt A (gorny-lewy)");

    // [v1.4.1 C3] Watchdog petli (15 s, patrz setup()) jest juz wlaczony, a
    // czlowiek czytajacy napisy na ekranie potrzebuje wiecej czasu. Kazda
    // petla czekania na palec / na puszczenie karmi watchdoga, inaczej panel
    // bez zapisanej kalibracji restartowalby sie w kolko (powod task_wdt).
    auto czekajChwile = []() { delay(10); esp_task_wdt_reset(); };

    bool udana = false;
    while (!udana) {
      // --- znacznik A: gorny-lewy ---
      tft.drawCircle(KAL_A_X, KAL_A_Y, 8, TFT_MAGENTA);
      uint16_t rx, ry, sx, sy;
      bool zlapano = false;
      while (!zlapano) {
        while (!touchOdczytajSurowy(rx, ry)) { czekajChwile(); }
        zlapano = touchPobierzStabilnaPozycje(sx, sy);
        if (!zlapano) Serial.println("[kalibracja] falszywy alarm (dotyk znikl w trakcie) - probuj ponownie");
      }
      kalDotyku.rawXA = sx;
      kalDotyku.rawYA = sy;
      while (touchOdczytajSurowy(rx, ry)) { czekajChwile(); }  // czekaj na puszczenie
      tft.drawCircle(KAL_A_X, KAL_A_Y, 8, KOL_TLO);
      Serial.printf("[kalibracja] punkt A zapisany: x=%u y=%u\n", kalDotyku.rawXA, kalDotyku.rawYA);
      Serial.println("[kalibracja] punkt B (dolny-prawy)");

      delay(300);

      // --- znacznik B: dolny-prawy ---
      tft.drawCircle(KAL_B_X, KAL_B_Y, 8, TFT_MAGENTA);
      zlapano = false;
      while (!zlapano) {
        while (!touchOdczytajSurowy(rx, ry)) { czekajChwile(); }
        zlapano = touchPobierzStabilnaPozycje(sx, sy);
        if (!zlapano) Serial.println("[kalibracja] falszywy alarm (dotyk znikl w trakcie) - probuj ponownie");
      }
      kalDotyku.rawXB = sx;
      kalDotyku.rawYB = sy;
      while (touchOdczytajSurowy(rx, ry)) { czekajChwile(); }
      tft.drawCircle(KAL_B_X, KAL_B_Y, 8, KOL_TLO);
      Serial.printf("[kalibracja] punkt B zapisany: x=%u y=%u\n", kalDotyku.rawXB, kalDotyku.rawYB);

      // --- WALIDACJA: oba punkty musza sie realnie roznic, inaczej map() ---
      // --- dzieli przez zero i caly dotyk pozniej jest zepsuty           ---
      const int MIN_ROZNICA = 300;   // minimalna sensowna odleglosc raw miedzy punktami
      int roznicaX = abs((int)kalDotyku.rawXB - (int)kalDotyku.rawXA);
      int roznicaY = abs((int)kalDotyku.rawYB - (int)kalDotyku.rawYA);
      if (roznicaX < MIN_ROZNICA || roznicaY < MIN_ROZNICA) {
        Serial.printf("[kalibracja] BLAD: punkty za blisko siebie (dX=%d dY=%d, min=%d) - powtarzam\n",
                      roznicaX, roznicaY, MIN_ROZNICA);
        tft.fillScreen(KOL_TLO);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(KOL_ERR, KOL_TLO);
        tft.drawString("Kalibracja nieudana - powtarzam", 240, 160);
        tft.setTextColor(KOL_TEKST, KOL_TLO);
        tft.drawString("Dotknij mocniej i przytrzymaj chwile", 240, 185);
        delay(1200);
        esp_task_wdt_reset();
        tft.fillScreen(KOL_TLO);
        tft.drawString("Kalibracja dotyku", 240, 160);
        tft.drawString("Dotknij i przytrzymaj znacznik", 240, 185);
        udana = false;
      } else {
        udana = true;
      }
    }

    kalibracja.putUShort("rawXA", kalDotyku.rawXA);
    kalibracja.putUShort("rawYA", kalDotyku.rawYA);
    kalibracja.putUShort("rawXB", kalDotyku.rawXB);
    kalibracja.putUShort("rawYB", kalDotyku.rawYB);
    kalibracja.putUChar("obrot", EKRAN_OBROT);   // [v1.5.2] znaczniki byly rysowane w tej orientacji
    dotykOdwrocony = false;
  }
  kalibracja.end();
}

// ----------------------------------------------------------------------------
// WIFI
// ----------------------------------------------------------------------------
// [v1.2.0 WIFI-WYBOR] Wpis do logu: z KTORYM punktem dostepowym panel jest
// polaczony. BSSID to adres MAC nadajnika centrali - jedyne, czym od strony
// sieci rozni sie prawdziwa centrala od centrali testowej (obie: ta sama
// nazwa sieci, to samo haslo, ten sam adres 192.168.4.1).
static void wifiZalogujPolaczenie(const char *zdarzenie) {
  char bssid[18];
  diag::formatBssid(WiFi.BSSID(), bssid, sizeof(bssid));
  Serial.printf("tag=WIFI zdarzenie=%s bssid=%s kanal=%d rssi=%d\n",
                zdarzenie, bssid, (int)WiFi.channel(), (int)WiFi.RSSI());
}

// [v1.2.0 WIFI-WYBOR] Skanuje wszystkie kanaly i wybiera NAJSILNIEJSZY punkt
// dostepowy o nazwie WIFI_SSID. Zwraca liczbe znalezionych sieci o tej
// nazwie (0 = zadnej albo skan sie nie udal; `wybrany` jest wtedy nietkniety).
//
// POWOD: v1.1.0 wolal WiFi.begin(ssid, haslo) bez zadnych ustawien, a
// domyslna metoda w arduino-esp32 to WIFI_FAST_SCAN (WiFiSTA.cpp: "First
// SSID match [WIFI_FAST_SCAN] or Sorted [WIFI_ALL_CHANNEL_SCAN]") - sterownik
// laczy sie z PIERWSZA siecia o pasujacej nazwie, jaka znajdzie, a nie z
// najsilniejsza. Gdy w zasiegu sa dwie centrale naraz (prawdziwa przy piecu
// + testowa na biurku - obie nadaja te sama nazwe sieci, z tym samym haslem
// i adresem 192.168.4.1), panel po wlaczeniu zasilania moze trafic na
// dowolna z nich, niezaleznie od tego, ktora stoi obok - i wtedy "kafelki
// nie reaguja na zmiany w centrali", bo pokazuja dane DRUGIEJ.
// To jedna z HIPOTEZ dla zgloszenia z 2026-11-10 (niepotwierdzona: log z
// tamtego dnia nie zawiera BSSID). v1.2.0 usuwa te mozliwosc i wypisuje
// BSSID, wiec nastepny log to rozstrzygnie.
//
// Skan ASYNCHRONICZNY + wlasne czekanie z limitem (a nie blokujace
// WiFi.scanNetworks()): wersja blokujaca czeka w rdzeniu bez karmienia
// watchdoga do 60 s (WiFiScanClass::_scanTimeout w arduino-esp32 3.x), a
// loopTask jest pod watchdogiem 15 s. Sam skan to 13 kanalow x 100-150 ms,
// czyli ok. 1,5-2 s (z ustawien skanu - niezmierzone na plytce); dzieje sie
// tylko raz, przy zimnym starcie, gdy ekran i tak pokazuje komunikat o
// laczeniu.
static int wifiSkanujCentrale(diag::KandydatAp &wybrany) {
  static const int MAKS_KANDYDATOW = 6;
  static const uint32_t SKAN_LIMIT_MS = 6000;
  diag::KandydatAp kandydaci[MAKS_KANDYDATOW];
  int liczba = 0;

  WiFi.scanDelete();
  int znalezione = WiFi.scanNetworks(true /*async*/, false /*ukryte*/, false /*pasywny*/, 150 /*ms na kanal*/);
  if (znalezione == WIFI_SCAN_RUNNING) {
    const unsigned long startSkanu = millis();
    while ((znalezione = WiFi.scanComplete()) == WIFI_SCAN_RUNNING && millis() - startSkanu < SKAN_LIMIT_MS) {
      esp_task_wdt_reset();
      obsluzBrzeczyk();   // [v1.2.1] alarm przywrocony po samoczynnym restarcie ma grac swoim rytmem takze tutaj
      delay(50);
    }
    esp_task_wdt_reset();
  }
  if (znalezione < 0) {
    // Skan sie nie udal albo nie zmiescil w limicie SKAN_LIMIT_MS (radio
    // moze wtedy skanowac dalej). Przerwij go, zeby nie kolidowal z
    // WiFi.begin() zaraz po powrocie z tej funkcji, i daj zadaniu zdarzen
    // WiFi chwile na obsluzenie SCAN_DONE, zanim scanDelete() nizej zwolni
    // wyniki.
    esp_wifi_scan_stop();
    delay(100);
    znalezione = WIFI_SCAN_FAILED;
  }

  for (int i = 0; i < znalezione && liczba < MAKS_KANDYDATOW; i++) {
    if (WiFi.SSID((uint8_t)i) != WIFI_SSID) continue;
    const uint8_t *b = WiFi.BSSID((uint8_t)i);
    if (b == nullptr) continue;
    memcpy(kandydaci[liczba].bssid, b, 6);
    kandydaci[liczba].kanal = WiFi.channel((uint8_t)i);
    kandydaci[liczba].rssi = WiFi.RSSI((uint8_t)i);
    liczba++;
  }
  WiFi.scanDelete();

  for (int i = 0; i < liczba; i++) {
    char bssid[18];
    diag::formatBssid(kandydaci[i].bssid, bssid, sizeof(bssid));
    Serial.printf("tag=WIFI zdarzenie=skan_centrala nr=%d/%d bssid=%s kanal=%d rssi=%d\n",
                  i + 1, liczba, bssid, (int)kandydaci[i].kanal, (int)kandydaci[i].rssi);
  }
  if (liczba == 0) {
    // znalezione: >= 0 = liczba WSZYSTKICH sieci w zasiegu (zadna nie ma
    // naszej nazwy), -2 = skan sie nie udal / nie zmiescil w limicie.
    Serial.printf("tag=WIFI lvl=WARN zdarzenie=skan_brak_centrali ssid=%s wynik_skanu=%d\n", WIFI_SSID, znalezione);
    return 0;
  }
  if (liczba > 1) {
    Serial.printf("tag=WIFI lvl=WARN zdarzenie=wiele_central liczba=%d msg=\"kilka sieci o tej samej nazwie w zasiegu - lacze z najsilniejsza\"\n", liczba);
  }
  wybrany = kandydaci[diag::wybierzNajsilniejszy(kandydaci, liczba)];
  return liczba;
}

// [v1.2.0 WIFI-WYBOR] Jedno miejsce, w ktorym startuje laczenie (wolane z
// polaczWifi() i z wifiReconnectNonBlocking() - w v1.1.0 ten sam if/else byl
// w obu).
//  - Znany BSSID i kanal: szybkie laczenie dokladnie jak w v1.1.0. Szybki
//    skan zaczyna od podanego kanalu i konczy na pierwszym trafieniu, a
//    "trafic" moze tylko ten jeden BSSID - nie ma tu czego wybierac.
//  - Bez BSSID wyboru dokonuje sterownik WiFi: ma przeszukac wszystkie
//    kanaly i wziac NAJSILNIEJSZA siec o tej nazwie (domyslnie wzialby
//    pierwsza znaleziona). To kosztuje pelny skan przed kazda proba, dlatego
//    tylko w tej galezi.
static void wifiRozpocznijLaczenie() {
  if (rtcBssidZnany) {
    WiFi.setScanMethod(WIFI_FAST_SCAN);
    WiFi.begin(WIFI_SSID, WIFI_PASS, rtcKanalCentrali, rtcBssidCentrali, true);
  } else {
    WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
    WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }
  zasilanieZastosujTx(nullptr);   // [v1.4.7 ZASILANIE] ciche powtorzenie po kazdym begin() - patrz komentarz przy funkcji
}

// [v1.2.1 RATUNEK] Kod przyczyny kazdego rozlaczenia WiFi. Wolane przez
// rdzen z OSOBNEGO zadania (zdarzen sieci), dlatego tylko zapisuje dwie
// zmienne - log wypisuje petla glowna (wifiZalogujRozlaczenia()). Te sama
// postac funkcji ma wifiZdarzenieDiag() w centrali.
static void wifiZdarzenie(WiFiEvent_t zdarzenie, WiFiEventInfo_t info) {
  if (zdarzenie == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    wifiOstatniKodRozlaczenia = info.wifi_sta_disconnected.reason;
    wifiLiczbaRozlaczen = wifiLiczbaRozlaczen + 1;
  }
}

// Jedna linia na rozlaczenie (przy serii - najwyzej co 2 s, z liczba
// pominietych w "nowych="). Kody: diag::nazwaKoduRozlaczenia(); np. 201 =
// sieci centrali nie ma w eterze, 200 = centrala przestala nadawac, 8 = panel
// rozlaczyl sie sam (WiFi.disconnect() przed kolejna proba).
static void wifiZalogujRozlaczenia() {
  static uint32_t zalogowano = 0;
  static unsigned long ostatnioMs = 0;
  const uint32_t lacznie = wifiLiczbaRozlaczen;
  if (lacznie == zalogowano) return;
  if (zalogowano != 0 && millis() - ostatnioMs < 2000UL) return;
  ostatnioMs = millis();
  const uint8_t kod = wifiOstatniKodRozlaczenia;
  Serial.printf("tag=WIFI zdarzenie=rozlaczono kod=%u nazwa=%s nowych=%lu lacznie=%lu\n",
                (unsigned)kod, diag::nazwaKoduRozlaczenia(kod),
                (unsigned long)(lacznie - zalogowano), (unsigned long)lacznie);
  zalogowano = lacznie;
}

// [v1.2.1 RATUNEK] WiFi zglasza polaczenie, a centrala od 12 s nie odpowiada
// (patrz panelRatunekKrok()): rozlacz i polacz od nowa. Mozliwy powod takiego
// stanu (PRZYPUSZCZENIE, niesprawdzone na plytce): centrala zrestartowala sie
// na tyle szybko, ze panel nie zauwazyl przerwy w nadawaniu - uwaza
// polaczenie za wazne, a centrala po restarcie juz go nie zna. Niezaleznie od
// powodu: w tym stanie v1.1.0 i v1.2.0 nie probowaly niczego.
// `bezBssid`: zapomnij zapamietany nadajnik i szukaj sieci centrali od nowa.
static bool wifiWymuszenieTrwa = false;
static unsigned long wifiWymuszenieMs = 0;
static const uint32_t WIFI_WYMUSZENIE_CZAS_MS = 5000;   // tyle ma wymuszona proba, zanim wifiReconnectNonBlocking() zacznie wlasne

void wifiWymusPolaczenie(bool bezBssid, uint8_t nr) {
  const bool wifiOk = (WiFi.status() == WL_CONNECTED);
  char bssid[18];
  diag::formatBssid(wifiOk ? WiFi.BSSID() : nullptr, bssid, sizeof(bssid));
  Serial.printf("tag=WIFI lvl=WARN zdarzenie=wymuszam_polaczenie nr=%u szukam=%s bssid=%s kanal=%d rssi=%d bez_danych_s=%lu "
                "msg=\"WiFi polaczone, a centrala nie odpowiada - lacze od nowa\"\n",
                (unsigned)nr, bezBssid ? "od_nowa" : "ten_sam_bssid", bssid,
                wifiOk ? (int)WiFi.channel() : 0, wifiOk ? (int)WiFi.RSSI() : 0,
                (unsigned long)(panelRatunek.bezDanychMs / 1000UL));
  if (bezBssid) rtcBssidZnany = false;
  WiFi.disconnect(false);
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);
  wifiRozpocznijLaczenie();
  wifiWymuszenieTrwa = true;
  wifiWymuszenieMs = millis();
}

// [v1.4.6 WERSJE] Ekran startowy (TFT_eSPI, zanim LVGL przejmie wyswietlacz).
// Uklad (480x320, MC_DATUM = wspolrzedne to srodek napisu):
//   y= 96  "WYSWIETLACZ CO"           font 4 (26 px)
//   y=124  "Panel v1.4.6"             font 2 (16 px)
//   y=150  "Szukam centrali" / "Laczenie z WiFi"   (jak dotad; polaczWifi())
//   y=182  "Centrala v3.58.6"         po pierwszej odpowiedzi (ekranStartowyWersje())
// Napisy ASCII - czcionki TFT_eSPI nie maja polskich liter (jak "Szukam
// centrali"); to ekran na kilka sekund. Zgloszenie: "przy starcie LCD nie
// pokazuje wersji, jest tylko Szukam centrali / Laczenie z WiFi".
static const int EKRAN_START_Y_TYTUL    = 96;
static const int EKRAN_START_Y_PANEL    = 124;
static const int EKRAN_START_Y_CENTRALA = 182;
// Ile ekran startowy stoi po dopisaniu wersji centrali, zeby dalo sie ja
// przeczytac. Tylko przy zimnym starcie - po wybudzeniu z deep sleep
// (przycisk / ladowarka) ekranu startowego w ogole nie ma (v1.5.1,
// startPoSnie): pulpit LVGL idzie na wyswietlacz jeszcze przed WiFi.
static const uint32_t EKRAN_START_POKAZ_MS = 1500;

static void ekranStartowyNaglowek() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(KOL_TEKST, KOL_TLO);
  tft.setTextFont(4);
  tft.drawString("WYSWIETLACZ CO", 240, EKRAN_START_Y_TYTUL);
  tft.setTextFont(2);
  char linia[32];
  snprintf(linia, sizeof(linia), "Panel %s", FIRMWARE_VERSION_PANEL);
  tft.drawString(linia, 240, EKRAN_START_Y_PANEL);
}

// Wolane raz z setup(), po poczekajNaPierwszeDane(): ekran startowy wciaz
// jest na wyswietlaczu (LVGL rysuje dopiero od pierwszego lv_timer_handler()
// w loop()), wiec dopisanie linii jest tanie. Gdy centrala nie odpowiedziala
// w PIERWSZA_PROBKA_TIMEOUT_MS, napis mowi to wprost i nie ma dodatkowej
// pauzy - panel juz czekal 2,5 s, a pulpit i tak pokaze "--".
static void ekranStartowyWersje() {
  Serial.printf("tag=PANEL zdarzenie=ekran_startowy panel=%s centrala=%s\n",
                FIRMWARE_VERSION_PANEL, stan.centralaFw[0] != '\0' ? stan.centralaFw : "-");
  if (startPoSnie) return;   // [v1.5.1] po wybudzeniu na ekranie jest pulpit LVGL, nie ekran startowy - tylko log
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(KOL_TEKST, KOL_TLO);
  tft.setTextFont(2);
  tft.fillRect(0, EKRAN_START_Y_CENTRALA - 12, 480, 24, KOL_TLO);
  char linia[40];
  if (stan.centralaFw[0] != '\0') snprintf(linia, sizeof(linia), "Centrala %s", stan.centralaFw);
  else if (swiezoscStatusu.byloKiedykolwiek) snprintf(linia, sizeof(linia), "Centrala: wersja nieznana");
  else snprintf(linia, sizeof(linia), "Centrala: brak odpowiedzi");
  tft.drawString(linia, 240, EKRAN_START_Y_CENTRALA);
  if (!swiezoscStatusu.byloKiedykolwiek) return;
  const unsigned long start = millis();
  while (millis() - start < EKRAN_START_POKAZ_MS) {
    esp_task_wdt_reset();   // watchdog petli (15 s) juz dziala - jak w polaczWifi()
    obsluzBrzeczyk();       // alarm sprzed samoczynnego restartu gra dalej
    delay(50);
  }
}

void polaczWifi() {
  // [v1.4.1 B8] Bez tego biblioteka WiFi zapisuje nazwe sieci i haslo do NVS
  // i podaje je sterownikowi przy kazdym WiFi.begin() (arduino-esp32 3.3.8,
  // WiFiGeneric.cpp: _persistent = true). Panel ma je w programie, kopia w
  // NVS jest zbedna. Musi byc PRZED WiFi.mode().
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.onEvent(wifiZdarzenie);   // [v1.2.1 RATUNEK] kody rozlaczen do logu - polaczWifi() jest wolane raz, z setup()
  // [v1.4.7 ZASILANIE] Dopiero TERAZ, po WiFi.mode() (sterownik radia juz
  // wystartowal) - wczesniej esp_wifi_set_max_tx_power() zwraca blad
  // (audyt 2026-11-18, temat 18). Przy indeksie 0 ustawia 19,5 dBm = domyslne.
  zasilanieZastosujTx("start");

  // [ETAP 3 KROK 2] Statyczne IP PRZED WiFi.begin() — glowna dzwignia
  // szybkosci (patrz komentarz przy definicji WIFI_IP_STATYCZNE wyzej).
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);

  if (startPoSnie) {
    // [v1.5.1] Po wybudzeniu z glebokiego snu (przycisk / ladowarka) nikt nie
    // chce czytac wersji - ma byc pulpit. LVGL ma juz zbudowane ekrany
    // (lvgl_dashboard_poc_init() w setup() przed ta funkcja), ale rysuje
    // dopiero przy pierwszym lv_timer_handler() - zwykle w loop(), czyli po
    // WiFi (do 20 s) i po pierwszych danych (do 2,5 s). Jeden obrot tutaj
    // wysyla caly pulpit (kafelki z "--", naglowek bez WiFi) do wyswietlacza
    // OD RAZU; dane i ikona WiFi dochodza w tle, jak przy kazdej utracie
    // polaczenia. Zegar LVGL stal przez caly setup() - pierwszy tick nadrabia
    // go skokiem, to nieszkodliwe (animacji jeszcze nie ma).
    lvgl_port_tick();
    Serial.printf("tag=SEN zdarzenie=pulpit_po_wybudzeniu ms=%lu\n", (unsigned long)millis());
  } else {
    tft.fillScreen(KOL_TLO);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(KOL_TEKST, KOL_TLO);
    ekranStartowyNaglowek();   // [v1.4.6] "WYSWIETLACZ CO" + "Panel v1.4.6" nad napisami o laczeniu
    tft.setTextFont(2);
  }

  if (!rtcBssidZnany) {
    // [v1.2.0 WIFI-WYBOR] Zimny start (pamiec RTC pusta): wlasny skan, wybor
    // najsilniejszej centrali i polaczenie z NIA po BSSID.
    if (!startPoSnie) tft.drawString("Szukam centrali", 240, 150);   // [v1.5.1] po snie pulpit juz jest na ekranie
    diag::KandydatAp ap;
    wifiCentralPrzyStarcie = wifiSkanujCentrale(ap);
    if (wifiCentralPrzyStarcie > 0) {
      memcpy(rtcBssidCentrali, ap.bssid, 6);
      rtcKanalCentrali = ap.kanal;
      rtcBssidZnany = true;
    }
    // Napis jest wysrodkowany na y=150 (MC_DATUM, font 16 px -> zajmuje
    // y=142..158), a petla ponizej czysci tylko y=150..170 - bez tego spod
    // "Laczenie z WiFi" wystawalyby resztki "Szukam centrali".
    if (!startPoSnie) tft.fillRect(0, 138, 480, 24, KOL_TLO);
  }

  wifiRozpocznijLaczenie();

  unsigned long startMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startMs < 20000) {
    // [v1.2.0 FIX-WDT-WIFI] setup() podpina loopTask pod Task WDT (15 s,
    // panic) PRZED polaczWifi(), a ta petla czeka do 20 s i nie karmila
    // watchdoga: przy centrali wylaczonej albo poza zasiegiem panel
    // restartowalby sie co ok. 15 s w kolko, nie dochodzac do ekranu
    // (wniosek z kodu i z testu na PC - nie zaobserwowane na plytce).
    esp_task_wdt_reset();
    if (!startPoSnie) {   // [v1.5.1] po wybudzeniu na ekranie jest juz pulpit LVGL - nie mazac po nim
      tft.fillRect(0, 150, 480, 20, KOL_TLO);
      tft.drawString("Laczenie z WiFi" , 240, 150);
    }
    // [v1.2.1] Jak w petli skanu wyzej: alarm sprzed samoczynnego restartu
    // gra dalej. 8 x 50 ms zamiast jednego delay(400), bo fazy wzorca trwaja
    // po 500 ms; bez alarmu obsluzBrzeczyk() wraca od razu.
    for (int i = 0; i < 8; i++) {
      obsluzBrzeczyk();
      delay(50);
    }
  }
  esp_task_wdt_reset();

  if (WiFi.status() == WL_CONNECTED) {
    // Zapamietaj BSSID/kanal na nastepne polaczenie (np. po wybudzeniu z deep sleep).
    memcpy(rtcBssidCentrali, WiFi.BSSID(), 6);
    rtcKanalCentrali = WiFi.channel();
    rtcBssidZnany = true;
    wifiZalogujPolaczenie("polaczono");
  } else {
    // Nieudane polaczenie (np. bledny cache po zmianie kanalu AP) — wyczysc cache,
    // zeby kolejna proba nie utknela na nieaktualnych danych.
    rtcBssidZnany = false;
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=brak_polaczenia_na_starcie msg=\"ponawiam w tle, ekran pokaze --\""));
  }
}

// [ETAP 5, PIEC_ESPNOW_PLAN.md] Wywolywane w KAZDEJ iteracji loop() —
// nieblokujace (WiFi.begin() na ESP32 wraca od razu, laczenie dzieje sie w
// tle; sam sprawdzamy WiFi.status() w kolejnych iteracjach). Reconnect z
// rosnacym opoznieniem miedzy probami zamiast co iteracje — patrz komentarz
// research przy WIFI_RECONNECT_MAX_MS na gorze pliku.
void wifiReconnectNonBlocking() {
  static bool poprzednioPolaczony = true; // setup() juz polaczyl (polaczWifi())
  // [v1.2.0 WIFI-WYBOR] Ile kolejnych prob bylo "na sztywno" do zapamietanego
  // BSSID. v1.1.0 probowal tak w nieskonczonosc: gdy ta konkretna centrala
  // znikla na stale (wylaczona centrala testowa, wymieniona plytka), panel
  // nie polaczyl sie juz z zadna inna az do odlaczenia zasilania.
  static const uint8_t WIFI_PROB_Z_BSSID = 3;
  static uint8_t probyZBssid = 0;

  wifiZalogujRozlaczenia();   // [v1.2.1 RATUNEK] kod przyczyny kazdego rozlaczenia

  if (WiFi.status() == WL_CONNECTED) {
    probyZBssid = 0;
    wifiWymuszenieTrwa = false;
    if (!poprzednioPolaczony) {
      wifiZalogujPolaczenie("reconnect_ok"); // [v1.2.0] z BSSID - widac, czy to ta sama centrala co przedtem
      lvglPokazZnacznikCentrali();          // koncowka BSSID w naglowku mogla sie zmienic
      // Odswiez cache BSSID/kanalu na wypadek, gdyby centrala zmienila kanal
      // AP miedzy rozlaczeniem a tym polaczeniem (patrz tez polaczWifi()).
      memcpy(rtcBssidCentrali, WiFi.BSSID(), 6);
      rtcKanalCentrali = WiFi.channel();
      rtcBssidZnany = true;
    }
    wifiOpoznienieReconnectMs = 1000; // reset backoff po odzyskaniu polaczenia
    poprzednioPolaczony = true;
    return;
  }

  if (poprzednioPolaczony) {
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=utrata_polaczenia"));
    // Pierwsza proba dopiero po uplywie poczatkowego opoznienia (1s), nie
    // natychmiast — unikamy "hammering" AP przy bardzo krotkich zanikach.
    wifiOstatniaProbaReconnectMs = millis();
  }
  poprzednioPolaczony = false;

  // [v1.2.1 RATUNEK] Laczenie zlecone przed chwila przez wifiWymusPolaczenie()
  // ma WIFI_WYMUSZENIE_CZAS_MS na dokonczenie - proba ponizej zaczyna sie od
  // WiFi.disconnect(), wiec przerwalaby je w polowie.
  if (wifiWymuszenieTrwa) {
    if (millis() - wifiWymuszenieMs < WIFI_WYMUSZENIE_CZAS_MS) return;
    wifiWymuszenieTrwa = false;
  }

  if (millis() - wifiOstatniaProbaReconnectMs < wifiOpoznienieReconnectMs) return;
  wifiOstatniaProbaReconnectMs = millis();

  Serial.printf("tag=WIFI zdarzenie=reconnect_probe nastepna_proba_za_ms=%lu\n", (unsigned long)wifiOpoznienieReconnectMs);

  // [FIX-WIFI-STATE, ten sam wzorzec co main.cpp centrali] disconnect(false)
  // PRZED begin() — bez tego powtarzane probe w petli moga trafic na
  // ESP_ERR_WIFI_STATE ("sta is connecting, cannot set config") i nie
  // wystartowac wcale. disconnect(false), NIE disconnect(true) — nie
  // zatrzymuje calego stosu WiFi, wraca w <1ms.
  WiFi.disconnect(false);
  WiFi.config(WIFI_IP_STATYCZNE, WIFI_GATEWAY, WIFI_MASKA);
  if (rtcBssidZnany && probyZBssid >= WIFI_PROB_Z_BSSID) {
    // [v1.2.0 WIFI-WYBOR] Zapamietany punkt dostepowy nie odpowiada - dalej
    // szuka juz sterownik WiFi (wszystkie kanaly, najsilniejszy sygnal -
    // patrz wifiRozpocznijLaczenie()). Bez wlasnego skanu: tu nie wolno
    // blokowac.
    rtcBssidZnany = false;
    probyZBssid = 0;
    Serial.println(F("tag=WIFI lvl=WARN zdarzenie=porzucam_bssid msg=\"zapamietana centrala nie odpowiada - szukam najsilniejszej sieci o tej nazwie\""));
  }
  if (rtcBssidZnany) probyZBssid++;
  wifiRozpocznijLaczenie();

  wifiOpoznienieReconnectMs = min(wifiOpoznienieReconnectMs * 2, WIFI_RECONNECT_MAX_MS);
}

// ----------------------------------------------------------------------------
// [ETAP 3b] ZASILANIE — deep sleep + wybudzanie przyciskiem
// ----------------------------------------------------------------------------
// [v1.5.0 SEN] Od tej wersji idzSpac() jest NAPRAWDE wolane - z senKrok()
// (nizej), ktore co sekunde liczy diag::senDecyzja() z warunkow plytki.
// Do v1.4.7 ten kod czekal (od v1.4.4 nikt go nie wolal - usypianie bylo
// podpiete tylko pod stary dashboard TFT). Sprzet jest juz zlutowany
// (2026-11-20): przycisk GPIO14 -> 1 kOhm -> tact -> GND (R4 chroni pin, pull-up
// wewnetrzny), ladowarka TP4056 na GPIO35 (patrz USB_DETECT_PIN), ogniwo na
// dzielniku GPIO34 (patrz BATERIA_ADC_PIN).
// GPIO14 wybrany w poprzedniej sesji: wolny (nie koliduje z TFT/dotykiem —
// patrz platformio.ini + TOUCH_* wyzej), w domenie RTC (wymog ext0), i NIE
// jest pinem strapping (w odroznieniu od 0/2/12/15) — bezpieczny wybor.
#define WAKE_BTN_PIN GPIO_NUM_14
// [v1.5.0 SEN] Drugie zrodlo wybudzenia: wlozenie ladowarki. TP4056 sciaga
// wtedy CHRG/STDBY (GPIO35, zewnetrzne 10k do 3V3) do masy - ext1 w trybie
// ALL_LOW na jednym pinie to dokladnie "obudz, gdy pin spadnie do 0". GPIO35
// jest w domenie RTC (RTC_GPIO5), wiec ext1 na nim dziala; ext0 i ext1 moga
// byc wlaczone rownoczesnie (ESP32 classic).
#define WAKE_USB_PIN_MASK (1ULL << 35)

// [ETAP 3b, detekcja USB — ZDECYDOWANE: TP4056 CHRG+STDBY, nie dzielnik na
// VIN] Pierwotny plan (dzielnik napiecia z VIN/5V) zalezal od zalozenia o
// wewnetrznym okablowaniu plytki (czy bateria tez idzie przez VIN) — po
// decyzji o uzyciu TP4056 do ladowania baterii, jest prostsze i pewniejsze
// rozwiazanie niezalezne od tego zalozenia w ogole.
//
// TP4056 ma dwa wyjscia open-drain: CHRG (aktywne = ladowanie w toku) i
// STDBY (aktywne = ladowanie zakonczone) — OBA sa w stanie wysokiej
// impedancji (pływaja), gdy TP4056 NIE MA zasilania z VCC (czyli gdy USB
// odlaczone), niezaleznie od tego czy bateria jest podlaczona (research:
// datasheet TP4056 + kilka niezaleznych zrodel, potwierdzone jednoznacznie:
// "STDBY high impedance in other states", "floating pins cause false
// triggering — connect pull-up if open-drain output needed").
//
// Rozwiazanie: zewrzyj CHRG i STDBY razem (bezpieczne — oba to open-drain,
// wired-OR), JEDEN rezystor pull-up 10k z tego wspolnego wezla do 3V3
// ESP32 (NIE do VCC samego TP4056 — to VCC znika razem z USB, wiec pull-up
// do VCC nie dawalby zadnego jednoznacznego stanu przy braku USB; 3V3
// ESP32 jest zasilane z baterii przez caly czas, wiec zawsze dostepne jako
// stabilna referencja). Wspolny wezel -> GPIO35. Bez dzielnika napiecia —
// niepotrzebny, sygnal jest juz 3,3V-bezpieczny (open-drain do GND, pull-up
// do 3V3, nigdy do 5V).
//
// Logika (odwrotna wzgledem starej wersji z dzielnikiem): USB podlaczone ->
// TP4056 zasilany -> CHRG lub STDBY (lub oba) aktywne -> wezel ciagniety w
// dol -> LOW. USB odlaczone -> TP4056 bez zasilania -> oba piny plywaja ->
// pull-up wygrywa -> HIGH.
#define USB_DETECT_PIN 35

// Kilka probek zamiast jednego odczytu — tania ochrona przed pojedynczym
// zakloceniem/szumem na linii (podobny wzorzec do touchReadChannelStabilnie
// dla dotyku, ktory juz jest w tym pliku).
bool zasilanieUSB() {
  int niskie = 0;
  for (int i = 0; i < 5; i++) {
    if (digitalRead(USB_DETECT_PIN) == LOW) niskie++;
    delayMicroseconds(200);
  }
  return niskie >= 3; // wiekszosc probek LOW = TP4056 zasilany z USB (CHRG/STDBY ciagna w dol)
}

// [BATERIA-POMIAR, ta sesja] Realne napiecie ogniwa (nie tylko "czy sie
// laduje" jak zasilanieUSB() wyzej) - osobny dzielnik napiecia, osobny pin,
// bo to inny pomiar (poziom analogowy, nie stan cyfrowy open-drain).
// Dzielnik: Vbat --[R1 100k]-- BATERIA_ADC_PIN --[R2 100k]-- GND, + ~100nF
// z pinu do GND (filtr). R1=R2 -> pin ADC widzi dokladnie polowe Vbat, co
// dla ogniwa 1S (3,0-4,2V) daje 1,5-2,1V na pinie - srodek zakresu ADC
// ESP32, z dala od najmniej liniowych okolic 0V i 3,3V.
//
// Pin: musi byc ADC1 (GPIO 32-39) - ADC2 przestaje dzialac przy aktywnym
// WiFi (sprzetowe ograniczenie ESP32, a WiFi u nas chodzi caly czas).
// Z wolnych ADC1 (32/34/36/39 - 33 i 35
// juz zajete przez TOUCH_MISO/USB_DETECT_PIN) wybrany GPIO34 - najczesciej
// spotykany wybor w tego typu obwodach, wejsciowy-only, nic z istniejacego
// okablowania NIE trzeba przelutowywac. Pelne uzasadnienie: PLAN_BATERIA.md.
#define BATERIA_ADC_PIN 34
static const float BATERIA_DZIELNIK = 2.0f; // R1=R2 -> odczyt z pinu razy 2 = pelne Vbat

// Usrednienie probek - ten sam motyw co w zasilanieUSB() wyzej, pojedynczy
// odczyt ADC potrafi "skakac" o kilkanascie mV. analogReadMilliVolts() (NIE
// surowy analogRead()) uzywa fabrycznej kalibracji ADC (eFuse) - wymaga
// jednorazowego analogSetAttenuation(ADC_11db) w setup() (patrz tam), bo
// analogReadMilliVolts() honoruje TYLKO globalne attenuation, ignoruje
// wersje "per-pin" (research, ta sesja - znana pulapka rdzenia arduino-esp32).
float bateriaOdczytajNapiecie() {
  const int PROBEK = 16;
  uint32_t sumaMv = 0;
  for (int i = 0; i < PROBEK; i++) {
    sumaMv += analogReadMilliVolts(BATERIA_ADC_PIN);
    delayMicroseconds(200);
  }
  return (sumaMv / (float)PROBEK) * BATERIA_DZIELNIK / 1000.0f; // mV na pinie -> pelne V baterii
}

// Napiecie LiPo NIE spada liniowo z % naladowania - 4,20 do ok. 3,80V to
// plaskie plateau (wiekszosc pojemnosci), ponizej ~3,7V krzywa gwaltownie
// przyspiesza w dol. Tabela + interpolacja liniowa MIEDZY punktami zamiast
// prostego mapowania 3,0-4,2V -> 0-100% (to zawyzaloby srodek skali).
// Orientacyjna krzywa 1S, zgodna z powszechnie publikowanymi wartosciami
// (research, ta sesja) - to wskaznik "jak w telefonie", nie licznik
// coulombow.
struct BPunkt { float v; int proc; };
static const BPunkt TABELA_BAT[] = {
  {4.20f,100},{4.15f,95},{4.11f,90},{4.08f,85},{4.02f,80},
  {3.98f,75},{3.95f,70},{3.91f,65},{3.87f,60},{3.85f,55},
  {3.84f,50},{3.82f,45},{3.80f,40},{3.79f,35},{3.77f,30},
  {3.75f,25},{3.73f,20},{3.71f,15},{3.69f,10},{3.61f, 5},
  {3.27f, 0},
};
static const int TABELA_BAT_N = sizeof(TABELA_BAT) / sizeof(TABELA_BAT[0]);

int bateriaNapiecieNaProcent(float v) {
  if (v >= TABELA_BAT[0].v) return 100;
  if (v <= TABELA_BAT[TABELA_BAT_N - 1].v) return 0;
  for (int i = 0; i < TABELA_BAT_N - 1; i++) {
    if (v <= TABELA_BAT[i].v && v >= TABELA_BAT[i + 1].v) {
      float zakresV = TABELA_BAT[i].v - TABELA_BAT[i + 1].v;
      float pozycja = (TABELA_BAT[i].v - v) / zakresV;
      return round(TABELA_BAT[i].proc - pozycja * (TABELA_BAT[i].proc - TABELA_BAT[i + 1].proc));
    }
  }
  return 0;
}

// [v1.5.0 SEN] Ostatni pomiar ogniwa - wspolny dla wskaznika (bateriaKrok())
// i decyzji o snie (senKrok()); senKrok() nie mierzy sam, zeby ADC nie byl
// odpytywany co sekunde bez potrzeby.
static float bateriaOstatnieV   = 0.0f;
static bool  bateriaOstatnioJest = false;   // diag::bateriaObecna(bateriaOstatnieV)
static int8_t bateriaUsbPokazane = -1;      // [v1.5.3] stan USB na wskazniku (-1 = jeszcze nic); senKrok() odswieza wskaznik od razu po zmianie
static bool  bateriaZmierzona   = false;    // false do pierwszego pomiaru (od v1.5.0: od razu po starcie)

// [research, ta sesja] Znany bug/pulapka Espressif: jesli pin wybudzajacy
// jest WCIAZ wcisniety w momencie wywolania esp_deep_sleep_start(), ESP32
// natychmiast wybudza sie z powrotem — petla usypiania. Dlatego czekamy na
// PUSZCZENIE przycisku + krotki debounce TUZ PRZED wlasciwym usnieciem,
// nie tylko przy samym wybudzeniu.
//
// [v1.5.0 SEN] `powod` to tekst z diag::senDecyzja() (do logu); gdy zaczyna
// sie od "bateria_", przed snem stoi przez diag::SEN_NAPIS_MS napis dla
// uzytkownika - inaczej panel "sam zgasl" bez slowa i wygladaloby to na awarie.
// Napis idzie prosto przez TFT_eSPI (ASCII, bez polskich liter - jak ekran
// startowy): LVGL nie jest juz potrzebne, za chwile twardy reset.
void idzSpac(const char* powod) {
  // [OTA-PANEL, ta sesja, prompt #8] Nigdy nie usypiaj w trakcie pobierania/
  // flashowania firmware'u — przerwany zapis do flash moze zostawic panel
  // w zlym stanie (patrz BAGNO 5.1 w OTA_PANEL_CALOSC.md). Flaga jest
  // czyszczona po KAZDEJ probie pobierania (otaSprzatnijPoProbie(), [v1.4.0
  // OTA]) - udanej konczy sie restartem - wiec to tylko chwilowe opoznienie
  // usniecia, nie trwala blokada. (senDecyzja() tez to sprawdza - to drugi
  // bezpiecznik, na wypadek wolania idzSpac() z innego miejsca.)
  if (trwaAktualizacjaOtaPanelu) {
    Serial.println(F("tag=OTA-PANEL zdarzenie=usypianie_zablokowane"));
    return;
  }

  Serial.printf("tag=SEN zdarzenie=usypianie powod=%s napiecie=%.2fV bez_dotyku_s=%lu\n",
                powod != nullptr ? powod : "-", (double)bateriaOstatnieV,
                (unsigned long)(lvgl_dashboard_poc_bez_dotyku_ms() / 1000UL));

  // Brzeczyk: cisza i koniec PWM PRZED snem - ewentualny alarm sprzed chwili
  // nie ma juz do kogo grac, a pin musi byc zwyklym wyjsciem (nie LEDC),
  // zeby gpio_hold_en() nizej zamrozil stan niski.
  ledcWriteTone(BRZECZYK_PIN, 0);
  ledcDetach(BRZECZYK_PIN);
  pinMode(BRZECZYK_PIN, OUTPUT);
  digitalWrite(BRZECZYK_PIN, LOW);

  if (diag::senZNapisem(powod)) {
    // Napis na pelnej jasnosci (ekran mogl byc przyciemniony) - ma byc widoczny.
    senPrzyciemnione = false;
    ustawJasnoscPodswietlenia(jasnoscDziennaWartosc);
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(4);
    tft.drawString("BATERIA ROZLADOWANA", 240, 120);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextFont(2);
    tft.drawString("Podlacz ladowarke. Panel usypia.", 240, 170);
    tft.drawString("Przycisk albo ladowarka budzi panel.", 240, 195);
    const unsigned long start = millis();
    while (millis() - start < diag::SEN_NAPIS_MS) {
      esp_task_wdt_reset();
      delay(50);
    }
  }

  // Wygaszenie ekranu przed snem — nie ma sensu trzymac podswietlenia
  // wlaczonego, skoro i tak zaraz nastapi twardy reset przy wybudzeniu.
  senPrzyciemnione = false;
  ustawJasnoscPodswietlenia(0);
  tft.fillScreen(TFT_BLACK);

  // [FIX-ILI9488-SLEEP, ta sesja] Samo wygaszenie ekranu na czarno NIE usypia
  // kontrolera ILI9488 — bez tej komendy kontroler zostaje aktywny i ciagnie
  // normalny prad roboczy przez caly czas snu ESP32, niezaleznie od tego, ze
  // nic nie widac na ekranie. Potwierdzone pomiarem multimetrem: AKTYWNY
  // (Sleep OUT) ~62mA vs SPOCZYNEK (Sleep IN) ~0.11mA.
  // Po stronie wybudzenia NIE trzeba jawnego Sleep OUT — TFT_RST=4 (patrz
  // platformio.ini) to prawdziwy pin, wiec tft.init() w setup() i tak zawsze
  // robi twardy reset przez RST, a nastepnie tabela init dla ILI9488 sama
  // wysyla Sleep OUT + delay(120) (zweryfikowane w zrodlach TFT_eSPI 2.5.43,
  // plik TFT_Drivers/ILI9488_Init.h) — dublowanie tego w setup() byloby
  // zbedne (dodatkowe 120ms przy kazdym starcie bez zadnej korzysci).
  tft.writecommand(0x10);             // ILI9488: Sleep IN

  // [FIX-TFT-BL-HOLD, ta sesja] Pin podswietlenia (TFT_BL=GPIO21) NIE MOZE
  // "wisiec" (floating) podczas deep sleep — wewnetrzne stany pinow sa
  // resetowane przy wejsciu w sen, wiec bez jawnego "hold" podswietlenie
  // moze sie z powrotem czesciowo zaswiecic (przez uplywnosc/podciaganie na
  // plytce wyswietlacza) i zjadac baterie w tle przez caly sen. GPIO21 NIE
  // jest pinem RTC (w odroznieniu od poprzedniego GPIO32), wiec wystarczy
  // uniwersalny mechanizm gpio_hold_en + globalny gpio_deep_sleep_hold_en —
  // BEZ potrzeby uzywania rtc_gpio_hold_en().
  gpio_hold_en((gpio_num_t)TFT_BL);   // zamraza biezacy stan pinu (LOW) na czas snu
  // [v1.5.0 SEN] To samo dla brzeczyka: GPIO13 nie jest pinem RTC, wiec bez
  // "hold" plywalby przez caly sen; przez R5 (1k) idzie na baze C945 -
  // szum moglby cicho zalaczac modul i zjadac baterie. Stan niski = cisza.
  gpio_hold_en((gpio_num_t)BRZECZYK_PIN);
  gpio_deep_sleep_hold_en();          // globalny wlacznik "trzymania" cyfrowych pinow w deep sleep

  // Czekaj na puszczenie przycisku, zeby uniknac natychmiastowego wybudzenia.
  while (digitalRead(WAKE_BTN_PIN) == LOW) {
    delay(10);
  }
  delay(50); // debounce

  // Pin wybudzajacy NIE MOZE "wisiec" (floating) podczas snu — wewnetrzne
  // pull-upy/pull-downy sa w deep sleep wylaczone, chyba ze wlaczy sie je
  // jawnie funkcjami rtc_gpio_*. Bez tego przypadkowy szum na pinie moze
  // wybudzic urzadzenie natychmiast po usnieciu.
  rtc_gpio_pullup_en(WAKE_BTN_PIN);
  rtc_gpio_pulldown_dis(WAKE_BTN_PIN);

  esp_sleep_enable_ext0_wakeup(WAKE_BTN_PIN, 0); // 0 = wybudzenie na stanie LOW
  // [v1.5.0 SEN] Drugie zrodlo: ladowarka (GPIO35 spada do 0, gdy TP4056
  // dostaje USB). Pin ma zewnetrzne 10k do 3V3, wiec we snie nie plywa.
  esp_sleep_enable_ext1_wakeup(WAKE_USB_PIN_MASK, ESP_EXT1_WAKEUP_ALL_LOW);
  esp_deep_sleep_start();
  // (nic dalej sie nie wykona — po wybudzeniu ESP32 startuje od nowa od setup())
}

// [v1.5.0 SEN] Pomiar ogniwa + wskaznik baterii. Do v1.4.7 ten kod byl w
// loop(); wyniesiony tutaj, bo odczyt jest teraz pamietany dla senKrok().
// Pierwszy pomiar od razu po starcie (wskaznik nie pokazuje przez 30 s "USB"
// przy ogniwie, a panel wybudzony z rozladowana bateria nie czeka pol minuty
// na decyzje o powrocie do snu), potem co BATERIA_INTERWAL_MS.
static const uint32_t BATERIA_INTERWAL_MS = 30000;
void bateriaKrok() {
  static unsigned long ostatniaBateria = 0;
  const unsigned long teraz = millis();
  if (bateriaZmierzona && teraz - ostatniaBateria < BATERIA_INTERWAL_MS) return;
  ostatniaBateria = teraz;
  bateriaZmierzona = true;
  const float napiecieBat = bateriaOdczytajNapiecie();
  // [v1.2.0 BATERIA-BRAK] Ponizej diag::BATERIA_PROG_OBECNOSCI_V (2,5 V) na
  // dzielniku nie ma ogniwa - panel pracuje z samego USB. v1.1.0 pokazywal
  // wtedy "0%" w kolorze "laduje sie" (log z 2026-11-10:
  // "napiecie=0.28V procent=0 ladowanie=1" co 30 s). Teraz: napis "USB" na
  // wskazniku, a w logu jedna linia przy zmianie stanu zamiast co 30 s.
  static int8_t poprzednioBateriaJest = -1;   // -1 = jeszcze nie mierzono
  const bool bateriaJest = diag::bateriaObecna(napiecieBat);
  bateriaOstatnieV = napiecieBat;
  bateriaOstatnioJest = bateriaJest;
  if (bateriaJest) {
    const int procentBat = bateriaNapiecieNaProcent(napiecieBat);
    const bool usb = zasilanieUSB();
    Serial.printf("tag=BATERIA napiecie=%.2fV procent=%d ladowanie=%d\n", napiecieBat, procentBat, (int)usb);
    lvgl_dashboard_poc_aktualizuj_bateria(procentBat, usb);
    bateriaUsbPokazane = usb ? 1 : 0;
  } else {
    if (poprzednioBateriaJest != 0) {
      Serial.printf("tag=BATERIA zdarzenie=brak_ogniwa napiecie=%.2fV prog=%.1fV msg=\"zasilanie z USB, wskaznik pokazuje USB\"\n",
                    napiecieBat, diag::BATERIA_PROG_OBECNOSCI_V);
    }
    lvgl_dashboard_poc_aktualizuj_bateria(-1, false);
  }
  poprzednioBateriaJest = bateriaJest ? 1 : 0;
}

// [v1.5.0 SEN] Przyciemnienie i sen na baterii - wolane co obrot loop().
//  - co obrot (tanio): przycisk budzenia wcisniety w czasie pracy = aktywnosc;
//    jesli ekran jest przyciemniony, a ktos wlasnie dotknal - jasnosc wraca
//    NATYCHMIAST (nie dopiero przy nastepnym liczeniu decyzji za sekunde);
//  - co diag::SEN_SPRAWDZAJ_CO_MS: pelna decyzja diag::senDecyzja().
// Jasnosc po powrocie z przyciemnienia = ta, ktora wynika z harmonogramu
// dzien/noc (wyswietlaczPrzyciemnionyTeraz), nie zawsze dzienna.
static void senPrzywrocJasnosc(const char* powod) {
  if (!senPrzyciemnione) return;
  senPrzyciemnione = false;
  ustawJasnoscPodswietlenia(wyswietlaczPrzyciemnionyTeraz ? jasnoscNocnaWartosc : jasnoscDziennaWartosc);
  Serial.printf("tag=SEN zdarzenie=jasnosc powod=%s\n", powod);
}

void senKrok() {
  if (digitalRead(WAKE_BTN_PIN) == LOW) lvgl_dashboard_poc_zglos_aktywnosc();
  if (senPrzyciemnione && lvgl_dashboard_poc_bez_dotyku_ms() < diag::senPrzyciemnijMs(senPrzyciemnijIdx)) senPrzywrocJasnosc("dotyk");

  static unsigned long ostatnieSprawdzenie = 0;
  static const char* poprzedniPowod = "";
  const unsigned long teraz = millis();
  if (teraz - ostatnieSprawdzenie < diag::SEN_SPRAWDZAJ_CO_MS) return;
  ostatnieSprawdzenie = teraz;
  if (!bateriaZmierzona) return;   // jeszcze przed pierwszym pomiarem (tylko pierwsze ms po starcie)

  diag::SenWarunki w;
  w.bateriaJest  = bateriaOstatnioJest;
  w.zasilanieUsb = zasilanieUSB();
  w.napiecieV    = bateriaOstatnieV;
  // [v1.5.3] Wlozenie/wyjecie ladowarki widac na wskazniku od razu (co
  // sekunde), nie dopiero przy nastepnym pomiarze baterii za <= 30 s.
  if (w.bateriaJest && bateriaUsbPokazane >= 0 && (bateriaUsbPokazane != 0) != w.zasilanieUsb) {
    bateriaUsbPokazane = w.zasilanieUsb ? 1 : 0;
    Serial.printf("tag=ZASILANIE zdarzenie=%s napiecie=%.2fV\n", w.zasilanieUsb ? "usb_podlaczone" : "usb_odlaczone", (double)bateriaOstatnieV);
    lvgl_dashboard_poc_aktualizuj_bateria(bateriaNapiecieNaProcent(bateriaOstatnieV), w.zasilanieUsb);
  }
  w.bezDotykuMs  = lvgl_dashboard_poc_bez_dotyku_ms();
  w.alarm        = stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel || brzeczykAktywnyTyp != BRZECZYK_BRAK;
  w.ota          = trwaAktualizacjaOtaPanelu || lvgl_dashboard_poc_ota_widoczna();
  w.przyciemnijPoMs = diag::senPrzyciemnijMs(senPrzyciemnijIdx);   // [v1.5.1] z ekranu WYSWIETLACZ (0 = nigdy)
  w.uspijPoMs       = diag::senUspijMs(senUspijIdx);
  const diag::SenDecyzja d = diag::senDecyzja(w);

  switch (d.akcja) {
    case diag::SEN_AKCJA_USPIJ:
      idzSpac(d.powod);   // nie wraca (poza blokada OTA - wtedy za sekunde jeszcze raz)
      break;
    case diag::SEN_AKCJA_PRZYCIEMNIJ:
      if (!senPrzyciemnione) {
        senPrzyciemnione = true;
        ustawJasnoscPodswietlenia(diag::SEN_JASNOSC_PROC);
        Serial.printf("tag=SEN zdarzenie=przyciemnienie powod=%s napiecie=%.2fV jasnosc=%u%%\n",
                      d.powod, (double)bateriaOstatnieV, (unsigned)diag::SEN_JASNOSC_PROC);
      }
      break;
    default:
      // Powod inny niz "aktywny"/"dotyk" (usb podlaczone, alarm, ota) tez
      // konczy przyciemnienie - np. alarm w nocy ma byc widoczny.
      senPrzywrocJasnosc(d.powod);
      break;
  }
  // Jedna linia logu przy zmianie powodu "dlaczego panel nie spi" - tylko
  // dla powodow blokujacych (alarm/ota), zeby log nie pisal "aktywny"/"usb" w kolko.
  if (strcmp(d.powod, poprzedniPowod) != 0) {
    if (strcmp(d.powod, "alarm") == 0 || strcmp(d.powod, "ota") == 0) {
      Serial.printf("tag=SEN zdarzenie=sen_wstrzymany powod=%s\n", d.powod);
    }
    poprzedniPowod = d.powod;
  }
}

// [OTA-ROLLBACK] Potwierdza biezaca partycje jako VALID, jesli zostala
// wgrana przez OTA i wciaz czeka na weryfikacje (PENDING_VERIFY) — patrz
// Ustalenie #6 w OTA_PANEL_CALOSC.md, mechanizm zweryfikowany
// empirycznie w projekcie testowym ota_rollback_test (main_v1.cpp, ta sama
// logika co tutaj). Wolane z KONCA setup(), PO tym jak ekran juz sie
// poprawnie narysowal i (w trybach z WiFi) pierwsza probka pobierzStatus()
// juz sie wykonala — to nasz sygnal "firmware dziala wystarczajaco dobrze,
// zeby je zatwierdzic". Bezpieczne do wywolania w kazdym trybie: gdy
// partycja NIE jest PENDING_VERIFY (np. wgranie przez USB), warunek w
// srodku po prostu nic nie robi.
// [v1.4.0 OTA] Zwraca true, gdy biezaca partycja jest zatwierdzona (albo
// zatwierdzenia nie wymagala); false tylko wtedy, gdy czekala na zatwierdzenie
// i nie udalo sie go zapisac. Wynik bierze otaPoZatwierdzeniu().
bool potwierdzPartycjeJesliCzeka() {
  const esp_partition_t *biezaca = esp_ota_get_running_partition();
  esp_ota_img_states_t stanOta;

  esp_err_t wynik = esp_ota_get_state_partition(biezaca, &stanOta);
  if (wynik != ESP_OK) {
    // Normalne przy wgraniu przez USB — otadata nie sledzi obrazow
    // wgranych przez USB, to nie blad. Patrz main_v1.cpp / Ustalenie #6.
    Serial.printf("tag=OTA-ROLLBACK zdarzenie=brak_stanu_partycji blad=%d\n", (int)wynik);
    return true;
  }

  Serial.printf("tag=OTA-ROLLBACK zdarzenie=stan_partycji partycja=%s stan=%d\n",
                biezaca->label, (int)stanOta);

  if (stanOta == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t r = esp_ota_mark_app_valid_cancel_rollback();
    Serial.printf("tag=OTA-ROLLBACK zdarzenie=potwierdzono_valid wynik=%s\n",
                  esp_err_to_name(r));
    return r == ESP_OK;
  }
  return true;
}

// ============================================================================
// [v1.4.0 OTA] AKTUALIZACJA PANELU PRZEZ CENTRALE - wykonanie
// ============================================================================
// Decyzje (kiedy zaczac, kiedy odmowic, co zapamietac) sa w
// diagnostyka_panelu.h, sekcja [v1.4.0 OTA] - czysty kod testowany na PC.
// Tutaj jest to, co dotyka sprzetu: odczyt wlasnego programu, NVS, pobieranie
// (otaFetch), zapis do flash (Update), ekran i restart.
//
// Przebieg:
//   1. start panelu: otaPrzyStarcie() - identyfikator programu, z ktorego
//      panel pracuje, i to, co panel zapamietal o ostatniej ofercie,
//   2. po zatwierdzeniu partycji: otaPoZatwierdzeniu() - czy ostatnie wgranie
//      sie przyjelo, czy bootloader wrocil do poprzedniego programu,
//   3. w petli: otaKrok() - raz na sekunde decyzja (diag::otaDecyzja()), a w
//      trakcie pobierania kolejny kawalek pliku w kazdym obrocie petli.
// Do v1.3.0 (wykonajAktualizacjeOTAPanelu() + sprawdzIWykonajOtaPanelu(),
// biblioteka HTTPUpdate): panel sprawdzal oferte RAZ, na koncu setup(), i
// pobieral plik bez powrotu do petli - z pustym ekranem, bez odpytywania
// centrali (wiec i bez alarmu), z watchdogiem wypisanym na czas pobierania.
// Tego samego pliku nie odroznial od nowego, wiec przy centrali, ktora nie
// zdjela oferty, wgrywal go przy kazdym starcie.
// ----------------------------------------------------------------------------
static const char OTA_NVS_PRZESTRZEN[] = "otapanel";
static const char OTA_NVS_KLUCZ[] = "zapis";
static const uint8_t OTA_NVS_WERSJA = 1;   // pierwszy bajt zapisu; inna wartosc = zapis w nieznanej postaci, pomijany

// Caly zapis idzie do NVS jednym wpisem (putBytes): albo jest stary, albo
// nowy - nigdy polowa (np. nowy klucz oferty ze starym licznikiem prob).
static void otaZapisWczytaj() {
  memset(&otaZapis, 0, sizeof(otaZapis));
  Preferences p;
  // [v1.4.6] Do zapisu (false), nie tylko do odczytu: Preferences::begin() w
  // trybie odczytu loguje "[E][Preferences.cpp] nvs_open failed: NOT_FOUND"
  // przy kazdym starcie panelu, ktory nigdy jeszcze nie zaczynal
  // aktualizacji (przestrzeni w NVS nie ma; log z plytki 2026-11-19).
  // Otwarcie do zapisu zaklada pusta przestrzen po cichu (jednorazowo
  // kilkanascie bajtow NVS) - dalej ten sam odczyt co dotad.
  if (!p.begin(OTA_NVS_PRZESTRZEN, false)) return;
  uint8_t bufor[1 + sizeof(diag::OtaZapis)];
  if (p.isKey(OTA_NVS_KLUCZ) && p.getBytesLength(OTA_NVS_KLUCZ) == sizeof(bufor) &&
      p.getBytes(OTA_NVS_KLUCZ, bufor, sizeof(bufor)) == sizeof(bufor) && bufor[0] == OTA_NVS_WERSJA) {
    memcpy(&otaZapis, bufor + 1, sizeof(otaZapis));
    otaZapis.klucz[sizeof(otaZapis.klucz) - 1] = '\0';
    otaZapis.oczekiwany[sizeof(otaZapis.oczekiwany) - 1] = '\0';
    if (otaZapis.stan > diag::OTA_ZAPIS_ODMOWA) memset(&otaZapis, 0, sizeof(otaZapis));
  }
  p.end();
}

static bool otaZapisZachowaj() {
  Preferences p;
  if (!p.begin(OTA_NVS_PRZESTRZEN, false)) return false;
  uint8_t bufor[1 + sizeof(diag::OtaZapis)];
  bufor[0] = OTA_NVS_WERSJA;
  memcpy(bufor + 1, &otaZapis, sizeof(otaZapis));
  const bool ok = (p.putBytes(OTA_NVS_KLUCZ, bufor, sizeof(bufor)) == sizeof(bufor));
  p.end();
  return ok;
}

// Poczatek setup(), przed pierwszym zapytaniem o status.
// Identyfikator wlasnego programu to poczatek sumy SHA-256, ktora esptool
// dopisuje na koncu kazdego programu ESP32; esp_partition_get_sha256() dla
// partycji z programem zwraca wlasnie te dopisana sume. Ta sama suma jest na
// koncu pliku firmware.bin, z ktorego program wgrano (kablem albo przez OTA),
// wiec centrala moze policzyc ten sam identyfikator z samego pliku.
void otaPrzyStarcie() {
  const esp_partition_t *biezaca = esp_ota_get_running_partition();
  uint8_t suma[32];
  otaObrazWlasny[0] = '\0';
  if (biezaca != nullptr && esp_partition_get_sha256(biezaca, suma) == ESP_OK) diag::otaObrazZSumy(suma, otaObrazWlasny);
  // Pierwszy start po wgraniu przez OTA: partycja czeka na zatwierdzenie
  // (potwierdzPartycjeJesliCzeka() na koncu setup()). Do tej chwili panel
  // zglasza centrali "panel_obraz_proba", nie "panel_obraz".
  esp_ota_img_states_t stanOta;
  otaObrazZatwierdzony = !(biezaca != nullptr && esp_ota_get_state_partition(biezaca, &stanOta) == ESP_OK &&
                           stanOta == ESP_OTA_IMG_PENDING_VERIFY);
  otaZapisWczytaj();
  Serial.printf("tag=OTA-PANEL zdarzenie=stan_przy_starcie obraz=%s zatwierdzony=%d zapis_stan=%u zapis_prob=%u zapis_klucz=%s\n",
                otaObrazWlasny[0] != '\0' ? otaObrazWlasny : "nieznany", (int)otaObrazZatwierdzony,
                (unsigned)otaZapis.stan, (unsigned)otaZapis.prob, otaZapis.klucz[0] != '\0' ? otaZapis.klucz : "-");
  if (otaObrazWlasny[0] == '\0') {
    Serial.println(F("tag=OTA-PANEL lvl=WARN zdarzenie=brak_identyfikatora_obrazu msg=\"nie da sie odczytac sumy wlasnego programu - aktualizacja przez centrale wylaczona do restartu\""));
  }
}

// Koniec setup(), zaraz po potwierdzPartycjeJesliCzeka(). `zatwierdzona` to
// jej wynik: true = biezaca partycja jest zatwierdzona (albo nie wymagala tego).
void otaPoZatwierdzeniu(bool zatwierdzona) {
  if (!zatwierdzona) return;   // nie udalo sie zatwierdzic: panel dalej zglasza "panel_obraz_proba"
  otaObrazZatwierdzony = true;
  const int ocena = diag::otaOcenPoStarcie(otaZapis, otaObrazWlasny);
  if (ocena == 0) return;
  otaZapisZachowaj();
  if (ocena == 1) {
    Serial.printf("tag=OTA-PANEL zdarzenie=nowy_firmware_dziala obraz=%s wersja=%s\n", otaObrazWlasny, FIRMWARE_VERSION_PANEL);
  } else {
    // Panel wgral plik i zrestartowal sie, a dziala inny program niz wgrany:
    // nowy nie doszedl do zatwierdzenia i bootloader wrocil do poprzedniego.
    Serial.printf("tag=OTA-PANEL lvl=ERR zdarzenie=nowy_firmware_wycofany wgrany=%s dziala=%s\n",
                  otaZapis.oczekiwany, otaObrazWlasny[0] != '\0' ? otaObrazWlasny : "nieznany");
  }
}

// Koniec proby pobierania (udanej albo nie): gniazdo, bufor zapisu, ekran.
static void otaSprzatnijPoProbie() {
  if (otaFetch.wTrakcie()) otaFetch.przerwij();
  else otaFetch.skwituj();
  if (otaP.zapisZaczety) Update.abort();   // nic z niedokonczonego zapisu nie zostaje do uruchomienia
  otaP.zapisZaczety = false;
  otaP.trwa = false;
  trwaAktualizacjaOtaPanelu = false;
  otaBylaProba = true;
  otaKoniecProbyMs = millis();
  lvgl_dashboard_poc_ota(-1, nullptr);
}

// Proba nieudana z winy pliku, sieci albo zapisu - liczy sie do limitu prob.
static void otaProbaNieudana(uint8_t powod, bool ostateczna, const char *opis) {
  const bool koniecProb = diag::otaZapisNieudana(otaZapis, powod, ostateczna);
  const bool zapisanoStan = otaZapisZachowaj();
  logBezPamieci("tag=OTA-PANEL lvl=ERR zdarzenie=nieudana powod=%s proba=%u/%u koniec_prob=%d http=%d bajtow=%lu z=%ld etap=%c nvs=%d blad=\"%s\"\n",
                diag::otaPowodTekst(powod), (unsigned)otaZapis.prob, (unsigned)diag::OTA_MAX_PROB, (int)koniecProb,
                otaFetch.kodHttp(), (unsigned long)otaP.zapisano, otaFetch.dlugoscZapowiedziana(), otaFetch.etapBledu(),
                (int)zapisanoStan, opis != nullptr ? opis : "");
  otaSprzatnijPoProbie();
}

// Proba przerwana nie z winy pliku ani sieci (alarm, oferta wycofana) - nie liczy sie.
static void otaProbaPrzerwana(const char *dlaczego) {
  diag::otaZapisOddajProbe(otaZapis);
  otaZapisZachowaj();
  logBezPamieci("tag=OTA-PANEL lvl=WARN zdarzenie=przerwana powod=%s bajtow=%lu z=%ld msg=\"ta proba sie nie liczy\"\n",
                dlaczego, (unsigned long)otaP.zapisano, otaFetch.dlugoscZapowiedziana());
  otaSprzatnijPoProbie();
}

// Kolejny kawalek pliku od centrali (wolane z otaFetch.poll()). Nie wolno tu
// przerywac pobierania - blad jest zapamietywany, a reaguje otaKrokPobierania().
static void otaOdbiorcaBajtow(const uint8_t *dane, size_t n) {
  if (otaP.blad != diag::OTA_POWOD_BRAK) return;   // po bledzie reszta danych z tego obrotu przepada
  const size_t bylo = otaP.skan.lacznie;
  diag::otaSkanDodaj(otaP.skan, dane, n);
  if (otaP.skan.lacznie < diag::OTA_NAGLOWEK_B) return;   // naglowek pliku jeszcze niepelny - nic nie zapisujemy
  if (!otaP.zapisZaczety) {
    // Caly naglowek (24 bajty) jest: czy to w ogole program dla tego panelu?
    if (diag::otaSkanOcenNaglowek(otaP.skan) != diag::OTA_POWOD_BRAK) {
      otaP.blad = diag::OTA_POWOD_OBRAZ;
      otaP.ostateczny = true;
      otaP.opisBledu = "plik nie jest programem dla ESP32";
      return;
    }
    const long zapowiedziano = otaFetch.dlugoscZapowiedziana();
    if (zapowiedziano < (long)(diag::OTA_NAGLOWEK_B + diag::OTA_SUMA_B + 1)) {
      otaP.blad = diag::OTA_POWOD_POBIERANIE;
      otaP.opisBledu = "centrala nie podala rozmiaru pliku";
      return;
    }
    otaP.rozmiar = (uint32_t)zapowiedziano;
    if (!Update.begin((size_t)zapowiedziano)) {
      const bool zaDuzy = (Update.getError() == UPDATE_ERROR_SIZE);
      otaP.blad = zaDuzy ? diag::OTA_POWOD_ZA_DUZY : diag::OTA_POWOD_ZAPIS;
      otaP.ostateczny = zaDuzy;
      otaP.opisBledu = zaDuzy ? "plik wiekszy niz partycja panelu" : "Update.begin() nieudane";
      return;
    }
    otaP.zapisZaczety = true;
    if (Update.write(otaP.skan.naglowek, diag::OTA_NAGLOWEK_B) != diag::OTA_NAGLOWEK_B) {
      otaP.blad = diag::OTA_POWOD_ZAPIS;
      otaP.opisBledu = "zapis naglowka nieudany";
      return;
    }
    otaP.zapisano = (uint32_t)diag::OTA_NAGLOWEK_B;
    // Poczatek tego kawalka mogl nalezec do naglowka (juz zapisanego wyzej).
    const size_t zNaglowka = (bylo < diag::OTA_NAGLOWEK_B) ? (diag::OTA_NAGLOWEK_B - bylo) : 0;
    dane += zNaglowka;
    n -= zNaglowka;
  }
  if (n == 0) return;
  // Update.write() bierze wskaznik bez const, ale danych nie zmienia (kopiuje je do wlasnego bufora).
  if (Update.write(const_cast<uint8_t *>(dane), n) != n) {
    otaP.blad = diag::OTA_POWOD_ZAPIS;
    otaP.opisBledu = "zapis do flash nieudany";
    return;
  }
  otaP.zapisano += (uint32_t)n;
}

// Czy centrala nadal oferuje to samo, co panel wlasnie pobiera.
static bool otaOfertaBezZmian() {
  if (!stan.panelOtaDostepna) return false;
  char klucz[diag::OTA_KLUCZ_MAKS];
  diag::otaKlucz(klucz, sizeof(klucz), diag::otaObrazPoprawny(stan.panelOtaObraz), stan.panelOtaNr, stan.panelOtaObraz, stan.panelOtaWersja);
  return strcmp(klucz, otaP.klucz) == 0;
}

// Caly plik odebrany: sprawdzenie, uruchomienie, restart.
static void otaPoPobraniu() {
  if (!otaFetch.bylSukces()) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, otaFetch.ostatniBlad());
    return;
  }
  const int kod = otaFetch.kodHttp();
  if (kod == 503) {   // centrala 3.58.6+: trwa alarm
    otaProbaPrzerwana("alarm_centrali");
    return;
  }
  if (kod == 401 || kod == 403) {
    otaProbaNieudana(diag::OTA_POWOD_HASLO, true, "centrala odrzucila haslo OTA");
    return;
  }
  if (kod != 200) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, "centrala nie wydala pliku");
    return;
  }
  if (!otaP.zapisZaczety || !otaFetch.kompletna() || otaP.zapisano != otaP.rozmiar) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, "plik niepelny");
    return;
  }
  char obrazPliku[17];
  diag::otaSkanObraz(otaP.skan, obrazPliku);
  if (otaP.nowaCentrala && strcmp(obrazPliku, otaP.obrazOferty) != 0) {
    otaProbaNieudana(diag::OTA_POWOD_OBRAZ, true, "pobrany plik ma inny identyfikator niz oferta");
    return;
  }
  if (obrazPliku[0] != '\0' && strcmp(obrazPliku, otaObrazWlasny) == 0) {
    // Mozliwe tylko ze starsza centrala (nie podaje identyfikatora przed
    // pobraniem): to ten sam program, z ktorego panel juz pracuje. Nie ma
    // czego wgrywac ani po co sie restartowac.
    otaZapis.stan = diag::OTA_ZAPIS_DZIALA;
    otaZapis.powod = diag::OTA_POWOD_BRAK;
    otaZapisZachowaj();
    Serial.printf("tag=OTA-PANEL zdarzenie=ten_sam_plik obraz=%s msg=\"plik z centrali to program, z ktorego panel juz pracuje\"\n", obrazPliku);
    otaSprzatnijPoProbie();
    return;
  }
  lvgl_dashboard_poc_ota(100, "Sprawdzanie pliku...");
  lvgl_port_tick();
  // Update.end() dopisuje poczatek programu, sprawdza caly obraz (suma
  // kontrolna i SHA-256 - robi to ESP-IDF) i dopiero wtedy ustawia go jako
  // program startowy. Do tej chwili partycja z nowym plikiem nie da sie uruchomic.
  if (!Update.end(false)) {
    const bool zlyObraz = (Update.getError() == UPDATE_ERROR_ACTIVATE);
    otaP.zapisZaczety = false;   // Update sam porzucil zapis
    otaProbaNieudana(zlyObraz ? diag::OTA_POWOD_OBRAZ : diag::OTA_POWOD_ZAPIS, zlyObraz, Update.errorString());
    return;
  }
  otaP.zapisZaczety = false;
  diag::otaZapisWgrane(otaZapis, obrazPliku);
  const bool zapisanoStan = otaZapisZachowaj();
  Serial.printf("tag=OTA-PANEL zdarzenie=pobrano_ok obraz=%s bajtow=%lu trwalo_ms=%lu nvs=%d restart_za_chwile\n",
                obrazPliku[0] != '\0' ? obrazPliku : "-", (unsigned long)otaP.zapisano, (unsigned long)otaFetch.trwaloMs(), (int)zapisanoStan);
  otaFetch.skwituj();
  lvgl_dashboard_poc_ota(100, "Gotowe - restart panelu");
  for (int i = 0; i < 20; i++) {   // ok. 0,5 s, zeby napis zdazyl sie narysowac
    lvgl_port_tick();
    delay(25);
  }
  Serial.flush();
  ESP.restart();
}

// Trwa pobieranie: jeden obrot petli.
static void otaKrokPobierania() {
  otaFetch.poll();   // do 4 kB na obrot; kazdy kawalek od razu idzie do otaOdbiorcaBajtow()
  if (otaP.blad != diag::OTA_POWOD_BRAK) {
    otaProbaNieudana(otaP.blad, otaP.ostateczny, otaP.opisBledu);
    return;
  }
  // Alarm jest wazniejszy niz aktualizacja: ekran i brzeczyk maja byc wolne.
  if (stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel) {
    otaProbaPrzerwana("alarm");
    return;
  }
  if (!otaOfertaBezZmian()) {
    otaProbaPrzerwana("oferta_wycofana");
    return;
  }
  if (otaFetch.gotowyWynik()) {
    otaPoPobraniu();
    return;
  }
  if (otaP.rozmiar > 0) {
    const int procent = (int)(((uint64_t)otaP.zapisano * 100U) / otaP.rozmiar);
    if (procent != otaP.procentNaEkranie) {
      otaP.procentNaEkranie = procent;
      lvgl_dashboard_poc_ota(procent, "Pobieranie z centrali");
    }
  }
}

// Poczatek proby: zapis w NVS, potem dopiero polaczenie.
static void otaZacznij(const diag::OtaOferta &o) {
  char haslo[91];
  char sciezka[112];   // tyle miesci otaFetch (StrumienHttpGet::sciezka_)
  const bool hasloMiesciSie = diag::urlKoduj(OTA_HASLO_PANEL, haslo, sizeof(haslo));
  memset(&otaP, 0, sizeof(otaP));
  otaP.opisBledu = "";
  diag::otaKlucz(otaP.klucz, sizeof(otaP.klucz), o.nowaCentrala, o.nr, o.obraz, o.tag);
  otaP.nowaCentrala = o.nowaCentrala;
  snprintf(otaP.obrazOferty, sizeof(otaP.obrazOferty), "%s", o.nowaCentrala ? o.obraz : "");
  otaP.procentNaEkranie = -1;
  // Licznik prob rosnie i trafia do NVS ZANIM cokolwiek sie zacznie. Gdyby
  // panel zawieszal sie w trakcie pobierania, po trzech startach przestanie
  // probowac. Bez udanego zapisu nie zaczynamy - to on chroni przed petla.
  diag::otaZapisRozpocznijProbe(otaZapis, otaP.klucz);
  if (!otaZapisZachowaj()) {
    diag::otaZapisOddajProbe(otaZapis);
    otaBylaProba = true;
    otaKoniecProbyMs = millis();
    Serial.println(F("tag=OTA-PANEL lvl=ERR zdarzenie=brak_zapisu_nvs msg=\"nie udalo sie zapisac stanu aktualizacji - nie zaczynam\""));
    return;
  }
  Serial.printf("tag=OTA-PANEL zdarzenie=start wersja_docelowa=%s oferta=%s proba=%u/%u ram=%lu ram_blok=%lu\n",
                o.tag, otaP.klucz, (unsigned)otaZapis.prob, (unsigned)diag::OTA_MAX_PROB,
                (unsigned long)ramWolny(), (unsigned long)ramBlok());
  otaP.trwa = true;
  trwaAktualizacjaOtaPanelu = true;
  if (!hasloMiesciSie) {
    // Haslo OTA po zakodowaniu do adresu ma ponad 90 znakow - z tym firmware'em nie zadziala nigdy.
    otaProbaNieudana(diag::OTA_POWOD_HASLO, true, "haslo OTA za dlugie na adres zapytania");
    return;
  }
  snprintf(sciezka, sizeof(sciezka), "/panel-ota.bin?token=%s", haslo);
  lvgl_dashboard_poc_ota(0, "Łączenie z centralą...");
  if (!otaFetch.startBinarnie(sciezka, otaOdbiorcaBajtow, diag::OTA_CISZA_MS, diag::OTA_CALOSC_MS)) {
    otaProbaNieudana(diag::OTA_POWOD_POBIERANIE, false, otaFetch.ostatniBlad());
  }
}

// Raz na sekunde: co zrobic z tym, co centrala podaje w /api/status.
static void otaKrokDecyzji(unsigned long teraz) {
  static const char *poprzedniPowod = "brak_oferty";   // zwykly stan po starcie - bez wpisu w logu
  diag::OtaOferta o;
  o.dostepna = stan.panelOtaDostepna;
  o.nowaCentrala = diag::otaObrazPoprawny(stan.panelOtaObraz);
  o.nr = stan.panelOtaNr;
  o.obraz = stan.panelOtaObraz;
  o.tag = stan.panelOtaWersja;

  // Najpierw bez pomiarow: jesli wynik nie zalezy od warunkow (brak oferty,
  // ten sam program, oferta juz wgrana albo odrzucona), nie ma po co mierzyc
  // baterii co sekunde - starsza centrala potrafi trzymac oferte bez konca.
  diag::OtaWarunki w;
  memset(&w, 0, sizeof(w));
  w.daneSwieze = true;
  w.wifi = true;
  w.odStartuMs = diag::OTA_PO_STARCIE_MS;
  w.bezDotykuMs = diag::OTA_BEZ_DOTYKU_MS;
  w.zasilanieUsb = true;
  w.ramWolny = diag::OTA_RAM_MIN_B;
  w.ramBlok = diag::OTA_BLOK_MIN_B;
  diag::OtaDecyzja d = diag::otaDecyzja(o, otaObrazWlasny, otaZapis, w);
  if (d.akcja == diag::OTA_AKCJA_START) {
    const float napiecie = bateriaOdczytajNapiecie();
    w.alarm = stan.dymAlarm || stan.alarmOgrzewanie || stan.alarmPanel;
    w.daneSwieze = !swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS);
    w.wifi = (WiFi.status() == WL_CONNECTED);
    w.siecZajeta = wykresyFetch.wTrakcie() || komendaFetch.wTrakcie();
    w.odStartuMs = (uint32_t)teraz;
    w.bezDotykuMs = lvgl_dashboard_poc_bez_dotyku_ms();
    w.bateriaJest = diag::bateriaObecna(napiecie);
    w.zasilanieUsb = zasilanieUSB();
    w.bateriaProcent = w.bateriaJest ? bateriaNapiecieNaProcent(napiecie) : 0;
    w.ramWolny = ramWolny();
    w.ramBlok = ramBlok();
    w.bylaProba = otaBylaProba;
    w.odOstatniejProbyMs = (uint32_t)(teraz - otaKoniecProbyMs);
    d = diag::otaDecyzja(o, otaObrazWlasny, otaZapis, w);
  }

  // To, co pojdzie do centrali w nastepnych zapytaniach o status. Tylko
  // centrala 3.58.6+ (podaje identyfikator pliku) rozumie te parametry.
  otaZglosOdmowe = (d.akcja == diag::OTA_AKCJA_ODMOW && o.nowaCentrala);
  otaOdmowaNr = o.nr;
  otaOdmowaPowod = d.powodOdmowy;
  snprintf(otaCzekaPowod, sizeof(otaCzekaPowod), "%s", (d.akcja == diag::OTA_AKCJA_CZEKAJ && o.nowaCentrala) ? d.powod : "");

  // Jedna linia logu przy kazdej ZMIANIE decyzji, nie co sekunde. Teksty
  // powodow to stale z diagnostyka_panelu.h, wiec wskaznik mozna zapamietac.
  if (strcmp(d.powod, poprzedniPowod) != 0) {
    poprzedniPowod = d.powod;
    if (d.akcja != diag::OTA_AKCJA_START) {
      Serial.printf("tag=OTA-PANEL zdarzenie=decyzja akcja=%s powod=%s oferta=%s nr=%lu ram=%lu ram_blok=%lu\n",
                    d.akcja == diag::OTA_AKCJA_NIC ? "nic" : d.akcja == diag::OTA_AKCJA_CZEKAJ ? "czekaj"
                    : d.akcja == diag::OTA_AKCJA_POMIN ? "pomin" : "odmow",
                    d.powod, o.dostepna ? o.tag : "-", (unsigned long)o.nr, (unsigned long)ramWolny(), (unsigned long)ramBlok());
    }
  }
  if (d.akcja == diag::OTA_AKCJA_START) otaZacznij(o);
}

// Wolane w kazdym obrocie loop() (galaz LVGL z centrala).
// ============================================================================
// [v1.5.4 OTA-VSCODE] WGRYWANIE Z PLATFORMIO PRZEZ WIFI (ArduinoOTA, port 3232)
// ============================================================================
// Komputer w sieci centrali (PiecCO_AP): `pio run -e esp32dev_wifi -t upload`
// (VSCode: PlatformIO -> esp32dev_wifi -> Upload). Panel musi byc obudzony i
// polaczony. Biblioteka blokuje petle na czas odbioru (kilkanascie sekund) -
// LVGL stoi, wiec pasek rysujemy sami z onProgress() przez lvgl_port_tick().
static bool otaVscodeGotowe = false;
static int  otaVscodeProcentNaEkranie = -1;

static void otaVscodeKrok() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!otaVscodeGotowe) {
    ArduinoOTA.setHostname("panelco");
    ArduinoOTA.setPassword(OTA_HASLO_PANEL);
    ArduinoOTA.setMdnsEnabled(false);   // wgrywanie po IP (192.168.4.50); mDNS to osobne zadanie i ~10 kB RAM
    ArduinoOTA.onStart([]() {
      esp_task_wdt_reset();
      trwaAktualizacjaOtaPanelu = true;   // jak przy aktualizacji z centrali: bez snu, bez ratunkowego restartu
      otaVscodeProcentNaEkranie = -1;
      if (senPrzyciemnione) senPrzywrocJasnosc("ota_vscode");   // na baterii po przyciemnieniu: pasek ma byc widoczny
      Serial.printf("tag=OTA-VSCODE zdarzenie=start typ=%s napiecie=%.2fV\n",
                    ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem", (double)bateriaOstatnieV);
      lvgl_dashboard_poc_ota(0, "Wgrywanie z komputera");
      lvgl_port_tick();
    });
    ArduinoOTA.onProgress([](unsigned int postep, unsigned int razem) {
      esp_task_wdt_reset();
      const int procent = razem > 0 ? (int)((uint64_t)postep * 100 / razem) : 0;
      if (procent == otaVscodeProcentNaEkranie) return;
      otaVscodeProcentNaEkranie = procent;
      if (procent % 10 == 0) Serial.printf("tag=OTA-VSCODE zdarzenie=postep procent=%d bajtow=%u\n", procent, postep);
      lvgl_dashboard_poc_ota(procent, "Wgrywanie z komputera");
      lvgl_port_tick();
    });
    ArduinoOTA.onEnd([]() {
      esp_task_wdt_reset();
      Serial.println(F("tag=OTA-VSCODE zdarzenie=koniec msg=\"restart, nowy program czeka na zatwierdzenie\""));
      lvgl_dashboard_poc_ota(100, "Gotowe - restart panelu");
      lvgl_port_tick();
    });
    ArduinoOTA.onError([](ota_error_t blad) {
      const char* opis = "nieznany";
      if (blad == OTA_AUTH_ERROR) opis = "zle_haslo";
      else if (blad == OTA_BEGIN_ERROR) opis = "begin_failed";
      else if (blad == OTA_CONNECT_ERROR) opis = "connect_failed";
      else if (blad == OTA_RECEIVE_ERROR) opis = "receive_failed";
      else if (blad == OTA_END_ERROR) opis = "end_failed";
      Serial.printf("tag=OTA-VSCODE lvl=ERR zdarzenie=blad kod=%d opis=%s\n", (int)blad, opis);
      trwaAktualizacjaOtaPanelu = false;
      lvgl_dashboard_poc_ota(-1, nullptr);   // z powrotem pulpit, panel pracuje dalej
    });
    ArduinoOTA.begin();
    otaVscodeGotowe = true;
    Serial.printf("tag=OTA-VSCODE zdarzenie=gotowe ip=%s port=3232 msg=\"pio run -e esp32dev_wifi -t upload\"\n", WiFi.localIP().toString().c_str());
  }
  ArduinoOTA.handle();
}

void otaKrok() {
  if (otaP.trwa) {
    otaKrokPobierania();
    return;
  }
  if (otaObrazWlasny[0] == '\0') return;   // bez identyfikatora wlasnego programu panel nie aktualizuje sie przez centrale
  const unsigned long teraz = millis();
  static unsigned long ostatniaDecyzjaMs = 0;
  if (teraz - ostatniaDecyzjaMs < 1000) return;
  ostatniaDecyzjaMs = teraz;
  otaKrokDecyzji(teraz);
}

unsigned long ostatniStatus = 0;

// [v1.4.1 ZRZUT] Skrot zrzutu po awarii. Biblioteki Arduino dla ESP32 maja
// wlaczony zapis zrzutu (core dump) do flash w formacie ELF
// (CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH, CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF),
// a partitions.csv panelu ma na to partycje "coredump" (64 kB). Po panice
// (powod restartu "panic", takze czesc "int_wdt"/"task_wdt") system zapisuje
// tam stan zadan; dotad nikt tego nie czytal. Ta funkcja, wolana raz po
// starcie: sprawdza, czy zrzut jest i jest poprawny; wypisuje jedna linie
// z tym, co najwazniejsze (nazwa zadania, adres bledu, powod paniki, do
// 12 adresow ze sladu wywolan); kasuje zrzut, zeby kolejna awaria miala
// miejsce. Adresy tlumaczy sie na funkcje i linie kodu poleceniem:
//   xtensa-esp32-elf-addr2line -pfiaC -e .pio/build/esp32dev/firmware.elf <adresy>
// z plikiem .elf z TEGO SAMEGO budowania, ktore jest na plytce (po kazdym
// wgraniu warto skopiowac .elf obok pliku .bin). Zrzut nie jest wysylany do
// centrali - tylko log przez USB.
// Bez wlaczonego zrzutu w bibliotekach (inna platforma) funkcja wypisuje
// "niedostepny" i nic wiecej nie robi.
void zrzutPoAwariiWypisz() {
#if defined(CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH) && defined(CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF)
  const esp_err_t jest = esp_core_dump_image_check();
  if (jest == ESP_ERR_NOT_FOUND || jest == ESP_ERR_INVALID_SIZE) {
    return;   // partycja pusta (najczesciej) - cisza, zeby nie zasmiecac logu przy kazdym starcie
  }
  if (jest != ESP_OK) {
    // Zrzut jest, ale nie przeszedl sprawdzenia (np. przerwany zapis, inna
    // wersja). Skasuj, zeby nie wracac do tego przy kazdym starcie.
    Serial.printf("tag=ZRZUT lvl=WARN zdarzenie=uszkodzony kod=%s\n", esp_err_to_name(jest));
    esp_core_dump_image_erase();
    return;
  }
  // Struktura ma kilkaset bajtow - na stosie setup() jest na to miejsce (8 kB),
  // ale malloc() jest bezpieczniejszy, bo przy starcie pamieci jest najwiecej.
  esp_core_dump_summary_t* skrot = (esp_core_dump_summary_t*)malloc(sizeof(esp_core_dump_summary_t));
  if (skrot == nullptr) {
    Serial.println("tag=ZRZUT lvl=WARN zdarzenie=brak_pamieci_na_skrot");
    return;   // zrzut zostaje - moze sie uda przy nastepnym starcie
  }
  memset(skrot, 0, sizeof(*skrot));
  const esp_err_t odczyt = esp_core_dump_get_summary(skrot);
  char powod[120];
  powod[0] = '\0';
  if (esp_core_dump_get_panic_reason(powod, sizeof(powod)) != ESP_OK) powod[0] = '\0';
  if (odczyt == ESP_OK) {
    skrot->exc_task[sizeof(skrot->exc_task) - 1] = '\0';
    // Slad: do 12 adresow oddzielonych przecinkami, bez spacji (jedno pole logu).
    char slad[12 * 11 + 1];
    size_t poz = 0;
    const uint32_t glebokosc = skrot->exc_bt_info.depth;
    for (uint32_t i = 0; i < glebokosc && i < 12 && poz + 11 < sizeof(slad); i++) {
      poz += (size_t)snprintf(slad + poz, sizeof(slad) - poz, "%s0x%08lx", i ? "," : "", (unsigned long)skrot->exc_bt_info.bt[i]);
    }
    if (poz == 0) strcpy(slad, "-");
    logBezPamieci("tag=ZRZUT lvl=ERR zdarzenie=po_awarii zadanie=%s pc=0x%08lx powod=\"%s\" slad_n=%lu slad_uszkodzony=%d slad=%s\n",
                  skrot->exc_task, (unsigned long)skrot->exc_pc, powod,
                  (unsigned long)glebokosc, (int)skrot->exc_bt_info.corrupted, slad);
  } else {
    Serial.printf("tag=ZRZUT lvl=WARN zdarzenie=skrot_nieczytelny kod=%s powod=\"%s\"\n", esp_err_to_name(odczyt), powod);
  }
  free(skrot);
  const esp_err_t skas = esp_core_dump_image_erase();
  Serial.printf("tag=ZRZUT zdarzenie=skasowany wynik=%s\n", esp_err_to_name(skas));
#else
  Serial.println("tag=ZRZUT zdarzenie=niedostepny powod=brak_obslugi_w_bibliotekach");
#endif
}

// [RESET-DIAG, ta sesja] Mapowanie esp_reset_reason_t na krotki, czytelny
// tekst. Odczytywane jako PIERWSZA rzecz w setup() (patrz nizej) — samo
// odczytanie stanu chipu nie zalezy od niczego innego, wiec robimy to
// zanim cokolwiek innego (WDT, WiFi, TFT) zdazy cokolwiek popsuc. Cel
// z TEMATU B (OTA_PANEL_CALOSC.md): dzis ESP_RST_BROWNOUT i
// ESP_RST_TASK_WDT objawiaja sie identycznie ("panel sie zresetowal"),
// a to dwie zupelnie rozne diagnozy — brownout to problem zasilania
// (kondensator/bateria), task_wdt to zawieszony task (np. podczas OTA,
// patrz Ustalenie #6). Dwa NORMALNE powody dla tego urzadzenia:
// ESP_RST_DEEPSLEEP (budzenie przyciskiem/timerem) i ESP_RST_SW
// (ESP.restart() po udanym OTA panelu, patrz otaPoPobraniu() wyzej)
// — logika alarmujaca po stronie centrali (main_centrala.cpp,
// handler /api/status) traktuje oba jako nie wymagajace alertu.
// [v1.2.1] Drugie wywolanie ESP.restart() jest w panelAutorestart() (brak
// danych z centrali) - taki restart panel zglasza centrali osobnym powodem
// "auto_...", patrz autorestartOdczytajPrzyStarcie() nizej.
const char* powodResetuTekst(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "poweron";     // pelna utrata zasilania (bateria padla / wymieniona)
    case ESP_RST_EXT:       return "ext";          // reset pinem EN
    case ESP_RST_SW:        return "sw";           // ESP.restart() z kodu (np. po udanym OTA)
    case ESP_RST_PANIC:     return "panic";        // wyjatek/crash (nie WDT)
    case ESP_RST_INT_WDT:   return "int_wdt";      // watchdog przerwan (rozne od task_wdt)
    case ESP_RST_TASK_WDT:  return "task_wdt";     // TEN watchdog skonfigurowalismy wyzej — zawieszony task
    case ESP_RST_WDT:       return "wdt";          // inny/ogolny watchdog (RTC WDT itp.)
    case ESP_RST_DEEPSLEEP: return "deepsleep";    // NORMALNE wybudzenie — jedyny oczekiwany powod
    case ESP_RST_BROWNOUT:  return "brownout";     // spadek napiecia ponizej progu — TEMAT B
    case ESP_RST_SDIO:      return "sdio";
    default:                return "unknown";
  }
}

// [v1.2.1 RATUNEK] Wolane raz, na poczatku setup(). Sprawdza, czy ten start
// nastapil po SAMOCZYNNYM restarcie (panelAutorestart()). Jesli tak, powod
// zglaszany centrali w /api/status to nie "sw", tylko token z przyczyna, np.
// "auto_milczy_r0_n1_t45_h61_b31_el0" (diag::tokenAutorestartu()) - centrala
// wpisuje go do swojego logu (tag=RESET-DIAG zdarzenie=nietypowy_restart_panelu)
// i wysyla o nim wiadomosc, tak jak o kazdym restarcie innym niz "sw" i
// "deepsleep". Restart po udanym OTA zostaje "sw" (oczekuje == 0).
// Centrala oglasza powod przy kazdej ZMIANIE zglaszanej wartosci, dlatego:
// - token idzie tylko do pierwszej odpowiedzi centrali, potem panel wraca do
//   "sw" (obslugaStatusFetch()) - inaczej centrala po WLASNYM restarcie
//   oglaszalaby od nowa stary restart panelu;
// - nie kazdy restart z serii jest zglaszany (diag::zglosicRestart()).
void autorestartOdczytajPrzyStarcie(esp_reset_reason_t powod) {
  if (!rtcAutorestart.wazna()) {
    rtcAutorestart.wyczysc();   // po wlaczeniu zasilania w tej pamieci sa przypadkowe wartosci
    return;
  }
  if (powod == ESP_RST_SW && rtcAutorestart.oczekuje) {
    startPoAutorestarcie = true;
    char token[diag::TOKEN_AUTORESTARTU_MAKS];
    diag::tokenAutorestartu(token, sizeof(token), rtcAutorestart);
    // Nie kazdy restart z serii jest zglaszany (diag::zglosicRestart()) -
    // pominiety idzie do centrali jako zwykle "sw", ktorego ona nie oglasza.
    autorestartDoZgloszenia = diag::zglosicRestart(rtcAutorestart.seria, rtcAutorestart.zgloszonyNr);
    if (autorestartDoZgloszenia) globalnyPowodResetu = token;
    Serial.printf("tag=PANEL lvl=WARN zdarzenie=start_po_autorestarcie nr_z_rzedu=%u w_serii=%u zgloszony_nr=%u lacznie=%u przyczyna=%s bez_danych_s=%u "
                  "rozl_kod=%u rozl_nazwa=%s heap_kb=%u blok_kb=%u zglaszam_centrali=%s\n",
                  (unsigned)rtcAutorestart.kolejnych, (unsigned)rtcAutorestart.seria, (unsigned)rtcAutorestart.zgloszonyNr,
                  (unsigned)rtcAutorestart.lacznie,
                  diag::przyczynaSkrot((diag::Przyczyna)rtcAutorestart.przyczyna), (unsigned)rtcAutorestart.bezDanychS,
                  (unsigned)rtcAutorestart.kodRozlaczenia, diag::nazwaKoduRozlaczenia(rtcAutorestart.kodRozlaczenia),
                  (unsigned)rtcAutorestart.heapKb, (unsigned)rtcAutorestart.blokKb, globalnyPowodResetu.c_str());
  } else if (powod != ESP_RST_SW) {
    rtcAutorestart.kolejnych = 0;   // restart z innego powodu (przycisk, watchdog, zasilanie) - licz od nowa
  }
  rtcAutorestart.oczekuje = 0;
  rtcAutorestart.zapieczetuj();
}

// [v1.2.1 RATUNEK] Po samoczynnym restarcie: ostatni znany stan alarmow
// (zapisany przez panelAutorestart()) wraca do `stan` i do brzeczyka, razem z
// tym, czy uzytkownik go wyciszyl. Bez tego restart z braku danych w trakcie
// alarmu po cichu wylaczalby dzwiek. Ekran alarmu przywraca osobno
// lvgl_dashboard_poc_przywroc_alarm() - dopiero gdy LVGL jest juz zbudowany.
// Pierwszy poprawny status z centrali i tak ustawia wszystko od nowa.
void alarmPrzywrocPoAutorestarcie() {
  if (!startPoAutorestarcie) return;
  const uint8_t a = rtcAutorestart.alarmy;
  stan.dymAlarm = diag::alarmyDym(a);
  stan.alarmOgrzewanie = diag::alarmyOgrzewanie(a);
  stan.alarmPanel = diag::alarmyPanel(a);
  g_brzeczykWyciszony = diag::alarmyWyciszony(a);
  g_brzeczykWyciszonyTyp = (BrzeczykAlarmTyp)diag::alarmyWyciszonyTyp(a);
  if (!diag::alarmyJakikolwiek(a)) return;
  Serial.printf("tag=PANEL lvl=WARN zdarzenie=alarm_sprzed_restartu dym=%d ogrzewanie=%d panel=%d wyciszony=%d "
                "msg=\"ostatni znany stan alarmu - obowiazuje do pierwszych danych z centrali\"\n",
                (int)stan.dymAlarm, (int)stan.alarmOgrzewanie, (int)stan.alarmPanel, (int)g_brzeczykWyciszony);
  brzeczykOdswiezZAlarmow(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel);
}

void setup() {
  // [v1.5.5] Kanal polecen: 1000 ms na polaczenie i 2000 ms na calosc (statusy
  // zostaja przy 400/1500 ms z SiecCfg) + do 3 prob w poleceniaKrok(). Patrz
  // diag::KolejkaPolecen::PROB_MAKS i HISTORIA ZMIAN v1.5.5.
  komendaFetch.ustawLimity(KOMENDA_LACZENIE_MS, KOMENDA_CALOSC_MS);
  // [FIX, ta sesja - PISKNIECIE BRZECZYKA PRZY WLACZENIU ZASILANIA] Dopoki
  // pin nie zostanie jawnie skonfigurowany, jest floating (Hi-Z) - dziala
  // jak antena na szum elektryczny (rozruch WiFi/TFT/LVGL/reszty sprzetu),
  // ktory pasywny piezo potrafi slyszalnie "zlapac" jako trzask/pisk.
  // Docelowa PELNA inicjalizacja (ledcAttach+ledcWriteTone) jest w
  // brzeczykZainicjuj(), wolanej pozniej w tej funkcji - ALE dopiero PO
  // tft.init()/kalibracji dotyku/LVGL init, czyli pin floatowalby przez
  // caly ten czas (realnie liczony w sekundach, nie mikrosekundach - stad
  // "piszczy przez pewien moment"). Te 2 linie na samej gorze setup(),
  // PRZED wszystkim innym (nawet Serial.begin()), zamykaja pin na LOW w
  // ciagu pojedynczych mikrosekund od startu programu - to maksimum tego,
  // co da sie zrobic programowo. Fragment SPRZED setup() (bootloader ROM
  // ESP32, dziala zanim jakikolwiek kod Arduino w ogole ruszy) nie jest pod
  // kontrola tego kodu - jesli po tej zmianie pisk nadal sie sluchowo
  // slyszy (bedzie duzo krotszy), jedyny dalszy krok to zewnetrzny
  // rezystor podciagajacy w dol (10-100k, pin->GND) na linii IO buzzera,
  // fizycznie trzymajacy ja na LOW nawet zanim jakikolwiek kod wystartuje.
  // [v1.5.0 SEN] Po wybudzeniu z glebokiego snu pin brzeczyka ma wlaczony
  // "hold" z idzSpac() (gpio_hold_en) - bez zdjecia go pinMode()/ledcAttach()
  // nizej nic by nie zmienily i alarm bylby niemy az do odlaczenia zasilania.
  // Przy zwyklym starcie (hold nigdy nie byl wlaczony) to no-op. Dla TFT_BL to
  // samo robi gpio_hold_dis() przed ledcAttach() dalej w setup().
  gpio_hold_dis((gpio_num_t)BRZECZYK_PIN);
  pinMode(BRZECZYK_PIN, OUTPUT);
  digitalWrite(BRZECZYK_PIN, LOW);

  Serial.begin(115200);
  delay(300);
  Serial.println("=== WYSWIETLACZ CO - start ===");

  // [RESET-DIAG, ta sesja] Odczyt PRZED wszystkim innym (WDT, WiFi, TFT) —
  // to czysty odczyt stanu chipu, nic od niczego innego nie zalezy, a
  // chcemy znac powod restartu nawet gdyby cos ponizej samo padlo.
  globalnyPowodResetu = powodResetuTekst(esp_reset_reason());
  autorestartOdczytajPrzyStarcie(esp_reset_reason());   // [v1.2.1 RATUNEK] po samoczynnym restarcie: "auto_..." zamiast "sw"
  otaPrzyStarcie();   // [v1.4.0 OTA] identyfikator wlasnego programu i zapis o ostatniej ofercie - przed pierwszym zapytaniem o status
  Serial.printf("tag=RESET-DIAG zdarzenie=start powod=%s\n", globalnyPowodResetu.c_str());
  if (globalnyPowodResetu != "deepsleep") {
    // Jedyny NORMALNY powod dla tego urzadzenia to wybudzenie z deep
    // sleep — wszystko inne jest warte podniesionego glosu w logu.
    Serial.printf("tag=RESET-DIAG lvl=WARN zdarzenie=nietypowy_restart powod=%s\n", globalnyPowodResetu.c_str());
  }
  // [v1.4.1 ZRZUT] Jesli poprzedni program padl z panika, w partycji coredump
  // lezy zrzut - jedna linia "tag=ZRZUT" ze skrotem i kasowanie. Tutaj, zaraz
  // po powodzie restartu: czysty odczyt flash, przed WiFi/TFT/LVGL.
  zrzutPoAwariiWypisz();
  // [v1.4.1 PAMIEC] Od tej chwili kazda odmowa pamieci jest liczona
  // (alok_nieudane= w linii "tag=PANEL") - opis przy alokNieudanaHak().
  heap_caps_register_failed_alloc_callback(alokNieudanaHak);

  // [FIX-WDT] Task Watchdog Timer — panel dotad NIE MIAL zadnej jawnej
  // konfiguracji TWDT (w odroznieniu od main_centrala.cpp). Bez tego
  // domyslne zachowanie TWDT po timeoutcie to TYLKO log ostrzezenia, BEZ
  // paniki/restartu (patrz Ustalenie #6 w OTA_PANEL_CALOSC.md,
  // potwierdzone dokumentacja ESP-IDF: CONFIG_ESP_TASK_WDT_PANIC musi byc
  // jawnie wlaczone) — czyli realne zawieszenie panelu (np. w trakcie OTA,
  // patrz prompt #7) mogloby NIE dawac samoistnego restartu. Ten sam
  // wzorzec co w centrali: reconfigure (bo TWDT jest juz init domyslnie
  // przez framework) + add(NULL) na tasku setup()/loop() (loopTask).
  {
    esp_task_wdt_config_t wdtCfg = {
      .timeout_ms = 15000,   // ta sama wartosc co WDT_TIMEOUT_MS w centrali
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_err_t wdtErr = esp_task_wdt_reconfigure(&wdtCfg);
    if (wdtErr != ESP_OK) {
      Serial.printf("tag=WDT lvl=ERR zdarzenie=reconfigure blad=%s\n", esp_err_to_name(wdtErr));
    }
    wdtErr = esp_task_wdt_add(NULL);
    if (wdtErr != ESP_OK) {
      Serial.printf("tag=WDT lvl=ERR zdarzenie=add_loopTask blad=%s\n", esp_err_to_name(wdtErr));
    }
    Serial.printf("tag=WDT zdarzenie=skonfigurowany timeout=15000ms panic=true obserwowany=loopTask\n");
  }

  // [ETAP 3b] Log powodu startu — odroznia normalny start (USB/reset) od
  // wybudzenia przyciskiem po deep sleep, przydatne przy diagnostyce.
  esp_sleep_wakeup_cause_t powod = esp_sleep_get_wakeup_cause();
  startPoSnie = (powod == ESP_SLEEP_WAKEUP_EXT0 || powod == ESP_SLEEP_WAKEUP_EXT1);   // [v1.5.1] pulpit zamiast ekranu startowego
  if (powod == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println(F("tag=ZASILANIE zdarzenie=wybudzenie zrodlo=przycisk"));
  } else if (powod == ESP_SLEEP_WAKEUP_EXT1) {
    Serial.println(F("tag=ZASILANIE zdarzenie=wybudzenie zrodlo=usb"));   // [v1.5.0 SEN] wlozona ladowarka (GPIO35)
  } else {
    Serial.println(F("tag=ZASILANIE zdarzenie=start zrodlo=reset_lub_usb"));
  }

  pinMode(WAKE_BTN_PIN, INPUT_PULLUP); // ten sam pin uzywany do wykrywania puszczenia w idzSpac()
  // [v1.4.6] INPUT, nie INPUT_PULLUP: GPIO35 jest pinem tylko-wejsciowym
  // bez wewnetrznego pull-upu (ESP32, pady 34-39). INPUT_PULLUP dawal tylko
  // "E gpio: gpio_pullup_en(85): GPIO number error (input-only pad has no
  // internal PU)" w logu przy kazdym starcie (log z plytki 2026-11-19) i nie
  // podciagal niczego. Jedyny pull-up to ZEWNETRZNE 10k do 3V3 - patrz
  // komentarz przy USB_DETECT_PIN; bez niego pin plywa i zasilanieUSB()
  // klamie. TP4056 (open-drain) i tak wygrywa (LOW), gdy jest aktywny.
  pinMode(USB_DETECT_PIN, INPUT);

  // [BATERIA-POMIAR, ta sesja] Raz, globalnie - analogReadMilliVolts() w
  // bateriaOdczytajNapiecie() honoruje TYLKO to ustawienie (nie wersje
  // per-pin), patrz komentarz przy tej funkcji.
  analogSetAttenuation(ADC_11db);

  touchInicjalizujPiny();
  tft.init();
  tft.setRotation(EKRAN_OBROT);   // [v1.5.2] 3 = pozioma 480x320 obrocona o 180 stopni (zlacze po lewej); bylo 1
  tft.fillScreen(KOL_TLO);

  // [FIX-TFT-BL-HOLD, ta sesja] Odblokuj pin TFT_BL PRZED ledcAttach() -
  // po kazdym wybudzeniu z deep sleep pin nadal ma wlaczony "hold" z
  // idzSpac() (patrz gpio_hold_en() tam). Bez tego wywolania PWM nie
  // ruszyloby wcale, bo pin zostalby "zamrozony" na stale, takze po
  // kolejnych resetach. Bezpieczne do wywolania rowniez przy zwyklym
  // starcie (reset/USB), gdy hold nigdy nie byl wlaczony - wtedy to no-op.
  gpio_hold_dis((gpio_num_t)TFT_BL);

  ledcAttach(TFT_BL, 5000, 8);       // PWM 5kHz, 8-bit na podswietleniu
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Wczytaj zapisana jasnosc/
  // harmonogram PRZED pierwszym ustawJasnoscPodswietlenia() - startujemy od
  // razu z ostatnia znana jasnoscia DZIENNA usera (nie na sztywno 100%).
  // Godzina jest nieznana az do 1. pobierzStatus() nizej
  // (ta plytka nie ma wlasnego RTC) - wyswietlaczHarmonogramOdswiez() wolane
  // stamtad skoryguje na jasnosc NOCNA w te kilkadziesiat ms, jesli akurat
  // trwa okno przyciemnienia.
  wyswietlaczWczytajZPamieci();
  ustawJasnoscPodswietlenia(jasnoscDziennaWartosc);
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Czysto sprzetowa
  // inicjalizacja (PWM na pinie), wiec JEDNO wywolanie tutaj wystarcza dla
  // obu galezi ponizej (symulacja/prawdziwa centrala) - w odroznieniu od
  // rejestracji callbackow WYSWIETLACZ, tu nie ma niczego do zarejestrowania
  // osobno w kazdej galezi.
  brzeczykZainicjuj();
  alarmPrzywrocPoAutorestarcie();   // [v1.2.1 RATUNEK] alarm trwajacy przed samoczynnym restartem gra dalej

  // [FIX - dotyk] Kalibracja MUSI byc zrobiona przed uzyciem dotykOdczytaj()
  // (ktorego uzywa lvgl_touch_read() w lvgl_port.cpp) - inaczej kalDotyku
  // zostaje z zerami, dotykOdczytaj() widzi "zdegenerowana" kalibracje
  // (roznica raw < 50) i ZAWSZE zwraca false, wiec LVGL nigdy nie widzi
  // zadnego dotyku. Jesli kalibracja jest juz zapisana w Preferences
  // (namespace "dotyk"), to tylko szybki odczyt, bez interakcji.
  kalibrujDotykJesliTrzeba();

  // Interfejs LVGL + prawdziwe WiFi/API centrali (patrz pompaManualWyslij() wyzej).
  Serial.println(F("=== LVGL + WiFi/API centrali ==="));
  lvgl_port_init();
  lvgl_dashboard_poc_init();
  // [v1.2.2 PAMIEC] Ile RAM-u zostaje po zbudowaniu ekranow, zanim wystartuje
  // WiFi (ktore bierze kolejne kilkadziesiat kB).
  Serial.printf("tag=PAMIEC zdarzenie=po_ekranach ram=%lu ram_blok=%lu\n", (unsigned long)ramWolny(), (unsigned long)ramBlok());
  lvgl_dashboard_poc_ustaw_callback_pompy(pompaManualWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyboru(pompaWyborWyslij);
  // [POPRAWKA, ta sesja - panel SERWO v2] Brakujaca rejestracja (ta sama
  // przyczyna co w galezi symulacji wyzej) - podpina panel SERWO pod
  // serwoTrybWyslij()/serwoPozycjaWyslij() (patrz definicje przy
  // pompaWyborWyslij()), ktore wolaja juz istniejace
  // wyslijTrybPracy()/wyslijServoManualny().
  lvgl_dashboard_poc_ustaw_callback_serwo_tryb(serwoTrybWyslij);
  lvgl_dashboard_poc_ustaw_callback_serwo_pozycja(serwoPozycjaWyslij);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA]
  lvgl_dashboard_poc_ustaw_callback_serwo_ustawienia(serwoUstawienieWyslij);
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA POMPY]
  lvgl_dashboard_poc_ustaw_callback_pompa_ustawienia(pompaUstawienieWyslij);
  lvgl_dashboard_poc_ustaw_callback_pompa_antystop(pompaAntystopWyslij);
  // [ROZSZERZENIE, ta sesja - PANEL DYM]
  lvgl_dashboard_poc_ustaw_callback_dym_prog(dymProgWyslij);
  lvgl_dashboard_poc_ustaw_callback_dym_ustawienia(dymUstawienieWyslij);
  // [ROZSZERZENIE, ta sesja - PANEL PROG PRZEGRZANIA]
  lvgl_dashboard_poc_ustaw_callback_alarm_temp_prog(alarmTempProgWyslij);
  // [ROZSZERZENIE, ta sesja - EKRAN WYKRESY; v1.3.0] Patrz wykresyFetchStart():
  // ekran Wykresy prosi tedy o dane, a pobieranie popycha obslugaWykresyFetch()
  // w loop().
  lvgl_dashboard_poc_ustaw_callback_wykresy_fetch(wykresyFetchStart);
  // [ROZSZERZENIE, ta sesja - PANEL WYSWIETLACZ] Ten sam wzorzec co galaz
  // symulacji wyzej (patrz komentarz tam) - TE SAME 3 funkcje w obu
  // galeziach, bo podswietlenie tej plytki nie zalezy od tego, czy centrala
  // jest akurat podlaczona.
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_jasnosc(wyswietlaczJasnoscWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_harmonogram(wyswietlaczHarmonogramWyslij);
  lvgl_dashboard_poc_ustaw_callback_wyswietlacz_ustawienia(wyswietlaczUstawieniaWyslij);
  // [EKRAN ALARMOWY, ta sesja] Ten sam powod co WYSWIETLACZ powyzej - brzeczyk
  // jest lokalny (GPIO tej plytki), wiec dziala identycznie w obu galeziach.
  lvgl_dashboard_poc_ustaw_callback_alarm_wycisz(alarmWyciszObsluga);
  lvgl_dashboard_poc_aktualizuj_ustawienia_wyswietlacza(jasnoscDziennaWartosc, jasnoscNocnaWartosc,
                                                         godzinaStartPrzyciemnienia, godzinaKoniecPrzyciemnienia,
                                                         harmonogramWlaczonyWartosc);
  // [v1.4.7 ZASILANIE] Lokalne jak podswietlenie (NVS + sprzet tej plytki) - callback i zapisane indeksy.
  lvgl_dashboard_poc_ustaw_callback_zasilanie(zasilanieWyslij);
  lvgl_dashboard_poc_aktualizuj_zasilanie(zasilanieCpuIdx, zasilanieTxIdx);
  lvgl_dashboard_poc_aktualizuj_sen(senPrzyciemnijIdx, senUspijIdx);   // [v1.5.1] wiersz "Na baterii" na ekranie WYSWIETLACZ
  // [v1.2.1 RATUNEK] Po samoczynnym restarcie w trakcie alarmu: ekran alarmu
  // (albo dzwonek w naglowku, jesli byl zamkniety) i czerwone ramki kafelkow
  // wracaja, zanim przyjda pierwsze dane.
  if (startPoAutorestarcie && diag::alarmyJakikolwiek(rtcAutorestart.alarmy)) {
    lvgl_dashboard_poc_przywroc_alarm(stan.dymAlarm, stan.alarmOgrzewanie, stan.alarmPanel,
                                      diag::alarmyOdrzucony(rtcAutorestart.alarmy));
  }

  polaczWifi();
  pobierzStatus();
  // [ROZSZERZENIE, ta sesja - ekran USTAWIENIA SERWA] W trybie LVGL nie ma
  // juz starego EKRAN_USTAWIENIA, ktory wczesniej wolal pobierzUstawienia()
  // przy starcie (patrz galaz nie-LVGL nizej) - bez tego wywolania tutaj
  // ekran USTAWIENIA SERWA pokazywalby same zera az do pierwszej recznej
  // zmiany. Wola tez lvgl_dashboard_poc_aktualizuj_ustawienia_serwa()
  // (patrz koniec obslugaUstawieniaFetch()).
  pobierzUstawienia();
  // [SIEC-NIEBLOKUJACA, ta sesja] Oba GET-y wystartowaly, ale jeszcze sie
  // nie skonczyly - patrz obszerny komentarz przy poczekajNaPierwszeDane().
  poczekajNaPierwszeDane();
  ekranStartowyWersje();   // [v1.4.6] "Centrala v3.58.6" pod napisami o laczeniu, chwila na przeczytanie
  // aktualizujSezon() zostaje - `aktualnySezon` jest nadal uzywany gdzie
  // indziej w tym pliku (np. ikona slonca na starym dashboardzie), tylko
  // przestal byc przekazywany do LVGL (patrz FIX w dashboard_lvgl_poc.h -
  // podzial sezonowy kafelkow LVGL zostal usuniety).
  aktualizujSezon();
  lvgl_dashboard_poc_aktualizuj(stan.polaczono, stan.tZewn, stan.tBojler,
                                 stan.tOgrz, stan.tOgrzSr, stan.tPanel,
                                 stan.pompa, stan.pompaOverrideMin,
                                 stan.dym, stan.dymAlarm,
                                 stan.tPokoj, stan.wilgotnosc, stan.cisnienie,
                                 diag::serwoStopnieNaProcent(stan.klapa, diag::KLAPA_KAT_MAKS), diag::serwoStopnieNaProcent(stan.syberka, diag::SYBERKA_KAT_MAKS), (int)trybWyboru,   // [v1.5.6] centrala podaje stopnie
                                 (int)trybSerwaWartosc,
                                 stan.alarmOgrzewanie, stan.alarmPanel,
                                 stan.godzina, stan.minuta, stan.dzien, stan.miesiac, stan.rok,
                                 stan.rtcOk,
                                 stan.rssi,
                                 stan.internet,
                                 stan.deszczStan, stan.deszczMinuty,
                                 stan.pogodaKod, stan.pogodaKodOpad, stan.pogodaDzien, stan.wiatrPorywy,   // [v1.5.14]
                                 stan.pogodaTemp);   // [v1.5.17]
  // [v1.2.2 PAMIEC] Stan po starcie: WiFi dziala, pierwsze dane odebrane.
  // To jest liczba, ktora mowi, czy panel ma zapas (kilkadziesiat kB), czy
  // pracuje na styk (log z 2026-11-12: ok. 8 kB).
  logBezPamieci("tag=PAMIEC zdarzenie=po_starcie ram=%lu ram_blok=%lu ram_min=%lu\n",
                (unsigned long)ramWolny(), (unsigned long)ramBlok(), (unsigned long)ramMin());
  // [v1.4.7 ZASILANIE] Dopiero tutaj (ekran narysowany, WiFi polaczone,
  // pierwsze dane sa): zapisane taktowanie procesora. Do tego miejsca setup()
  // idzie zawsze na 240 MHz - kalibracja dotyku i pierwsze rysowanie nie
  // zwalniaja. Odczyt na ekran ZASILANIE od razu, nie dopiero za 2 s.
  zasilanieZastosujCpu("start");
  zasilaniePomiarNaEkran();
  // [v1.5.0 SEN] Licznik bezczynnosci liczy sie od TEJ chwili (panel gotowy,
  // ekran narysowany), nie od lv_init() kilka sekund wczesniej - inaczej
  // pierwsze przyciemnienie na baterii przyszloby o te kilka sekund za wczesnie.
  // Najpierw lvgl_port_tick(): zegar LVGL (lv_tick_inc w lvgl_port_tick()) stoi
  // przez caly setup() i nadrabia caly ten czas jednym skokiem przy pierwszym
  // wywolaniu - bez tego "dotyk" zglaszany tutaj bylby od razu kilka sekund stary.
  lvgl_port_tick();
  lvgl_dashboard_poc_zglos_aktywnosc();
  ostatniStatus = millis();
  // [OTA-ROLLBACK] Tutaj, nie wczesniej: ekran juz narysowany (obie galezie
  // SYMULACJA=0/1 zbiegaja sie w tym miejscu), a w galezi SYMULACJA=0
  // pierwsza probka pobierzStatus() juz sie wykonala (patrz wyzej).
  // [v1.4.0 OTA] Po zatwierdzeniu: czy ostatnie wgranie sie przyjelo, czy
  // bootloader wrocil do poprzedniego programu. Samej aktualizacji setup() juz
  // nie zaczyna (do v1.3.0: sprawdzIWykonajOtaPanelu() w tym miejscu, z pustym
  // ekranem do konca pobierania) - robi to otaKrok() w loop().
  otaPoZatwierdzeniu(potwierdzPartycjeJesliCzeka());
}

void loop() {
  // [FIX-WDT] Karmienie na poczatku KAZDEJ iteracji, przed jakimkolwiek
  // rozgalezieniem — loopTask jest teraz podpiety pod TWDT (patrz setup()),
  // wiec bez tego wywolania panel resetowalby sie co 15s w calkowicie
  // normalnej pracy, nie tylko przy realnym zawieszeniu.
  esp_task_wdt_reset();
  // [ROZSZERZENIE, ta sesja - BRZECZYK ALARMOWY] Musi isc co obrot petli
  // (nie tylko co STATUS_INTERWAL_MS jak reszta w tym bloku), zeby fazy
  // wzorca (odstepy rzedu setek ms) byly odmierzane dokladnie.
  obsluzBrzeczyk();
  // loop() liczy animacje LVGL.
  lvgl_port_tick();

  // [BATERIA-POMIAR] Pomiar ogniwa i wskaznik baterii - wspolne dla obu
  // podgalezi (SYMULACJA i realna), lokalne, niezalezne od centrali. Od
  // v1.5.0 w bateriaKrok() (pierwszy pomiar od razu, potem co 30 s).
  bateriaKrok();
  // [v1.5.0 SEN] Na baterii: przyciemnienie po 1 min, sen po 5 min bez dotyku,
  // sen od razu przy rozladowanym ogniwie. Na USB nic nie robi.
  senKrok();

  // Pilnuje WiFi + co STATUS_INTERWAL_MS odswieza dane z centrali.
  wifiReconnectNonBlocking();

  // [SIEC-NIEBLOKUJACA, ta sesja] Musza isc CO ITERACJE (nie tylko co
  // STATUS_INTERWAL_MS / rytm ustawien ponizej) - to one sprawdzaja,
  // czy zadanie wystartowane w ktorejs POPRZEDNIEJ iteracji juz sie
  // zakonczylo (statusFetch/ustawieniaFetch/komendaFetch.poll() sa tanie,
  // pojedynczy select() z zerowym czekaniem gdy bezczynne). Patrz komentarze
  // przy tych funkcjach.
  obslugaStatusFetch();
  obslugaUstawieniaFetch();
  obslugaKomendaFetch();
  poleceniaKrok();         // [v1.4.2 C4] polecenie czekajace na cisze po przytrzymanym przycisku
  obslugaWykresyFetch();   // [v1.3.0 WYKRESY] historia dla ekranu Wykresy - po kawalku na obrot, bez blokowania
  otaKrok();               // [v1.4.0 OTA] aktualizacja przez centrale: decyzja raz na sekunde, pobieranie po kawalku na obrot
  otaVscodeKrok();         // [v1.5.4 OTA-VSCODE] wgrywanie z PlatformIO przez WiFi (ArduinoOTA.handle())

  // [v1.2.0 DANE-NIEAKTUALNE] Brak poprawnie odczytanego /api/status przez
  // PROG_NIEAKTUALNYCH_DANYCH_MS -> kafelki na "--" (raz na kazdy zanik).
  if (straznikSwiezosci.wlasniePrzeterminowane(swiezoscStatusu.saNieaktualne(PROG_NIEAKTUALNYCH_DANYCH_MS))) {
    panelOznaczDaneJakoNieaktualne();
  }
  // [v1.2.1 RATUNEK] Dluzej bez danych: ponowne polaczenie z siecia centrali,
  // a gdy to nie pomaga - restart panelu. Patrz panelRatunekKrok().
  panelRatunekKrok();
  // [v1.2.0 DIAG] Jedna linia "tag=PANEL" co 30 s - patrz panelDiagOkresowo().
  panelDiagOkresowo();
  panelPamiecKrok();   // [v1.2.2 PAMIEC] ostrzezenie w logu, gdy wolnego RAM-u jest malo

  unsigned long terazLvgl = millis();
  if (terazLvgl - ostatniStatus >= STATUS_INTERWAL_MS) {
    ostatniStatus = terazLvgl;
    pobierzStatus(); // tylko startuje GET - parsowanie+odswiezenie LVGL teraz w obslugaStatusFetch() wyzej
    zasilaniePomiarNaEkran();   // [v1.4.7 ZASILANIE] zywy odczyt (RSSI panelu, MHz, dBm) - tani, gdy ekran zamkniety
  }
  // [v1.4.3 B4] Odczyt /api/ustawienia-get wtedy, gdy jest potrzebny: co 5 s
  // przy otwartym ekranie ustawien (i od razu przy jego otwarciu, gdyby ktos
  // rownolegle zmienial to samo z telefonu / strony WWW), w tle co 60 s.
  // Do v1.4.2: co 5 s zawsze. Patrz diag::RytmUstawien.
  if (rytmUstawien.czasNaGet((uint32_t)terazLvgl, lvgl_dashboard_poc_ekran_ustawien_otwarty())) {
    pobierzUstawienia(); // tylko startuje GET - patrz obslugaUstawieniaFetch() wyzej
  }
  delay(5); // LVGL nie potrzebuje petli szybszej niz ~200Hz, a to odciaza CPU/WDT
}
