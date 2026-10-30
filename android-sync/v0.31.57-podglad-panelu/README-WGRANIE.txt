SterownikCO — CAŁY PROJEKT ANDROID, wersja 0.31.57
====================================================

KOMPLETNY projekt, gotowy do zbudowania. Nie składasz nic z kawałków.
WERSJA: versionCode 31057, versionName "0.31.57"
W Ustawieniach po zbudowaniu musi być "v0.31.57".


CO JEST NOWEGO WOBEC 0.31.56
----------------------------
Komendy z aplikacji docierają do centrali szybciej i potwierdzenia wracają
szybciej — ale TYLKO wtedy, gdy aplikacja jest otwarta.

BYŁO (najgorszy przypadek, aplikacja otwarta, cisza od godziny):
    klik -> centrala odczytuje komende do 7 s -> ACK zapisany -> aplikacja
    zauwaza go do 1 s  =  razem do ~8 s

JEST:
    klik -> centrala odczytuje komende do 2 s -> ACK zapisany -> aplikacja
    zauwaza go do 0,3 s  =  razem ~1,3-2,5 s

Jak: aplikacja co 50 s melduje centrali "podglad_wl" (heartbeat, nic nie
przelacza). Firmware 3.57.0 trzyma wtedy okno podgladu 90 s: czyta komendy
co 2 s i publikuje status co 5 s. Zamkniecie aplikacji = "podglad_wyl" =
natychmiastowy powrot do 15 s / 7 s. Zabita aplikacja = okno samo wygasa.
Dodatkowo sprawdzanie ACK w aplikacji: co 300 ms (bylo 1000 ms).

WYMAGA FIRMWARE 3.57.0, zeby dzialalo czysto. Ze starszym firmware (3.56.x)
aplikacja tez dziala i tez przyspiesza (heartbeat uruchamia tryb turbo
1 s/1 s), ale w logu centrali pojawia sie wpis "nieznana_komenda" co ~50 s.
Kolejnosc wgrywania nie ma znaczenia.


CO JEST JESZCZE (z 0.31.56 — szybki polling)
--------------------------------------------
Odpytywanie statusu przy otwartej aplikacji: 2 s (bylo 8 s). W tle zero
zapytan z UI — alarmy pilnuje osobna usluga jednym zapytaniem na 60 s.


CO JEST JESZCZE (z 0.31.55 — wiersz Pogody)
-------------------------------------------
Trzy wartości pod łukiem słońca były ucinane wielokropkiem:
    BYŁO:  Promieniowanie: 2...   Elewacja słońca: ok.47° (...   Długość dnia: ...
    JEST:  Promieniowanie: 2 W/m² (est.)        Długość dnia: 10h 5m
           Elewacja słońca: ok.47° (szac.)
Dwie wartości w wierszu (wagi 1,15 / 0,85), trzecia osobnym wierszem na całą
szerokość.


CO JEST W TEJ WERSJI (bez zmian wobec 0.31.54)
----------------------------------------------
- Stare nazwy: kafelek "Panel słon.", "Panel słoneczny (solary)",
  "Kolektor słoneczny próżniowy", "Panel teraz / powietrze",
  "Różnica panel - powietrze", "BILANS & ZYSK KOLEKTORA SŁONECZNEGO"
- Wykres: praca pompy na całą wysokość
- Model z niebem (0.31.52): skyRadWk 8,20, tankUa 1,4


POBIERANIE
----------
Ta paczka leży na Dysku Google jako:
    SterownikCO-Android-v0.31.57-PELNY-PROJEKT.zip
Link do pobrania (z ID pliku) jest w wiadomości na czacie — tam zawsze jest
aktualny. Samo ID pliku nie zmienia się między pobraniami.


JAK ZBUDOWAĆ — JEDNO POLECENIE
------------------------------
Skoro czytasz ten plik, masz już ZIP. Rozpakuj go do
    C:\Users\<Ty>\Documents\SterownikCO-0.31.57
(a jeśli rozpakowałeś gdzie indziej — zmień $d na swój folder) i wklej CAŁĄ
tę linię do PowerShella. Znajduje SDK (także w starym projekcie), ustawia JDK
z Android Studio, buduje i wypisuje ścieżkę do APK:

$d="$env:USERPROFILE\Documents\SterownikCO-0.31.57"; $lp="$env:USERPROFILE\Documents\SterownikCO-0.31.49\android\local.properties"; $sdk=$null; if(Test-Path -LiteralPath $lp){ $m=Select-String -LiteralPath $lp -Pattern '^sdk\.dir=(.+)$' | Select-Object -First 1; if($m){ $sdk=$m.Matches[0].Groups[1].Value.Replace('\:',':').Replace('\\','\') } }; if(-not $sdk){ $sdk=@("$env:LOCALAPPDATA\Android\Sdk","$env:ANDROID_HOME","$env:ANDROID_SDK_ROOT","C:\Android\Sdk","D:\Android\Sdk") | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1 }; if($sdk){ $env:ANDROID_HOME=$sdk; $env:ANDROID_SDK_ROOT=$sdk; Write-Host "SDK: $sdk" } else { Write-Host "BRAK SDK — ustaw ANDROID_HOME recznie" }; $jbr="C:\Program Files\Android\Android Studio\jbr"; if((-not $env:JAVA_HOME) -and (Test-Path -LiteralPath $jbr)){ $env:JAVA_HOME=$jbr; Write-Host "JDK: $jbr" }; if(-not (Test-Path -LiteralPath "$d\gradlew.bat")){ Write-Host "BRAK PROJEKTU w $d — rozpakuj ZIP do tego folderu" } else { cd $d; .\gradlew.bat assembleDebug; $apk="$d\app\build\outputs\apk\debug\app-debug.apk"; if(Test-Path -LiteralPath $apk){ Write-Host ""; Write-Host "=== GOTOWE ==="; Get-Item -LiteralPath $apk | Format-List FullName,Length,LastWriteTime } else { Write-Host ""; Write-Host "=== NIE ZBUDOWALO SIE — patrz komunikat wyzej ===" } }


JEŚLI "SDK location not found"
------------------------------
To normalne przy świeżo rozpakowanym projekcie: plik local.properties nie jest
w paczce (bo zawiera ścieżki z cudzego komputera). Polecenie wyżej i tak
znajduje SDK i ustawia ANDROID_HOME + ANDROID_SDK_ROOT — Gradle czyta te
zmienne, więc błąd nie wystąpi. Samemu local.properties NIE wpisuj ręcznie
(trzeba by podwajać backslashe).


JAK SPRAWDZIĆ, ŻE MASZ DOBRĄ WERSJĘ
-----------------------------------
1. Ustawienia -> na dole "v0.31.57"
2. Pogoda -> nad łukiem słońca: Promieniowanie i Długość dnia w jednym
   wierszu, Elewacja słońca pod nimi — żadna liczba nie kończy się "..."
3. Kafelek: "Panel słon." — NIE "Zbiornik"
4. Wykresy: legenda "Panel", praca pompy jako kolumna na całą wysokość
5. Szybkość: kliknij np. pompę — potwierdzenie i stan na kafelku w ~1–3 s
6. Firmware: Ustawienia -> wersja firmware. Pelny efekt = v3.57.0


CZEGO NIE RUSZA
---------------
Firmware centrali: bez zmian. 3.56.9 czeka osobno.
Stary folder SterownikCO-0.31.49 możesz zostawić — ten ZIP go nie dotyka.
