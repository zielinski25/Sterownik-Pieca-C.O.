# Ender-3 V2 — materiały robocze

**Nie instalować ani nie drukować przy użyciu v33.** Oprogramowanie nie zostało w pełni skompilowane dla ARM ani sprawdzone na drukarce. Wersja v32 także nie ma potwierdzonej pełnej kompilacji ani testów sprzętowych. W kodzie źródłowym v32 start z karty może uruchamiać pomiar przed wykonaniem poleceń nagrzewania z pliku wydruku; przed jego użyciem sprawdź działanie na urządzeniu. Sama paczka v32 zawiera źródła, a nie potwierdzony nowy wsad.

- `v33-robocza-zmiana-od-v32.patch` zawiera zmiany sześciu plików źródeł względem roboczej paczki v32. To nie jest samodzielne oprogramowanie ani wsad do drukarki.
- Pełne archiwa źródeł są na Dysku: [v32](https://drive.google.com/file/d/1alhG7F8Fx1qSVMVd3M1UOQcRSALoNyWV/view) i [robocza v33](https://drive.google.com/file/d/1t-247fhkQm1lySGAtEDCd2SgoLk3aApD/view). Dużych archiwów nie dodano do repozytorium.
- `narzedzia/test_start_v33.py` jest częściowym testem na komputerze z atrapami, wymaga lokalnego archiwum v33 pod nazwą określoną w skrypcie oraz `g++`. Nie zastępuje kompilacji firmware ani prób na sprzęcie.
- `Ender-3 V2 - Uniwersalny zestaw PL.orca_bundle` zawiera profile do Orca; import i druk nie zostały sprawdzone. Sam profil nie naprawia wczesnego pomiaru wykonywanego przez v32.
- Pliki `Test pasowania - wersja oszczedna` są projektem modelu kalibracyjnego, nie firmware. Licencję, źródło oryginału i instrukcję podano w katalogu.
