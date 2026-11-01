SterownikCO — CAŁY PROJEKT ANDROID, wersja 0.31.55
====================================================

KOMPLETNY projekt, gotowy do zbudowania. Nie składasz nic z kawałków.
WERSJA: versionCode 31055, versionName "0.31.55"
W Ustawieniach po zbudowaniu musi być "v0.31.55".


CO JEST NOWEGO WOBEC 0.31.54
----------------------------
Naprawiony obcięty wiersz pod łukiem słońca na ekranie Pogoda.

BYŁO (wszystkie trzy liczby nieczytelne, ucinane wielokropkiem):
    Promieniowanie: 2...    Elewacja słońca: ok.47° (...    Długość dnia: ...

JEST (dwie wartości w wierszu, trzecia pod spodem — wszystkie w całości):
    Promieniowanie: 2 W/m² (est.)        Długość dnia: 10h 5m
    Elewacja słońca: ok.47° (szac.)

Przyczyna: trzy napisy stały w jednym wierszu z wagami 1 / 1,2 / 0,8.
Przy szerokości ~360 dp każdy dostawał 80–130 dp, a same etykiety są szersze.
Teraz najdłuższa etykieta dostaje całe pole (cała szerokość, 332 dp).


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
    SterownikCO-Android-v0.31.55-PELNY-PROJEKT.zip
Link do pobrania (z ID pliku) jest w wiadomości na czacie — tam zawsze jest
aktualny. Samo ID pliku nie zmienia się między pobraniami.


JAK ZBUDOWAĆ — JEDNO POLECENIE
------------------------------
Skoro czytasz ten plik, masz już ZIP. Rozpakuj go do
    C:\Users\<Ty>\Documents\SterownikCO-0.31.55
(a jeśli rozpakowałeś gdzie indziej — zmień $d na swój folder) i wklej CAŁĄ
tę linię do PowerShella. Znajduje SDK (także w starym projekcie), ustawia JDK
z Android Studio, buduje i wypisuje ścieżkę do APK:

$d="$env:USERPROFILE\Documents\SterownikCO-0.31.55"; $lp="$env:USERPROFILE\Documents\SterownikCO-0.31.49\android\local.properties"; $sdk=$null; if(Test-Path -LiteralPath $lp){ $m=Select-String -LiteralPath $lp -Pattern '^sdk\.dir=(.+)$' | Select-Object -First 1; if($m){ $sdk=$m.Matches[0].Groups[1].Value.Replace('\:',':').Replace('\\','\') } }; if(-not $sdk){ $sdk=@("$env:LOCALAPPDATA\Android\Sdk","$env:ANDROID_HOME","$env:ANDROID_SDK_ROOT","C:\Android\Sdk","D:\Android\Sdk") | Where-Object { $_ -and (Test-Path -LiteralPath $_) } | Select-Object -First 1 }; if($sdk){ $env:ANDROID_HOME=$sdk; $env:ANDROID_SDK_ROOT=$sdk; Write-Host "SDK: $sdk" } else { Write-Host "BRAK SDK — ustaw ANDROID_HOME recznie" }; $jbr="C:\Program Files\Android\Android Studio\jbr"; if((-not $env:JAVA_HOME) -and (Test-Path -LiteralPath $jbr)){ $env:JAVA_HOME=$jbr; Write-Host "JDK: $jbr" }; if(-not (Test-Path -LiteralPath "$d\gradlew.bat")){ Write-Host "BRAK PROJEKTU w $d — rozpakuj ZIP do tego folderu" } else { cd $d; .\gradlew.bat assembleDebug; $apk="$d\app\build\outputs\apk\debug\app-debug.apk"; if(Test-Path -LiteralPath $apk){ Write-Host ""; Write-Host "=== GOTOWE ==="; Get-Item -LiteralPath $apk | Format-List FullName,Length,LastWriteTime } else { Write-Host ""; Write-Host "=== NIE ZBUDOWALO SIE — patrz komunikat wyzej ===" } }


JEŚLI "SDK location not found"
------------------------------
To normalne przy świeżo rozpakowanym projekcie: plik local.properties nie jest
w paczce (bo zawiera ścieżki z cudzego komputera). Polecenie wyżej i tak
znajduje SDK i ustawia ANDROID_HOME + ANDROID_SDK_ROOT — Gradle czyta te
zmienne, więc błąd nie wystąpi. Samemu local.properties NIE wpisuj ręcznie
(trzeba by podwajać backslashe).


JAK SPRAWDZIĆ, ŻE MASZ DOBRĄ WERSJĘ
-----------------------------------
1. Ustawienia -> na dole "v0.31.55"
2. Pogoda -> nad łukiem słońca: Promieniowanie i Długość dnia w jednym
   wierszu, Elewacja słońca pod nimi — żadna liczba nie kończy się "..."
3. Kafelek: "Panel słon." — NIE "Zbiornik"
4. Wykresy: legenda "Panel", praca pompy jako kolumna na całą wysokość


CZEGO NIE RUSZA
---------------
Firmware centrali: bez zmian. 3.56.9 czeka osobno.
Stary folder SterownikCO-0.31.49 możesz zostawić — ten ZIP go nie dotyka.
