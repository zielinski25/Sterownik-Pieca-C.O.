# Etap 216 — „od przycisku do pliku” (v3.56.3)

Ten etap nie dokłada nowej funkcji. Sprawdza **szew** między dwiema rzeczami, które
dotąd były testowane osobno: **przyciskiem w panelu WWW** a **trasą pobrania w firmware**.
I poprawia to, co ten szew natychmiast pokazał.

Kandydat: **v3.56.3**. `release_ready=false`, `main_changed=false`, urządzenia nie tykałem.

---

## 1. Co było zepsute (znalezione tutaj, nie w testach)

### D2 — przycisk „Długa (archiwum)” **nie działał nigdy**

Co robił panel (v3.56.2):

1. `GET /history/link?stream=long` → centrala wydaje bilet **na strumień długi**,
2. panel otwiera `/history?k=<bilet>&s=long`.

Co robi trasa: `/history` jest zarejestrowana **zawsze jako strumień krótki**
(`historiaPobranie.obsluz(request, Strumien::Krotki)`), a bilet jest **związany ze
strumieniem** (`Bilety::zuzyj` porównuje strumień). Bilet długi na trasie krótkiej
nie pasuje → **401** i komunikat *„Link wygasł albo został już użyty - kliknij
pobieranie jeszcze raz”*.

Czyli: użytkownik klika „Długa”, dostaje informację o wygasłym linku, który **nigdy**
nie działał. Krótka historia działa (bo tam strumienie się zgadzają).

Dlaczego tego nie było widać: etap 213 sprawdził `link(stream=long)` (dawało bilet)
i `/history/long` z biletem długim (dawało 200). Nikt nie złożył z tego **adresu,
który panel naprawdę otwiera**.

### D3 — pobrany plik nie miał nazwy

Odpowiedź CSV nie miała `Content-Disposition`. Telefon (i część przeglądarek)
pokazuje wtedy CSV **jako tekst na stronie**, a nie zapisuje pliku — albo zapisuje
go bez rozszerzenia. Cały przycisk „pobierz” kończył się widokiem na ekranie.

---

## 2. Jaka jest poprawka (v3.56.3)

| plik | zmiana |
|---|---|
| `main_centrala.cpp` | panel wybiera trasę **według strumienia**: `/history` albo `/history/long`; nie dokleja już `s=` (trasa tego parametru nie czytała) |
| `main_centrala.cpp` | `FIRMWARE_VERSION` `v3.56.2` → `v3.56.3` (+ komentarz na górze pliku) |
| `HistoriaPobranie.h` | odpowiedź CSV dostaje `Content-Disposition: attachment; filename="historia-krotka.csv"` / `"historia-dluga.csv"` |

Kolejność include’ów z v3.56.2 (poprawka D1) **zostaje bez zmian** — etap tego pilnuje
osobnym sprawdzeniem.

---

## 3. Jak to jest sprawdzane (jedna brama, pięć kroków)

Wszystko idzie jedną komendą: `python3 -B narzedzia/od-przycisku216/run.py`

1. **`build.py`** — bierze przypięty `main_centrala.cpp` v3.55.8 (SHA-256), nakłada
   211+212+213+214+216 i porównuje **bajt w bajt** z patchem skumulowanym
   `v3.55.8 → v3.56.3`. Dodatkowo: patch skumulowany **cofnięty** musi oddać
   dokładnie plik v3.55.8. Z pliku po patchu wyciąga **prawdziwe** kawałki:
   tekst panelu, listę tras, treści trzech lambd, bramkę i obiekt trasy.
2. **`api216.py`** — nowy `HistoriaPobranie.h` i kod z pliku kompilują się wobec
   **prawdziwego** `ESPAsyncWebServer.h` (przypięty, LGPL-3.0-or-later) w `gnu++11`
   **i** `gnu++17`, z `-Werror`. Kontrola negatywna: kolejność include z etapu 213
   **musi** się nie kompilować.
3. **`panel_test.mjs`** — **prawdziwy skrypt panelu** uruchomiony w prawdziwym silniku
   JS (node). Sprawdza 26 rzeczy: który element dotyka, pod jaki adres idzie, co
   pokazuje przy błędzie, czy każdy `getElementById` trafia w istniejący element.
   Na koniec wypisuje **adresy, pod które panel naprawdę idzie** (`urls.txt`).
4. **`Test.cpp`** — te adresy (nie przepisane ręcznie!) idą do **prawdziwej trasy**
   z firmware: bilet → pobranie → porównanie bajtów z właściwym plikiem →
   `Content-Disposition` z nazwą. Trzy profile: `native`, `asan-ubsan`
   (z `detect_leaks=1`), `glibcxx-assertions`.
5. **pięć kontroli negatywnych** — każda musi zrobić bramę **czerwoną**:

| kontrola | co psujemy | co musi paść |
|---|---|---|
| N1a | panel ze **starego** pliku (v3.56.2) w JS | `J12-2`/`J13-dluga` — 2 błędy |
| N1b | te same stare adresy w **prawdziwej trasie** | 401 na długiej — 2 błędy |
| N2 | **stary** `HistoriaPobranie.h` (bez `Content-Disposition`) | 4 błędy: brak nazwy pliku |
| N3 | literówka `histBtn` → `histBtm` | `panel_test` czerwony |
| N4 | `bilet.length !== 32` → `!== 31` | `panel_test` czerwony |

