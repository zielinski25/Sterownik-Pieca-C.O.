#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Brama układu ekranu (0.31.72) — sprawdzenia, które da się wykonać bez Gradle/JDK.

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
TEST = PROJEKT / "app/src/test/java/com/sterownikco/pro/core/ScreenLayoutTest.kt"

# Jedno miejsce do zmiany przy kolejnej paczce.
WERSJA = "0.31.72"
KOD_WERSJI = "31072"

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
        "8.dp + startInset" in chrome,
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
        "tresc omija pasek nawigacji (navigationBarsPadding)",
        "navigationBarsPadding" in kod and "navigationBarsPadding" in chrome,
        "przyciski nie moga byc pod paskiem systemowym",
    )
    sprawdz(
        "pas pod paskiem statusu malowany przez nas",
        "windowInsetsTopHeight(WindowInsets.statusBars)" in kod,
        "edge-to-edge zdejmuje kolor paska statusu z systemu",
    )
    sprawdz(
        "szyna: tlo startuje pod paskiem statusu",
        ".statusBarsPadding()\n            .background(Brush.linearGradient(listOf(Pal.Nav, Pal.Nav2)))" in chrome,
        "inaczej gorny pas szyny mialby inny kolor",
    )

    return raport()


def raport():
    print("=" * 78)
    print("BRAMA UKŁADU EKRANU — 0.31.72 (edge-to-edge: tresc pod paskami, szyna od krawedzi)")
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
