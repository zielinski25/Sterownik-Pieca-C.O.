Sterownik CO — wersja 0.31.38 (Android)
======================================
Jedna poprawka: karta „DO KOŃCA DNIA” pokazywała „brak prognozy”,
choć karty 7-dniowe obok miały normalne liczby.

OBJAW (zrzuty 2026-10-29, 00:37)
--------------------------------
Pulpit:       DO KOŃCA DNIA — „brak prognozy”
Arkusz:       Do końca dnia · szacunek — „brak prognozy”
Obok:         DZIŚ -4,5°C (41 W/m²), JUTRO +4,0°C, Sb +3,5°C, ... — dane są.

PRZYCZYNA — kolejność startu, nie dane
---------------------------------------
Model „od teraz do północy” potrzebuje DWÓCH rzeczy naraz:
  1. aktualnej temperatury wody (próbka w archiwum),
  2. godzin pogody z Open-Meteo.

Po starcie aplikacji pogoda przychodzi pierwsza (jedno zapytanie sieciowe),
a historia z chmury (Firebase) dopiero po niej. Model wystartował więc bez
próbki wody, zwrócił „brak danych”, a obliczenie było powtarzane dopiero przy
następnym odświeżeniu pogody — czyli po 15 minutach. Karty 7-dniowe liczą się
z samej prognozy, bez próbki, dlatego działały.

CO JEST W 0.31.38
-----------------
1. Po scaleniu historii z chmury (a więc i po starcie, i po powrocie do
   aplikacji, i po otwarciu arkusza) prognoza jest liczona PONOWNIE, jeśli
   wcześniej się nie udała. Nie trzeba już czekać 15 minut.
2. Komunikat rozdziela dwie różne przyczyny:
   - „brak próbki wody z czujnika” — archiwum puste, nie ma od czego liczyć,
   - „brak prognozy” — brak godzin pogody z Open-Meteo.

CZEGO SIĘ SPODZIEWAĆ
--------------------
29.10 to dzień bardzo pochmurny: prognoza szczytu 41 W/m² przy stracie
5,57 °C/dobę. Model policzy więc samo wychładzanie i karta pokaże
„słońce nie przebije strat ciepła” zamiast mylącego „brak prognozy”.
Wieczorem, po zachodzie słońca, wróci „bez słońca do końca dnia”.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.38"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31038 (wyższy od 31037).