Wynik ostatniego przebiegu: **`WYNIKI/run001/wynik.json`, status PASS** —
3 profile × 28 sprawdzeń, 26 sprawdzeń JS, 6 kontroli API, 5 kontroli negatywnych
wszystkie czerwone tam, gdzie mają być.

---

## 4. Co to dowodzi, a czego NIE dowodzi

**Dowodzi:**
- panel, taki jaki leci do przeglądarki, prowadzi do adresów, które **prawdziwa trasa
  przyjmuje** i które zwracają **właściwy plik** z **nazwą** (dla obu przycisków),
- nowy nagłówek kompiluje się wobec prawdziwego API biblioteki w obu standardach C++,
- patch jest odwracalny i odtwarzalny (łańcuch == skumulowany, cofnięcie == źródło),
- brama **nie jest ozdobą** — pięć kontroli negatywnych naprawdę ją wywala.

**NIE dowodzi:**
- że panel wygląda dobrze — DOM w `panel_test.mjs` to **atrapa** (cztery rzeczy,
  których skrypt używa), a nie przeglądarka; nie ma CSS, układu, dotyku,
- że plik naprawdę wyląduje w „Pobranych” na Twoim telefonie — to zależy od
  przeglądarki; `Content-Disposition` to jedyne, co firmware może w tej sprawie
  zrobić (i tego wcześniej brakowało),
- że cały `main_centrala.cpp` się skompiluje — to sprawdza PlatformIO u Ciebie,
- niczego o TLS, LittleFS, kolejkach TCP i zachowaniu przy zerwaniu łącza
  (to były etapy 196–211).

---

## 5. Pliki

| plik | co to jest |
|---|---|
| `zrob_patch.py` | generuje oba patche i nowy `HistoriaPobranie.h` z przypiętego źródła |
| `main_centrala-v3.56.2-to-v3.56.3.patch` | **przyrostowy** (dla drzewa już po 214) |
| `main_centrala-v3.55.8-to-v3.56.3.patch` | **skumulowany** (dla drzewa v3.55.8) |
| `HistoriaPobranie.h` | nagłówek v3.56.3 — **podmień plik**, nie doklejaj |
| `build.py`, `api216.py`, `panel_test.mjs`, `Test.cpp`, `run.py` | brama (5 kroków) |
| `Extracted216.inc`, `Trasy216.inc`, `panel/panel*.html`, `panel/panel.js` | wygenerowane z pliku po patchu — nie edytować |
| `WYNIKI/run001/` | wynik przebiegu: `wynik.json`, `urls.txt`, `urls.json` |

---

## 6. Co zrobić na komputerze

**Jeśli nie nakładałeś jeszcze patcha 214** (stan v3.55.8) — użyj **skumulowanego**,
on zawiera też poprawkę D1:

```bash
cd <katalog z main_centrala.cpp, HistoriaPin.h, HistoriaPobranie.h, HistoriaBilety.h>
patch -p1 -i main_centrala-v3.55.8-to-v3.56.3.patch
cp <paczka>/HistoriaPobranie.h .          # PODMIEŃ plik na wersję v3.56.3
grep -n 'FIRMWARE_VERSION = "v3.56.3"' main_centrala.cpp     # linia 5601
grep -n -A2 '<ESPAsyncWebServer.h>' main_centrala.cpp | head -4
```

Oczekiwane: `5242:#include <ESPAsyncWebServer.h>`, `5243 HistoriaPobranie.h`,
`5244 HistoriaBilety.h`, wersja w linii **5601**.

**Jeśli już nałożyłeś 214** (v3.56.2):

```bash
patch -p1 -i main_centrala-v3.56.2-to-v3.56.3.patch
cp <paczka>/HistoriaPobranie.h .
```

Potem, jak zwykle, PlatformIO. Jeśli coś nie przejdzie — pierwsza linia błędu wystarczy.

---

## 7. Co zostaje ryzykiem (nierozwiązane tutaj)

1. Siedem ryzyk z etapu 215 (`brama-api-esp215/README.md` §4) — bez zmian, żadne nie
   dotyczy tego, co zmienia 216.
2. **Zachowanie przeglądarki w telefonie** — czy pobierze plik, czy otworzy go w
   przeglądarce, zależy od systemu; firmware robi wszystko, co może
   (`Content-Disposition`), ale tego nie da się sprawdzić bez telefonu.
3. **Kolejka pobrań** — nadal jedno pobranie naraz (503 „Trwa inne pobranie”); nie
   ruszane, bo nie ma dowodu z urządzenia, że to przeszkadza.
4. **Stary panel w cache przeglądarki** — `/` idzie z `Cache-Control: no-store`, więc
   po wgraniu nowego firmware przycisk „Długa” będzie już nowy, ale jeśli telefon
   trzyma kartę otwartą od wczoraj, trzeba odświeżyć stronę.
