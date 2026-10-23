# Mapa czujników i instalacji

Dokument opisuje, **gdzie fizycznie siedzi każdy czujnik** w instalacji pana Rafała
i co z tego wynika dla analizy danych z Firebase.

Stan na **23.10.2026**. Opis rozmieszczenia pochodzi od użytkownika — to nie jest
domysł. Wartości temperatur pochodzą z eksportu RTDB (40 205 rekordów, 28 dni).

---

## 1. Schemat instalacji

Schemat narysowany przez użytkownika (23.10). **Fioletowe kulki = czujniki.**
Na rysunku są **dwie** — po jednej na każdym pionie.

```
              ┌──────────────────────────────────────┐
              │   RURA DO INSTALACJI (grzejniki,     │
              │   ciepła woda użytkowa)              │
              └──────────────────┬───────────────────┘
                                 │
              ┌──────────────────┴──────────────────┐
              │                                     │
         ● "ogrz"                             ● "trociny"
         (C_O)                                (C_O_trociny)
              │                                     │
              │  woda z kotła                       │  woda z płaszcza
              │                                     │  przystawki
       ┌──────┴───────┐                   ┌─────────┴────────┐
       │              │                   │                  │
       │  PIEC C.O.   │◄─── płomień ──────│   PALENISKO      │
       │              │   korytarz        │   TROCINIAK      │
       │  (drewno)    │   kilka cm        │  (płaszcz wodny) │
       └──────────────┘                   └──────────────────┘
```

Niebieska linia na rysunku użytkownika = **obieg wody** między piecem a przystawką.
Czerwona = **płomień** idący z paleniska trociniaka do pieca C.O.

### Jak to działa

1. **Przystawka na trociny** stoi przy piecu C.O. — to osobne palenisko na trociny.
2. Przystawka ma **płaszcz wodny** — woda w nim krąży, żeby ścianki się nie
   przegrzewały (temperatura spalania trocin jest bardzo wysoka).
3. Z płaszcza idzie **rura z wodą do instalacji grzewczej**.
4. Z przystawki wychodzi **płomień**, idzie **korytarzem o długości kilku centymetrów**
   i **ogrzewa ruszta pieca C.O.** — piec C.O. jest dodatkowym wymiennikiem ciepła.
5. Na obu pionach — nad piecem i nad przystawką — siedzą **po jednym czujniku**.

### Co z tego wynika dla czujnika `ogrz` — KOREKTA

W pierwszej wersji tego dokumentu przyjąłem, że czujnik `ogrz` leży na **wspólnej**
rurze, za miejscem zmieszania wody z kotła i z płaszcza (wariant A). **Rysunek
pokazuje, że jest inaczej.**

Czujnik `ogrz` siedzi na **własnym pionie nad piecem C.O.**, a czujnik `trociny` na
**własnym pionie nad przystawką**. To dwa osobne punkty pomiarowe, a nie jeden
po zmieszaniu.

**Czyli obowiązuje wariant B: `ogrz` mierzy czystą wodę z kotła C.O.**, zanim spotka
się z wodą z płaszcza przystawki.

To **dobra wiadomość** dla analiz: wszystkie liczby w raporcie opisują wodę z kotła,
bez domieszki. Nie ma czego przeliczać.

**Czego rysunek nie rozstrzyga:** w którą stronę płynie woda w tej pętli i czy obieg
przystawki jest normalnie otwarty, czy odcięty zaworem. Z danych wynika, że raczej
**odcięty** — bo przy piecu o temperaturze 90 °C czujnik `trociny` pokazywał 13,5 °C.
Gdyby woda z kotła przez niego przepływała, byłoby tam blisko 90 °C.

---
## 2. Wszystkie czujniki — tabela

Wszystkie czujniki temperatury to **DS18B20** na trzech magistralach 1-Wire.
Do tego dwa czujniki I2C/analogowe.

