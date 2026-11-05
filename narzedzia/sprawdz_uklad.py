#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Brama układu ekranu (0.31.73) — sprawdzenia, które da się wykonać bez Gradle/JDK.

Pilnuje błędu z 0.31.68: po obrocie telefonu aplikacja nie rozciągała się na całe
okno, bo sztywna ramka 412 dp włączała się od samej szerokości (maxWidth > 560.dp).
W tle zostawało 45–56 % okna, a zakładki były ucięte.

Uruchomienie:  python3 narzedzia/sprawdz_uklad.py [katalog_projektu]
Kod wyjścia:   0 = wszystko OK, 1 = coś do poprawy.
"""

import re
import sys
from pathlib import Path

PROJEKT = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
APP = PROJEKT / "app/src/main/java/com/sterownikco/pro"
KROK = APP / "ui/SterownikApp.kt"
CHROME = APP / "ui/components/Chrome.kt"
CORE = APP / "core/ScreenLayout.kt"
TEMA = APP / "ui/theme/Theme.kt"
TEST = PROJEKT / "app/src/test/java/com/sterownikco/pro/core/ScreenLayoutTest.kt"

# Jedno miejsce do zmiany przy kolejnej paczce.
WERSJA = "0.31.81"
KOD_WERSJI = "31081"

wyniki = []


def sprawdz(nazwa, warunek, szczegol=""):
    wyniki.append((nazwa, bool(warunek), szczegol))
    return warunek


def czytaj(p):
    return p.read_text(encoding="utf-8") if p.exists() else ""


def bez_komentarzy(s):
    s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
    return re.sub(r"//[^\n]*", "", s)


def main():
    if not sprawdz("projekt ma ui/SterownikApp.kt", KROK.exists(), str(KROK)):
        return raport()

    krok = czytaj(KROK)
    kod = bez_komentarzy(krok)
    tema = czytaj(TEMA)

    # 1) ramka symulatora nie może wrócić
    sprawdz(
        "brak sztywnej ramki (Modifier.width(m.frameWidth.dp))",
        "frameWidth" not in kod,
        "to była przyczyna: ramka 412 dp od samej szerokości okna",
    )
    sprawdz(
        "brak fillMaxHeight(0.98f) — aplikacja zajmuje całe okno",
        "0.98f" not in kod,
    )
    sprawdz(
        "brak tła sceny Pal.Stage pod aplikacją",
        "Pal.Stage" not in kod,
        "Pal.Stage to kolor 'poza ramką telefonu'",
    )

    # 2) decyzja o układzie pochodzi z rozmiaru okna i jest w core (testowalna)
    sprawdz(
        "decyzja o nawigacji: ScreenLayout.useNavRail(...)",
        "ScreenLayout.useNavRail(" in kod,
    )
    sprawdz(
        "logika w core/ScreenLayout.kt istnieje",
        CORE.exists() and "object ScreenLayout" in czytaj(CORE),
    )
    sprawdz(
        "brak decyzji po orientacji urządzenia",
        not re.search(r"Configuration\.ORIENTATION_|\.orientation\b", kod),
        "orientacja zmienia się przy multi-window; liczy się rozmiar okna",
    )
    sprawdz(
        "brak sztywnego 'isTablet' / progu 560 dp",
        "560.dp" not in kod,
    )

    # 3) chrome: dolny pasek i szyna wzajemnie się wykluczają
    sprawdz(
        "NavBar tylko gdy !rail",
        re.search(r"if\s*\(\s*!\s*rail\s*\)\s*NavBar\(", kod) is not None,
    )
    sprawdz(
        "NavRail tylko gdy rail",
        re.search(r"if\s*\(\s*rail\s*\)\s*NavRail\(", kod) is not None,
    )
    chrome = bez_komentarzy(czytaj(CHROME))
    sprawdz("NavRail zdefiniowany w Chrome.kt", "fun NavRail(" in chrome)
    sprawdz(
        "pozycje szyny mają weight (nie wychodzą poza ekran)",
        re.search(r"fun NavRail\(.*?\n(?:.|\n)*?Modifier\.weight\(1f\)", chrome) is not None,
    )

    # 4) martwe stałe po ramce nie mogą zostać
    theme = czytaj(APP / "ui/theme/Theme.kt")
    model = czytaj(APP / "core/AppModel.kt")
    sprawdz("brak martwego AppModel.frameWidth", "frameWidth" not in model)
    sprawdz("brak martwego Dimens.frameW", "frameW" not in theme)

    # 5) test jednostkowy istnieje i pokrywa kluczowe okna
    tst = czytaj(TEST)
    sprawdz("test ScreenLayoutTest.kt istnieje", TEST.exists())
    for wymiar in ("331, windowHeightDp = 735", "735, windowHeightDp = 331", "915, windowHeightDp = 412"):
        sprawdz(f"test pokrywa okno {wymiar.split(',')[0]}×…", wymiar in tst)

    # 6) wersja podbita (każda paczka = nowy numer, zasada od 0.31.54)
    gradle = czytaj(PROJEKT / "app/build.gradle.kts")
    vn = re.search(r'versionName = "([^"]+)"', gradle)
    sprawdz(
        f"versionName == {WERSJA}",
        bool(vn) and vn.group(1) == WERSJA,
        f"jest: {vn.group(1) if vn else 'brak'}",
    )

    # 7) balans nawiasów w zmienionych plikach (brak kompilatora w sandboxie)
    for nazwa, p in (("SterownikApp.kt", KROK), ("Chrome.kt", CHROME),
                     ("ScreenLayout.kt", CORE), ("ScreenLayoutTest.kt", TEST)):
        s = bez_komentarzy(czytaj(p))
        s = s.replace('"', "")
        bal = {"(": 0, "{": 0, "[": 0}
        pary = {")": "(", "}": "{", "]": "["}
        ok = True
        for ch in s:
            if ch in bal:
                bal[ch] += 1
            elif ch in pary:
                bal[pary[ch]] -= 1
                if bal[pary[ch]] < 0:
                    ok = False
        sprawdz(f"{nazwa}: nawiasy zbalansowane", ok and all(v == 0 for v in bal.values()),
                str({k: v for k, v in bal.items() if v}))

    # ---- 0.31.70: pas przy wycieciu kamery nie moze byc czarny ----
    # Objaw ze zrzutu 1.11.2026: w poziomie po lewej stronie 37 dp (110 px)
    # CZYSTEJ czerni (#000000). To nie uklad aplikacji — to system zamalowuje
    # obszar wyciecia, gdy motyw nie dopuszcza rysowania pod nim.
    motyw27 = PROJEKT / "app/src/main/res/values-v27/themes.xml"
    tm27 = czytaj(motyw27)
    main_akt = czytaj(APP / "MainActivity.kt")

    sprawdz("jest values-v27/themes.xml (atrybut wyciecia od API 27)", motyw27.exists(), str(motyw27))
    sprawdz(
        'motyw zezwala na rysowanie pod wycieciem (shortEdges)',
        "windowLayoutInDisplayCutoutMode" in tm27 and "shortEdges" in tm27,
        "bez tego w poziomie system maluje pas na czarno",
    )
    sprawdz(
        "ten sam tryb ustawiany w kodzie (niektore urzadzenia ignoruja motyw)",
        "LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES" in main_akt,
        "MainActivity powinno ustawic window.attributes.layoutInDisplayCutoutMode",
    )
    sprawdz(
        "ustawienie w kodzie chronione wersja API (stala od API 28)",
        "VERSION_CODES.P" in main_akt or "SDK_INT >= 28" in main_akt,
        "na API 26-27 nie ma tej stalej",
    )
    sprawdz(
        "interfejs odsunięty od dziurki kamery (displayCutoutPadding)",
        "displayCutoutPadding" in kod,
        "inaczej przyciski wchodza pod obiektyw",
    )
    sprawdz(
        "tlo pod wycieciem to Pal.Bg, nie czern",
        "background(Pal.Bg)" in kod,
        "pas ma wygladac jak tlo aplikacji",
    )
    sprawdz("brak martwej sceny (Pal.Stage) w korzeniu", "Pal.Stage" not in kod, "")

    gradle = czytaj(PROJEKT / "app/build.gradle.kts")
    wersja = re.search(r'versionName\s*=\s*"([^"]+)"', gradle)
    sprawdz(
        f"wersja podbita na {WERSJA}",
        bool(wersja) and wersja.group(1) == WERSJA,
        wersja.group(1) if wersja else "brak versionName",
    )
    vcode = re.search(r"versionCode\s*=\s*(\d+)", gradle)
    sprawdz(
        f"versionCode {KOD_WERSJI}",
        bool(vcode) and vcode.group(1) == KOD_WERSJI,
        vcode.group(1) if vcode else "brak versionCode",
    )

    # ---- 0.31.71: wciecie pochlania szyna, bez szwu przy krawedzi ----
    chrome = czytaj(CHROME)
    sprawdz(
        "NavRail przyjmuje startInset (wciecie wyciecia po lewej)",
        "fun NavRail(page: Int, startInset: Dp = 0.dp" in chrome,
        "szyna musi sama pochlanac wciecie",
    )
    sprawdz(
        "NavRail powieksza szerokosc o wciecie (tlo do krawedzi)",
        "Dimens.railW + startInset" in chrome,
        "inaczej przy krawedzi zostaje pas tla",
    )
    sprawdz(
        "NavRail odsuwa zawartosc o wciecie (ikony poza obiektywem)",
        "6.dp + startInset" in chrome,
        "ikony nie moga wejsc pod kamere",
    )
    sprawdz(
        "SterownikApp liczy wciecie z WindowInsets.displayCutout",
        "WindowInsets.displayCutout" in kod and "calculateStartPadding" in kod,
        "wciecie musi byc realne, nie stala",
    )
    sprawdz(
        "w trybie szyny tresc nie dostaje drugiego paddingu",
        "if (rail) Modifier else Modifier.displayCutoutPadding()" in kod,
        "podwojne odsuniecie = szew albo zwezenie tresci",
    )

    # ---- 0.31.72: wlasciwa przyczyna pasa = wciecie SYSTEMOWE, nie nasze ----
    # 0.31.71 nie mogla pomoc: system ustawial poczatek widoku tresci 37 dp od
    # krawedzi (decorFitsSystemWindows = true), wiec szyna zawsze zaczynala sie
    # od x=59 px — potwierdzone na zrzucie 12:30 (ramka szyny przy x=59 i brak
    # ramki przy x=0). Ratunkiem jest tryb edge-to-edge.
    sprawdz(
        "MainActivity wlacza edge-to-edge (setDecorFitsSystemWindows false)",
        "setDecorFitsSystemWindows(window, false)" in main_akt,
        "bez tego system dalej odsuwa cala tresc od wyciecia",
    )
    sprawdz(
        "TopBar: wciecie statusu DODANE do wysokosci paska, nie w srodku",
        ".statusBarsPadding()\n            .height(Dimens.topBarH)" in chrome,
        "inaczej pasek 62 dp traci 37 dp na wciecie",
    )
    sprawdz(
        "tresc omija pasek nawigacji (koniec + dol)",
        ".padding(end = navEnd, bottom = navBottom)" in kod,
        "przyciski nie moga byc pod paskiem systemowym",
    )
    # [0.31.74] `navigationBarsPadding()` w korzeniu dokladalo 39 dp z LEWEJ
    # (system raportuje pas nawigacji razem z wycieciem kamery), przez co szyna
    # 119 dp miala obok siebie drugie 39 dp pustki. W korzeniu ma go nie byc.
    sprawdz(
        "korzen NIE uzywa navigationBarsPadding (dublowal wciecie wyciecia)",
        "navigationBarsPadding" not in kod,
        "wtedy tresc startuje 39 dp za daleko od szyny",
    )
    sprawdz(
        "szyna nie szersza niz 70 dp (mniej zabiera pulpitowi)",
        "val railW = 68.dp" in tema,
        "80 dp bylo szersze niz najdluzsza etykieta",
    )

    # ---- 0.31.73: kolejnosc modyfikatorow (blad 0.31.72) ----
    # W Compose .statusBarsPadding().background() maluje tlo DOPIERO pod paskiem
    # statusu. Tlo musi byc wczesniej w lancuchu, inaczej gorny pas ma kolor tla
    # okna i gradient zaczyna sie od nowa (widoczna linia pod paskiem).
    chrome_kod = bez_komentarzy(chrome)
    i_top = chrome_kod.find("fun TopBar(")
    i_rail = chrome_kod.find("fun NavRail(")
    frag_top = chrome_kod[i_top:i_top + 1200] if i_top >= 0 else ""
    frag_rail = chrome_kod[i_rail:i_rail + 1200] if i_rail >= 0 else ""
    sprawdz(
        "TopBar: tlo PRZED wcieciem paska statusu",
        frag_top.find(".background(") != -1 and
        frag_top.find(".background(") < frag_top.find(".statusBarsPadding()"),
        "odwrotna kolejnosc = tlo nie pokrywa paska statusu",
    )
    sprawdz(
        "NavRail: tlo PRZED wcieciem paska statusu",
        frag_rail.find(".background(") != -1 and
        frag_rail.find(".background(") < frag_rail.find(".statusBarsPadding()"),
        "inaczej gorny rog ma kolor paska gornego",
    )
    sprawdz(
        "brak zbednego pasa pod paskiem statusu w korzeniu",
        "windowInsetsTopHeight" not in kod,
        "0.31.73: pasy maluja same TopBar i NavRail",
    )
    sprawdz(
        "brak nieuzywanego importu Brush w SterownikApp",
        not ("import androidx.compose.ui.graphics.Brush" in kod and "Brush." not in kod),
        "import bez uzycia",
    )

    # ---- 0.31.74: szyna wezsza + obramowanie przed wcieciem paska statusu ----
    # Obramowanie postawione PO .statusBarsPadding() rysowalo pozioma linie pod
    # paskiem statusu (widoczny "szew" na gorze szyny i paska gornego).
    i_bord_top = frag_top.find(".border(")
    i_bord_rail = frag_rail.find(".border(")
    sprawdz(
        "TopBar: obramowanie przed wcieciem paska statusu",
        i_bord_top != -1 and i_bord_top < frag_top.find(".statusBarsPadding()"),
        "inaczej linia tla pod paskiem statusu",
    )
    sprawdz(
        "NavRail: obramowanie przed wcieciem paska statusu",
        i_bord_rail != -1 and i_bord_rail < frag_rail.find(".statusBarsPadding()"),
        "inaczej pozioma linia na gorze szyny",
    )
    sprawdz(
        "NavRail: wewnatrz zostaje >= 50 dp na etykiety",
        ("val railW = 68.dp" in tema) and ("6.dp + startInset" in chrome) and ("end = 6.dp" in chrome),
        "najdluzsza etykieta (Ustawienia) ma ~44 dp",
    )

    # ---- 0.31.75: import funkcji, ktora NIE jest rozszerzeniem ----
    # `calculateBottomPadding()` (oraz top/left/right) to METODY interfejsu
    # PaddingValues, a nie funkcje-rozszerzenia w foundation.layout. Import
    # `androidx.compose.foundation.layout.calculateBottomPadding` nie istnieje
    # i wywala kompilacje ("Unresolved reference"), choc samo wywolanie jest OK.
    # Rozszerzeniami sa tylko calculateStartPadding i calculateEndPadding.
    importy_calc = re.findall(r"import androidx\.compose\.foundation\.layout\.(calculate\w+)", krok)
    zle_importy = [n for n in importy_calc if n not in ("calculateStartPadding", "calculateEndPadding")]
    sprawdz(
        "brak importu metody PaddingValues jako rozszerzenia",
        not zle_importy,
        ("usun import: " + ", ".join(zle_importy)) if zle_importy else "ok",
    )
    sprawdz(
        "wciecie nawigacji: koniec z asPaddingValues + metoda interfejsu",
        "asPaddingValues()" in kod and "calculateBottomPadding()" in kod,
        "calculateBottomPadding() wolamy bez importu",
    )

    # ---- 0.31.77: wykrywanie pobrań wody USUNIĘTE ----
    weather = bez_komentarzy(czytaj(APP / "ui/screens/Weather.kt"))
    sensors = bez_komentarzy(czytaj(APP / "ui/screens/sheets/Sensors.kt"))
    solar = bez_komentarzy(czytaj(APP / "core/SolarAnalytics.kt"))
    # Powód (nie kaprys): nie ma sygnału zaworu, więc "pobranie" było zgadywane
    # ze spadku temperatury. 04.11.2026: 5 pobrań i -23,3 °C przy całkowitym
    # spadku doby -15,8 °C — więcej ciepła, niż zbiornik stracił.
    sprawdz(
        "kafelek POBRANIA DZIŚ usunięty z karty BILANS",
        "POBRANIA DZIŚ" not in weather and "todayDrawCount" not in weather,
        "Weather.kt: 3 karty zamiast 4",
    )
    sprawdz(
        "wiersz Pobrania ciepłej wody usunięty z arkusza Czujniki",
        "Pobrania ciepłej wody" not in sensors and "todayDrawCount" not in sensors,
        "Sensors.kt: brak wiersza",
    )
    sprawdz(
        "nota w Czujnikach nie obiecuje wykrywania pobrań",
        "Pobranie liczy się jako" not in sensors and "nie ma sygnału zaworu" in sensors,
        "inaczej UI obiecuje funkcję, której nie ma",
    )
    sprawdz(
        "heurystyka pobrań usunięta z modelu solarnego",
        "recomputeDraws" not in solar and "rebuildDrawEvents" not in solar
        and "DRAW_STEP_C" not in solar and "waterDrawEvents" not in solar,
        "SolarAnalytics.kt: ~70 linii mniej",
    )
    sprawdz(
        "liczniki pobrań usunięte z modelu solarnego",
        "todayDrawCount" not in solar and "todayDrawDrop" not in solar
        and "accumulatedDrawDrop" not in solar and "drawCount" not in solar,
        "nie ma czego pokazać, więc nie ma czego liczyć",
    )
    sprawdz(
        "CSV zachowuje kolumny pobor_wody i spadek_poboru_C (zapisują 0)",
        "pobor_wody" in solar and "spadek_poboru_C" in solar,
        "stare pliki i czytniki nie mogą się rozjechać",
    )
    sprawdz(
        "zmierzony zysk dnia NIE został ruszony",
        "todayGain = if (todayHasData) lastT - firstT else 0.0" in solar,
        "SPADEK/ZYSK DZIŚ liczy się z temperatury, nie z pobrań",
    )

    # ---- 0.31.78: tytuły kart nie mogą być ucinane ----
    # Użytkownik widział „BILANS & ZYSK KOLEKTORA SŁON" bez końcówki — bo tytuł
    # miał maxLines = 1, softWrap = false i overflow = Clip (obcina w połowie słowa).
    sprawdz(
        "tytuł karty zawija się do 2 linii zamiast być obcinany",
        "maxLines = 2, softWrap = true, overflow = TextOverflow.Ellipsis" in weather,
        "WCard: wcześniej Clip ucinał w połowie słowa, bez wielokropka",
    )
    sprawdz(
        "brak obcinania tytułu bez ostrzeżenia (softWrap = false + Clip)",
        "softWrap = false, overflow = TextOverflow.Clip" not in weather,
        "taki duet wygląda jak ucięty ekran, a nie skrócony tekst",
    )
    # [0.31.79] Sprawdzamy FAKT, nie własne nowe sformułowanie: nagłówek musi
    # zawierać liczbę dni, a podpis musi mówić, że to nie pomiar. Oba warunki
    # przechodzą niezależnie od tego, jak tekst zostanie przeredagowany.
    sprawdz(
        "nagłówek prognozy 7-dniowej zawiera liczbę dni",
        bool(re.search(r"PROGNOZA POTENCJAŁU SOLARNEGO[^\"]*7 DNI", weather)),
        "bez „7 DNI” nagłówek nie mówi, na ile dni jest prognoza",
    )
    sprawdz(
        "podpis prognozy solarnej mówi wprost „nie pomiar”",
        bool(re.search(r"szacunek[^\"]{0,40}nie pomiar", weather)),
        "szacunek bez tego zastrzeżenia czyta się jak pomiar",
    )
    sprawdz(
        "nota w Czujnikach definiuje zysk jako różnicę temperatur",
        "Zysk = różnica temperatur wody" in sensors,
        "wcześniej „o ile °C ogrzała się woda” myliło różnicę z wartością",
    )

    # ---- 0.31.79: „DO KOŃCA DNIA" ma JEDNO źródło prawdy ----
    # Do 0.31.78 kafelek pokazywał +0,1 °C z podpisem „woda stygnie do północy",
    # a arkusz Czujniki nie pokazywał tam liczby wcale. Wzór na resztę doby był
    # przepisany w dwóch funkcjach (Weather.kt i dailyForecast7), więc mógł się
    # rozjechać bez śladu.
    sprawdz(
        "jedna funkcja liczy zmianę do końca doby (z warunkiem słońca)",
        "fun restOfDayDeltaC(sunLeft: Boolean): Double" in solar,
        "SolarAnalytics.restOfDayDeltaC(sunLeft)",
    )
    # [0.31.80] Każde wołanie MUSI podać sunLeft. Wołanie bez argumentu znaczy,
    # że ktoś znowu bierze zysk szczytu bez patrzenia, czy słońce jeszcze jest.
    sprawdz(
        "brak wołań restOfDayDeltaC() bez argumentu",
        not re.search(r"restOfDayDeltaC\(\s*\)", solar + weather + sensors),
        "wołanie bez sunLeft = zysk szczytu także po zachodzie",
    )
    sprawdz(
        "prognoza 7-dniowa bierze resztę doby z tej funkcji",
        bool(re.search(r"val rest = restOfDayDeltaC\(\s*Weather\.sunLeftToday\(", solar)),
        "inaczej karta DZIŚ liczy się drugim wzorem",
    )
    sprawdz(
        "kafelek DO KOŃCA DNIA bierze wartość z tej funkcji",
        bool(re.search(r"val d = sol\.restOfDayDeltaC\(\s*sunLeft\s*\)", weather)),
        "Weather.endOfDayGain",
    )
    sprawdz(
        "arkusz Czujniki używa tych samych funkcji co kafelek",
        "endOfDayGain(solar" in sensors and "endOfDaySub(solar" in sensors,
        "do 0.31.78 arkusz miał własny when-branch i gubił liczbę",
    )
    sprawdz(
        "arkusz nie liczy „do końca dnia” po swojemu",
        "solar.forecastGain" not in sensors,
        "żadnego drugiego wzoru w UI arkusza",
    )
    sprawdz(
        "opis „stygnie / ociepla” zależy od znaku, nie od pory dnia",
        bool(re.search(r"d < -ZERO_EPS_C -> \"bez słońca", weather))
        and bool(re.search(r"d > ZERO_EPS_C -> \"bez słońca", weather)),
        "godzina 22:00 nie znaczy „na pewno stygnie”",
    )
    # [0.31.80] Opis musi się zmieścić w kafelku (85,3 dp przy 360 dp, 7,5 sp)
    # i w wierszu arkusza. Zmierzone: 26 znaków ≈ 100 dp, więc limit 26.
    opisy = re.findall(r'-> "(bez słońca[^"]*)"', weather)
    sprawdz(
        "opisy „do końca dnia” są krótkie (≤ 26 znaków)",
        bool(opisy) and max(len(x.replace("\\u00A0", " ")) for x in opisy) <= 26,
        "długi opis nie mieści się w kafelku: " + ", ".join(sorted(opisy, key=len)[-1:]),
    )
    # [0.31.80] Wiersz arkusza: liczba osobno, opis w drugiej linii.
    sprawdz(
        "arkusz nie dokleja opisu do liczby („$v · $sub”)",
        "$v · $sub" not in sensors,
        "doklejony opis nie mieścił się w jednej linii (459 dp przy 308 dp)",
    )
    sprawdz(
        "SolarMetricRow ma drugą linię na opis",
        bool(re.search(r"fun SolarMetricRow\(title: String, value: String, sub: String = \"\"\)", sensors))
        and "if (sub.isNotEmpty())" in sensors,
        "bez tego opisu nie ma gdzie pokazać",
    )
    # [0.31.80] Jedna implementacja reguły „jest jeszcze słońce”. Dwie kopie to
    # dokładnie ten błąd, który dał zysk szczytu po zachodzie w prognozie 7-dniowej.
    reguly = []
    for plik in APP.rglob("*.kt"):
        t = bez_komentarzy(czytaj(plik))
        reguly += [plik.name] * len(re.findall(r"shortwaveRadiation \?: 0\.0\) > 20\.0", t))
    sprawdz(
        "reguła „jest jeszcze słońce” ma jedną implementację (core)",
        reguly == ["Weather.kt"],
        "kopie: " + (", ".join(reguly) if reguly else "brak"),
    )
    sprawdz(
        "ekran nie ma własnej kopii reguły (brak solarSunLeft w UI)",
        "fun solarSunLeft" not in weather,
        "reguła ma być w core/Weather.sunLeftToday",
    )
    sprawdz(
        "karta DZIŚ = zmierzone + to samo źródło",
        "out.add(todayGain + restOfDay)" in solar,
        "spójność kafelka i karty jest widoczna w kodzie",
    )

    # ---- 0.31.81: kafelki w jednym rzędzie mają równą wysokość ----
    # Kafel „DO KOŃCA DNIA" ma podpis w dwóch liniach, więc był wyższy od
    # sąsiadów i dolna krawędź nie stała w jednej linii.
    sprawdz(
        "rząd kafelków liczy wysokość z najwyższego (IntrinsicSize.Min)",
        bool(re.search(r"Row\(\s*Modifier\.fillMaxWidth\(\)\.height\(IntrinsicSize\.Min\)", weather)),
        "bez tego każdy kafelek ma własną wysokość",
    )
    sprawdz(
        "kafelek rozciąga się do wysokości rzędu (fillMaxHeight)",
        bool(re.search(r"weight\(1f\)\.fillMaxHeight\(\)", weather)),
        "sama wysokość rzędu nie wyrówna teł kafelków",
    )
    # ---- 0.31.81: CI uruchamia testy jednostkowe ----
    wf = czytaj(PROJEKT.parent / ".github/workflows/apk-android.yml")
    if not wf:
        wf = czytaj(Path("/home/user/Sterownik-Pieca-C.O./.github/workflows/apk-android.yml"))
    i_asm = wf.find("assembleDebug")
    i_test = wf.find("testDebugUnitTest")
    i_up = wf.find("upload-artifact")
    sprawdz(
        "CI uruchamia testDebugUnitTest po assembleDebug, przed uploadem",
        i_asm >= 0 and i_test > i_asm and i_up > i_test,
        "dziś CI dowodzi tylko kompilacji źródeł głównych",
    )

    return raport()


def raport():
    print("=" * 78)
    print("BRAMA UKŁADU EKRANU — 0.31.81 (0.31.75 + bez pobrań + jedno źródło z warunkiem słońca + równe kafelki)")
    print("=" * 78)
    ok = 0
    for nazwa, wynik, szczegol in wyniki:
        print(f"  {'OK  ' if wynik else 'BŁĄD'} {nazwa}" + (f"   [{szczegol}]" if not wynik and szczegol else ""))
        ok += wynik
    print("-" * 78)
    print(f"RAZEM: {ok} OK / {len(wyniki) - ok} BŁĄD")
    return 0 if ok == len(wyniki) else 1


if __name__ == "__main__":
    sys.exit(main())
