Sterownik CO — wersja 0.31.44 (Android)
======================================
PRZYWRÓCONE TWOJE USTAWIENIA. To nie była wina komputera — to była wina REPOZYTORIUM.

CO SIĘ STAŁO (mówię wprost)
---------------------------
W repo na GitHubie drzewo aplikacji Android zostało cofnięte do wersji
**0.31.11**. Wszystko, co zrobiłeś między 0.31.12 a 0.31.28, istniało tylko
w paczkach ZIP, które dostałeś na komputer — nigdy nie trafiło do repo.

Dlatego paczki 0.31.33–0.31.43, które budowałem z repo, były poprawne, ale
składały się na starszym fundamencie. Stąd brakujące rzeczy, które zauważyłeś.

Komputer, na którym kompilujesz, NIE ma z tym nic wspólnego. Ten sam kod
z każdego komputera daje ten sam efekt. Różnica jest w źródle, z którego
kompilujesz — a źródłem było okrojone repo.

CO WRÓCIŁO W 0.31.44
--------------------
1) USTAWIENIA → „Bufor offline" (z 0.31.22)
   - kafelek w liście Ustawień,
   - arkusz z przełącznikiem (spoolrt_wl / spoolrt_wyl),
   - liczniki od startu centrali: strony zapisane na flash, próbki zapisane,
     próbki odtworzone w Firebase,
   - stan czytany wyłącznie ze statusu centrali (pole spool_rt), więc
     przełącznik nie kłamie,
   - na starszym firmware (bez spool_rt) jest nieaktywny z wyjaśnieniem.
   Wymaga firmware v3.54.0 albo nowszego.

2) KAFELEK „SERWO" NA PULPICIE (z 0.31.23–0.31.25)
   - wartość w procentach: „klapa 42%" oraz w podpisie „syberek 17%",
   - pigułka trybu: AUTO / RĘCZNY,
   - po wysłaniu komendy: „CEL 65%" + „klapa 117° · czekam", dopóki
     centrala nie potwierdzi nowej pozycji,
   - tryb BEZPIECZNY ma własny opis.

3) POZOSTAŁE RZECZY Z TYCH WERSJI, których brakowało:
   - dźwięki alarmów wybierane z aplikacji (0.31.23) + synchronizacja
     z centralą,
   - terminal DLOG: automatyczne zatrzymanie po 5 min bez dotykania ekranu
     (0.31.21) — koniec z 720 zapytaniami na godzinę,
   - dzienny cache historii + komunikaty „brak N dni" (0.31.20),
   - naprawa czasu próbek po restarcie centrali (TelemetryTimeRepair),
   - 6 brakujących plików źródłowych, bez których tamte funkcje nie działały.

CZEGO NIE RUSZAŁEM
------------------
Wszystko z 0.31.33–0.31.43 zostaje:
   - wykres w czasie (oś czasu, przerwy jako odstęp),
   - brak napisów i kresek luk (0.31.40/0.31.41),
   - brak paska statusu i kafelków nad wykresem (0.31.42),
   - pamięć historii w telefonie i dociąganie tylko brakujących przedziałów
     (0.31.43),
   - solar: zbiornik na dachu, tPanel, prognoza po starcie.

JAK SPRAWDZISZ, ŻE TYMASZ WŁAŚCIWĄ WERSJĘ
-----------------------------------------
Ustawienia → na dole lista kafelków. Musi być „Bufor offline" (między
„Czujnik dymu" a „Data i czas").
Pulpit → kafelek SERWO pokazuje procenty: „klapa 42%".

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.44"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31044 (wyższy od 31043).

UWAGA O BŁĘDZIE KOMPILACJI
--------------------------
Ta wersja składa dwa strumienie pracy, których nigdy wcześniej nie było razem.
Nie mam w tym środowisku JDK ani Android SDK, więc NIE mogłem jej skompilować —
sprawdziłem statycznie (nawiasy, brakujące symbole, duplikaty, spójność API).
Jeśli Gradle zgłosi „Unresolved reference", prześlij mi dokładnie tę linię
z komunikatu — poprawka jest wtedy kwestią minut, nie godzin.