| # | kanał | czujnik w firmware | magistrala | pin | zakres | gdzie fizycznie |
|---|---|---|---|---|---|---|
| 0 | `zewn` | `czujnik_temp_zewnetrznej` | BUS_1 | GPIO 4 | −30…50 °C | **na zewnątrz, ściana wschodnia** — przy narożniku domu, nad kotłownią |
| 1 | `bojler` | `czujnik_temp_Bojlera` | BUS_2 | GPIO 5 | 0…160 °C | **zbiornik CWU pieca** |
| 2 | `ogrz` | `czujnik_temp_C_O` | BUS_2 | GPIO 5 | 0…160 °C | **na pionie nad piecem C.O. — woda z kotła** |
| 3 | `ogrz_sr` | *(brak — wyliczana)* | — | — | 10…99 °C | **to nie czujnik**, to średnia z `ogrz` |
| 4 | `powrot` | `czujnik_temp_C_O_powrot` | BUS_2 | GPIO 5 | −30…120 °C | rura powrotna, przed kotłem |
| 5 | `panel` | `czujnik_temp_Panel_Plus` | BUS_3 | GPIO 7 | −30…160 °C | zbiornik solarny 100 l na dachu |
| 6 | `pokoj` | `czujnik_temp_Wewnatrz` | BUS_1 | GPIO 4 | −50…50 °C | **środek pokoju, na wysokości 2,5 m** |
| 7 | `trociny` | `czujnik_temp_C_O_trociny` | BUS_2 | GPIO 5 | −30…120 °C | **na pionie nad przystawką — woda z jej płaszcza wodnego**. To **detektor stanu paleniska**, nie termometr pomiarowy |
| 8 | `rh` | DHT22 | — | GPIO 6 | 0…100 % | **na zewnątrz** |
| 9 | `cisn` | BMP280 | — | I2C | 970…1040 hPa | ciśnienie |
| 10 | `dym` | czujnik dymu | — | GPIO 1 | 0…4095 | w kanale kominowym |

### Dlaczego trzy magistrale

| magistrala | pin | co wisi |
|---|---|---|
| BUS_1 | GPIO 4 | pokój + zewnątrz |
| BUS_2 | GPIO 5 | wszystko przy piecu: zasilanie, powrót, przystawka, bojler |
| BUS_3 | GPIO 7 | tylko panel solarny |

**BUS_3 jest osobna, bo kabel idzie na dach.** Długa linia przy kominie i falowniku
zbiera zakłócenia; gdyby panel wisiał na wspólnej magistrali z piecem, zakłócenia
z dachu psułyby odczyty pieca.

---

## 3. Co to znaczy dla analizy danych

### 3.1 `ogrz` mierzy wodę z kotła — bez domieszki

**Sprostowanie.** W pierwszej wersji tego dokumentu napisałem, że `ogrz` mierzy wodę
po zmieszaniu kotła i płaszcza przystawki. **Rysunek od użytkownika pokazał, że jest
inaczej:** czujniki siedzą na dwóch osobnych pionach, każdy przy swoim urządzeniu.

```
   ● "ogrz"  ──── woda z KOTŁA C.O.        (osobny pion, nad piecem)
   ● "trociny" ── woda z PŁASZCZA PRZYSTAWKI (osobny pion, nad przystawką)
                        │
                        ▼
              dopiero dalej łączą się w instalacji
```

**Wniosek: `ogrz` mierzy czystą wodę z kotła.** Wszystkie analizy w raporcie
(bezwładność, stygnięcie, odbiór przez grzejniki) opisują więc wodę z kotła C.O. —
dokładnie to, co miały opisywać. **Nic nie trzeba przeliczać.**

**Za to `trociny` to nie temperatura paleniska trocin**, a **woda w płaszczu wodnym
przystawki** (na jej pionie). Mediana 13,5 °C oznacza, że woda w tym obiegu stała
zimna — czyli przystawka nie pracowała.

**Do czego ten czujnik naprawdę służy (wyjaśnienie użytkownika, 23.10):** nie do
pomiaru energii, a do **wykrywania, że palenisko trocin pracuje**. Gdy woda w płaszczu
się nagrzewa, program wie, że to palenisko jest w użyciu — i **serwa mają wtedy
pracować w trybie właściwym dla trocin**. To sygnał stanu, nie termometr.

**Obieg jest odcięty na czas letni** (woda spuszczona / zawór zamknięty). Dlatego
czujnik pokazuje temperaturę otoczenia.

### 3.2 `trociny` to temperatura przystawki, nie paleniska

Mediana **13,5 °C**, maksimum **22,3 °C** w ciągu 28 dni. To temperatury **otoczenia**,
nie paleniska. Wniosek: **przystawka na trociny w tym okresie nie była używana** —
stała zimna obok pracującego pieca.

