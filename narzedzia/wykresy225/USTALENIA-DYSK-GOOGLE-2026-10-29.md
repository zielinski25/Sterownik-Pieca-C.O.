# Ustalenia z przeszukania Dysku Google — 29.10.2026

Szukanie zlecone po zgłoszeniu: „nie mam w ustawieniach tej zakładki od przełączania
bufora i na dashboardzie ta ikonka serwo inaczej opisy miała".

## Wniosek w jednym zdaniu

**Repo GitHub nigdy nie miało drzewa Androida nowszego niż v0.31.11.**
Wszystko z zakresu v0.31.12–v0.31.32 istniało wyłącznie w paczkach ZIP
na Dysku Google i na komputerze użytkownika. Komputer kompilujący nie ma
z tym nic wspólnego.

## Dowód — cudzy dokument, nie moje domysły

`STATUS-AKTUALNE.txt` (Dysk, folder „Aktualne", plik `1u1QhwaOdrCtPkMegBhY4nHthlVO2ZNAR`,
data 18.10.2026, 1250 B). Cytat dosłowny:

> Android na urządzeniu: v0.31.19-debug (potwierdzenie: zrzut ekranu użytkownika).
> Ta wersja nie jest osobnym plikiem APK/ZIP znalezionym na Drive/GitHub; według
> instrukcji przekazanej przez użytkownika powstaje przez zbudowanie aplikacji:
> 1. Bazowy pełny projekt Androida z wrapperem Gradle:
>    `SterownikCO-Android-v0.31.18-build-20261016-094945\android`.
> 2. Nadpisanie jego plików zawartością katalogu `android` z paczki
>    `v3.50.0-alarm-sound-sync`.
> 3. Uruchomienie `.\gradlew.bat :app:assembleDebug`.
>
> GitHub: sprawdzone snapshoty main oraz gałęzi arena/01a8dd84 i arena/539ab1b6.
> Źródło Androida ma w nich versionName v0.31.11 (debug z sufiksem -debug),
> nie v0.31.19. (…) Repozytorium GitHub nie zostało zmienione.

To jest niezależne potwierdzenie (sporządzone 18.10, na długo przed tą sesją)
dokładnie tego, co wyszło z porównania drzew 29.10.

## Co jest na Dysku (stan 29.10.2026)

### Pełne projekty Androida (z wrapperem Gradle) — wszystkie STARSZE od 0.31.22

| wersja | plik | rozmiar | data | ID na Dysku |
|---|---|---|---|---|
| 0.31.18 | SterownikCO-Android-v0.31.18-Serwo-Live-Refresh-READY.zip | 1 114 091 B | 16.10 | `10U_spkzjjZn0byau_OzB2csfwktpv7xN` |
| 0.31.17 | SterownikCO-Android-v0.31.17-Serwo-Procenty-READY.zip | 1 108 483 B | 16.10 | `1Gls6Wapnv6Q1nnv4793B7KQq0Mfcvnlv` |
| 0.31.16 | SterownikCO-Android-v0.31.16-ServoPreview-READY.zip | 1 108 066 B | 15.10 | `1oQd07XipwBeMEB4v0tA0lrhqpPR4aFNx` |
| 0.31.14 | SterownikCO-Android-v0.31.14-ServoPreview-READY.zip | 918 353 B | 15.10 | `1T76G7vcpl-61qfLHyzRyy1BloiZNhPOA` |
| 0.31.13 | SterownikCO-Android-v0.31.13-ServoPreview-Windows.zip | 918 398 B | 15.10 | `1BFDARuTzS53Bo6dzy_h6WhEdbcamsgQr` |
| 0.31.12 | Android-Chart-Fix-v0.31.12-POMPA-PR2-MULTI-EPOCH-Windows.zip | 1 212 763 B | 14.10 | `1sAryNJM6I2JCBnzkABxA6nMjt0OmNq2a` |
| 0.31.12 | Android-Chart-Fix-v0.31.12-Windows.zip | 1 079 599 B | 13.10 | `1ciFRvt6md4ACzY4k_UOqULp03-auzTPd` |

### Snapshoty całościowe do analizy (najnowsze, nierozpakowane)

| plik | rozmiar | data | ID na Dysku |
|---|---|---|---|
| SterownikCO-AI-STEROWANIE-2026-10-28.zip | 1 460 463 B | 28.10 04:37 | `10_V8ruX_Xkslb4INN1FwtAt2D2lDTeG3` |
| SterownikCO-AI-ANALIZA-2026-10-26.zip | 1 068 844 B | 26.10 20:26 | `16BG-oWhrL0L0zHudECWMKACSALaBzEok` |
| Android-baza-przed-aktualizacja-84e10f3a….zip | 64 043 B | 26.10 11:16 | `1-ZBYbfHAJkgPhfTMaNs1yO1M6zEyzXrC` |

`Android-baza-przed-aktualizacja` powstała 12 minut przed paczką
`SterownikCO-Android-v0.31.34-NAPRAWA.zip` (26.10 11:28) — to kopia drzewa
sprzed nakładania tej poprawki, czyli stan v0.31.33.

### Paczki przyrostowe 0.31.28–0.31.34 (te same, które są w repo `android-sync/`)

`0.31.28` … `0.31.34` — brak w nich pełnego drzewa, tylko zmienione pliki.

## Czego z tego użyłem do 0.31.44

1. `android-sync/v0.31.22-offline-buffer/SterownikCO-Android-v0.31.22-PELNE-ZRODLA.zip`
   (1 119 837 B) — pełny projekt z 22.10, zbudowany na drzewie użytkownika v0.31.19.
2. Nakładki `android-sync/v0.31.23…v0.31.32/*.zip` — warstwy 0.31.23–0.31.32.
3. Na to scalone (git merge-file, baza = drzewo repo 0.31.11) nasze zmiany
   0.31.33–0.31.43.

Efekt: drzewo = **stan użytkownika z 0.31.32 + nasza praca 0.31.33–0.31.43**.

## Czego NIE dało się pobrać i dlaczego

Środowisko nie ma dostępu sieciowego do `drive.google.com` / `www.googleapis.com`
(połączenie TLS zrywane), a pliki binarne z Dysku wracają przez connector jako
base64 — 1,1–1,5 MB na plik to zbyt dużo, żeby je tędy przenieść.

Dlatego pełne snapshoty 0.31.18 i AI-* zostały **sprawdzone tylko po metadanych**
(nazwa, rozmiar, data), a nie po zawartości.

## Następny krok, jeśli coś jeszcze będzie brakować

Wgrać na czat plik `SterownikCO-AI-STEROWANIE-2026-10-28.zip` (jest na Dysku,
1 460 463 B, 28.10). Wtedy da się zrobić prawdziwy `diff` naszego drzewa
z drzewem użytkownika z 28.10 i wskazać co do pliku, czego jeszcze brakuje —
bez zgadywania.

## Stan weryfikacji 0.31.44

Brak JDK/Android SDK w środowisku → **nie skompilowano**. Wykonane kontrole
statyczne: nawiasy w 96 plikach .kt (0 rozjazdów), brak markerów konfliktu,
brak duplikatów deklaracji, wszystkie `m.*` / `Rtdb.*` / `S.*` / `Dimens.*` /
`Pal.*` / `Txt.*` używane przez UI mają definicje, brak osieroconych importów
`com.sterownikco.pro.*`.
