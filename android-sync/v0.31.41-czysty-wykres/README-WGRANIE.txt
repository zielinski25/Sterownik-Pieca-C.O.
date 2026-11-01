Sterownik CO — wersja 0.31.41 (Android)
======================================
Czysty wykres: zdjęte WSZYSTKO, co dodałem wokół luk w danych —
zarówno kreski na wykresie, jak i licznik luk w pasku statusu na górze.

CO ZDJAŁEM
----------
1) Wykres (ChartEngine.kt) — dwie pionowe przerywane kreski na granicach
   każdej przerwy w danych. Dodałem je w 0.31.34. Zdjęte.

2) Pasek statusu nad wykresem (AppModel.kt) — dopisek z liczbą luk:
       „· luk: 6 (najdłuższa 1 h 17 min)”
   Dodałem go w 0.31.34 razem z kreskami. Zdjęty — pasek kończy się teraz
   na tym, co było wcześniej:
       „Załadowano 1440 próbek · z RTDB · odcinki symulowane są kreskowane”

CZYLI: po 0.31.41 nie ma w aplikacji ANI JEDNEGO elementu, który ja
dołożyłem w sprawie luk. Zostało dokładnie to, co było przed 0.31.34.

JAK TERAZ WYGLĄDA PRZERWA W DANYCH
----------------------------------
Jak puste miejsce między próbkami — tak samo, jak w widoku HTML.
Krzywa się urywa, jest odstęp, krzywa wraca. Bez kresek, bez napisów,
bez liczb. Nic tego miejsca nie zasłania i nic nie trzeba czytać.

CO ZOSTAŁO (działa bez zmian)
-----------------------------
- Oś czasu: podziałki co równe godziny, więc przerwa jest widoczna jako
  odstęp, a nie jako „ściskanie” wykresu.
- Kursor po dotknięciu: jeśli trafisz na próbkę stojącą zaraz po przerwie,
  w dymku pojawi się „⏸ przerwa w danych: N min”. To tylko na żądanie,
  po dotknięciu — na wykresie nic nie wisi.
- Wszystkie serie, osie, siatka, seria pompy na dolnym pasku.

JAK ZBUDOWAĆ (PowerShell, w folderze projektu)
----------------------------------------------
  cd "$env:USERPROFILE\Downloads\SterownikCO-ANDROID\SterownikCO-Android-v0.31.41"
  Remove-Item Env:\JAVA_HOME -ErrorAction SilentlyContinue
  $env:JAVA_HOME = Split-Path (Split-Path (Get-Command java).Source -Parent) -Parent
  powershell -ExecutionPolicy Bypass -File .\zbuduj.ps1

Instalacja:
  & "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe" install -r "app\build\outputs\apk\debug\app-debug.apk"
  (albo plik APK na telefon przez Telegram/Dysk)

versionCode = 31041 (wyższy od 31040).