Potwierdza to korelacja z temperaturą zasilania: **r = 0,58**. Czujnik reaguje na
palenie w piecu C.O., bo stoi obok niego i nagrzewa się od obudowy — a nie dlatego,
że sam coś pali.

Gdyby przystawka pracowała, `trociny` pokazywałoby kilkadziesiąt stopni (woda
w płaszczu) i rosło razem z paleniem trocin.

### 3.3 `bojler` to zbiornik CWU pieca

Mediana 16,5 °C, maksimum 71,3 °C, korelacja z zasilaniem **r = 0,84**. Zachowuje się
jak zbiornik podgrzewany od kotła — czyli CWU, a nie bufor.

### 3.4 `panel` jest całkowicie niezależny od pieca

Korelacja z zasilaniem **r = −0,13**, czyli brak związku. To dobrze: potwierdza, że
układ solarny i piec C.O. to dwie osobne instalacje, połączone tylko przez człowieka,
który z nich korzysta.

### 3.5 `ogrz_sr` to nie czujnik

W firmware jest to **średnia z `ogrz`, liczona tylko wtedy, gdy pompa pracuje**.
W 87,4% minut jest identyczna z `ogrz` co do 0,1 °C. Firmware ma osobną łatkę
(v2.4.0), żeby ta średnia nie zamarzała po wygaśnięciu pieca.

**Do analizy zawsze używaj `ogrz`, nie `ogrz_sr`.**

### 3.7 Czujnik `trociny` jako detektor paleniska

To nie jest zwykły termometr do liczenia energii. Jego zadanie to **powiedzieć
programowi, czy palenisko trocin pracuje**.

**Jak to działa w firmware v3.54.0** (sprawdzone w kodzie):

| element | próg | co robi |
|---|---|---|
| `MieszadloTrocinyAuto` | **> 30 °C** | włącza automatycznie mieszadło trocin |
| odczyt nieświeży | — | automatyka **ignoruje** stary lub błędny odczyt (nie włączy mieszadła na podstawie zamrożonej wartości) |
| ręczne wyłączenie | ≤ 30 °C | blokada trzyma się do ostygnięcia poniżej progu |

**W 28 dniach danych maksimum `trociny` wyniosło 22,3 °C** — próg 30 °C **ani raz nie
został przekroczony**. Czyli mieszadło nigdy się nie włączyło, a program przez cały
okres wiedział, że palenisko trocin stoi.

**O serwach:** w tej wersji firmware **nie ma jeszcze** powiązania serw z odczytem
trocin. Serwa mają trzy tryby (AUTO / RĘCZNY / BEZPIECZNA POZYCJA), ale żaden nie
zależy od `t_trociny`. To znaczy, że opisana przez użytkownika automatyczna zmiana
zachowania serw dla paleniska trocin **jest planem na przyszłość**, a nie czymś, co
już działa. Warto to wiedzieć, żeby nie oczekiwać tego po wgraniu v3.54.0.

**Co trzeba by dodać, gdy przyjdzie czas:** nową gałąź w logice serw, np.

```
if (t_trociny > PROG_TROCINY && odczyt_swiezy) {
    tryb serw = TRYB_DLA_TROCIN;   // np. inna pozycja klapy
} else {
    tryb serw = TRYB_DLA_DREWNA;
}
```

Przy czym próg trzeba dobrać **na podstawie pomiarów**, nie zgadywać — dlatego warto
najpierw poobserwować `trociny` przez kilka dni palenia w przystawce, zanim ustali się
liczbę.

---

## 4. Problemy znalezione w danych

| kanał | problem | szczegóły |
|---|---|---|
| `zewn` | **zawieszony odczyt 37,5 °C** | 3.10 od 17:07 przez kilkanaście minut dokładnie ta sama wartość co do 0,1° |
| `rh` | **mediana 92,9%** | bardzo wysoko; w pokoju niemożliwe (skraplanie). DHT22 jest raczej w kotłowni lub na zewnątrz |
| `ogrz` | 49 skoków > 6 °C | część to nakładanie dwóch urządzeń 15.10 (11:54–13:00), nie usterka czujnika |

---

## 5. Czego jeszcze nie wiem

Potwierdzone 23.10 przez użytkownika (razem z rysunkiem):

- ✅ `bojler` — zbiornik CWU pieca
- ✅ `zewn` — ściana wschodnia, przy narożniku domu nad kotłownią
- ✅ DHT22 — na zewnątrz
- ✅ `pokoj` — środek pokoju, na wysokości 2,5 m

Potwierdzone 23.10 (ciąg dalszy):

- ✅ **Obieg płaszcza jest odcięty na czas letni** — woda spuszczona, zawór zamknięty.
  Dlatego `trociny` pokazuje temperaturę otoczenia.
- ✅ **Czujnik `trociny` pełni rolę detektora paleniska** — ma informować program, że
  palenisko trocin pracuje, żeby serwa działały inaczej. Szczegóły i stan
  implementacji: sekcja 3.7.
- ✅ **Nie ma powiązania serw z `t_trociny` w v3.54.0** — to plan na przyszłość,
  obecnie działa tylko automatyka mieszadła (> 30 °C).

Nadal otwarte:

1. **Gdzie dokładnie w przystawce siedzi czujnik `trociny`** — w płaszczu wodnym, czy
   w komorze paleniska trocin? Wartości wskazują na płaszcz.
2. **Który to pokój** — bo od tego zależy, jak czytać τ budynku (czy czujnik jest
   w pomieszczeniu ogrzewanym bezpośrednio przez piec, czy daleko od niego).

### 3.6 Co zmienia wysokość 2,5 m i ściana wschodnia

Dwa potwierdzone szczegóły mają realny wpływ na to, jak czytać wcześniejsze analizy.

**Pokój na wysokości 2,5 m — to pod sufitem.** Powietrze w pomieszczeniu się
warstwuje: pod sufitem jest zwykle **2–4 °C cieplej** niż na wysokości 1,5 m, gdzie
przebywa człowiek. Więc:

| co pokazuje czujnik | co jest naprawdę w strefie przebywania |
|---|---|
| 17,0 °C (mediana) | około 14–15 °C |
| 15,0 °C (minimum) | około 12–13 °C |

**To nie psuje stałej czasowej τ = 55,6 h** — bo τ liczy się z tempa zmian, a nie
z poziomu temperatury. Przesunięcie o stałą wartość nie zmienia wyniku.

**Ale zmienia wniosek o komforcie.** Zdania typu „dom trzyma 15–17 °C" trzeba czytać
jako „**pod sufitem** 15–17 °C, czyli w strefie przebywania bliżej 12–15 °C". To już
chłodno — i wyjaśnia, dlaczego palisz codziennie.

**Ściana wschodnia.** Czujnik `zewn` dostaje bezpośrednie słońce **rano** (w Polsce
mniej więcej od 6 do 11). Sprawdziłem w danych, czy to widać:

| godzina lokalna | dni słoneczne | dni pochmurne | różnica |
|---|---|---|---|
| 10:00 | 14,1 °C | 13,4 °C | +0,7 |
| 13:00 | 20,6 °C | 17,2 °C | +3,4 |
| **17:00** | **22,9 °C** | **17,7 °C** | **+5,2** |
| 22:00 | 15,2 °C | 14,1 °C | +1,1 |

**Nie widać porannego „garbu", którego należałoby oczekiwać od wschodniej ściany.**
Różnica między dniami słonecznymi a pochmurnymi rośnie dopiero po południu — czyli
czujnik mierzy nagrzane **powietrze**, a nie nagrzaną ścianę. To znaczy, że jest
prawdopodobnie **osłonięty albo w cieniu** i mierzy poprawnie.

**Wniosek: dane `zewn` są w porządku do analiz.** Ale ten jeden odczyt **37,5 °C
o 17:07 dnia 3.10** i tak jest awarią — o tej porze wschodnia ściana jest w cieniu,
więc to na pewno zawieszony czujnik, a nie słońce.

**DHT22 na zewnątrz** — to wyjaśnia medianę wilgotności **92,9%**. W październiku
na dworze to normalne (mgły, deszcz, noce). W pokoju byłoby to niemożliwe.

---

## 6. Powiązane dokumenty

- `ANALIZA_PIECA_BEZWLADNOSC_2026-10-23.md` — pełna analiza danych (rozdział 13:
  dlaczego czujnik na rurze zmienia interpretację stałej czasowej)
- `ARCHIWUM_SOLARNE_STAN.md` — stan archiwum solarnego (osobny układ, zbiornik 100 l)
- `narzedzia/analiza_pieca.py` — skrypt analizy (mapa kanałów w stałej `CH`)
