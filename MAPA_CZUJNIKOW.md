# Mapa czujników i instalacji

Dokument opisuje, **gdzie fizycznie siedzi każdy czujnik** w instalacji pana Rafała
i co z tego wynika dla analizy danych z Firebase.

Stan na **23.10.2026**. Opis rozmieszczenia pochodzi od użytkownika — to nie jest
domysł. Wartości temperatur pochodzą z eksportu RTDB (40 205 rekordów, 28 dni).

---

## 1. Schemat instalacji

```
                              ┌──────────┐
                              │  KOMIN   │
                              └────▲─────┘
                                   │ spaliny
   ┌───────────────────────────────┴──────────────────────┐
   │                      PIEC C.O.                       │
   │                                                      │
   │   ┌──────────────────┐                               │
   │   │   RUSZTA         │◄─── płomień z przystawki      │
   │   │   (palenisko     │     przez korytarz            │
   │   │    drewna)       │     kilkucentymetrowy         │
   │   └──────────────────┘                               │
   │                                                      │
   │            woda w kotle                              │
   └────────────────┬─────────────────────────────────────┘
                    │
                    │  RURA WYJŚCIOWA  ──►  instalacja grzewcza
                    │  ★ czujnik "ogrz" (C_O)        (grzejniki)
                    │
                    │  ← tu wpięta rura z płaszcza przystawki
                    │
   ┌────────────────┴─────────────────────────────────────┐
   │            PRZYSTAWKA NA TROCINY                     │
   │                                                      │
   │   ┌──────────────────────┐                           │
   │   │  palenisko trocin    │──── płomień ──► korytarz  │
   │   └──────────────────────┘      do ruszt pieca C.O.  │
   │                                                      │
   │   ┌──────────────────────┐                           │
   │   │   PŁASZCZ WODNY      │  ★ czujnik "trociny"      │
   │   │  (chłodzi przystawkę,│                           │
   │   │   żeby się nie        │                           │
   │   │   przegrzewała)      │                           │
   │   └──────────┬───────────┘                           │
   └──────────────┼───────────────────────────────────────┘
                  │
                  │  rura z płaszcza ──► wpięta w rurę wyjściową
                  │                       pieca C.O. (za kotłem)
                  ▼
```

### Jak to działa

1. **Przystawka na trociny** stoi przy piecu C.O. — to osobne palenisko na trociny.
2. Przystawka ma **płaszcz wodny** — woda w nim krąży, żeby ścianki przystawki się
   nie przegrzewały (temperatura spalania trocin jest bardzo wysoka).
3. Z płaszcza idzie **rura z wodą do instalacji grzewczej**.
4. Ta rura **wpina się w rurę wyjściową pieca C.O., zaraz za kotłem**.
5. Z przystawki wychodzi **płomień**, idzie **korytarzem o długości kilku centymetrów**
   i **ogrzewa ruszta pieca C.O.** — czyli piec C.O. jest dodatkowym wymiennikiem
   ciepła dla spalania trocin.

To znaczy, że **piec C.O. i przystawka pracują na wspólną instalację**, a ich wody
mieszają się zaraz za kotłem.

---

## 2. Wszystkie czujniki — tabela

Wszystkie czujniki temperatury to **DS18B20** na trzech magistralach 1-Wire.
Do tego dwa czujniki I2C/analogowe.

| # | kanał | czujnik w firmware | magistrala | pin | zakres | gdzie fizycznie |
|---|---|---|---|---|---|---|
| 0 | `zewn` | `czujnik_temp_zewnetrznej` | BUS_1 | GPIO 4 | −30…50 °C | na zewnątrz, na ścianie |
| 1 | `bojler` | `czujnik_temp_Bojlera` | BUS_2 | GPIO 5 | 0…160 °C | zbiornik CWU pieca |
| 2 | `ogrz` | `czujnik_temp_C_O` | BUS_2 | GPIO 5 | 0…160 °C | **rura wyjściowa z pieca C.O.**, za miejscem wpięcia rury z przystawki |
| 3 | `ogrz_sr` | *(brak — wyliczana)* | — | — | 10…99 °C | **to nie czujnik**, to średnia z `ogrz` |
| 4 | `powrot` | `czujnik_temp_C_O_powrot` | BUS_2 | GPIO 5 | −30…120 °C | rura powrotna, przed kotłem |
| 5 | `panel` | `czujnik_temp_Panel_Plus` | BUS_3 | GPIO 7 | −30…160 °C | zbiornik solarny 100 l na dachu |
| 6 | `pokoj` | `czujnik_temp_Wewnatrz` | BUS_1 | GPIO 4 | −50…50 °C | pokój |
| 7 | `trociny` | `czujnik_temp_C_O_trociny` | BUS_2 | GPIO 5 | −30…120 °C | **w przystawce na trociny** (płaszcz wodny) |
| 8 | `rh` | DHT22 | — | GPIO 6 | 0…100 % | wilgotność — miejsce do potwierdzenia |
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

### Wątpliwość interpretacyjna — do potwierdzenia

Opis użytkownika mówi: *„z płaszcza idzie rura z wodą (na niej jest też czujnik) do
instalacji grzewczej ciepłej wody zaraz za wyjściem z pieca C.O."*

To można rozumieć na dwa sposoby:

| wariant | gdzie jest czujnik `ogrz` | co mierzy |
|---|---|---|
| **A** (przyjęty w tym dokumencie) | na wspólnej rurze **za** wpięciem rury z płaszcza | mieszaninę: kocioł + przystawka |
| **B** | na samej rurze z płaszcza, przed wpięciem | tylko wodę z płaszcza przystawki |

Przyjąłem wariant **A**, bo nazwa czujnika to `C_O` (centralne ogrzewanie), a nie
„przystawka" — czyli logicznie powinien mierzyć wodę idącą do instalacji. **Jeśli
w rzeczywistości jest inaczej, analizy w rozdziale 3 raportu opisują wodę z płaszcza
przystawki, a nie z kotła** — i trzeba je przeliczyć od nowa.

W danych za wariantem A przemawia to, że `ogrz` reaguje na palenie drewna w piecu C.O.
(rozpałka podnosi je o 50 °C w 15 minut), a przystawka w tym okresie stała zimna
(`trociny` ≈ 13,5 °C). Gdyby czujnik siedział tylko na rurze z płaszcza, nie widziałby
palenia w piecu.

---

## 3. Co to znaczy dla analizy danych

### 3.1 `ogrz` mierzy wodę po zmieszaniu dwóch źródeł

To najważniejszy wniosek z opisu przystawki. Czujnik `C_O` siedzi na rurze wyjściowej
**za miejscem, w którym wpięta jest rura z płaszcza przystawki**. Więc mierzy:

```
woda z kotła C.O.  ──┐
                     ├──► [czujnik "ogrz"] ──► instalacja
woda z płaszcza    ──┘
przystawki
```

Gdyby przystawka pracowała, `ogrz` pokazywałoby temperaturę **mieszaniny** — czyli
zaniżoną albo zawyżoną względem samego kotła, zależnie od tego, co akurat grzeje mocniej.

**W praktyce w tych danych przystawka nie pracowała** — patrz 3.2. Więc `ogrz` to
temperatura wody z kotła C.O., ale trzeba o tym pamiętać, gdyby kiedyś przystawka
została uruchomiona: **od tego momentu porównywanie danych sprzed i po nie ma sensu
bez rozdzielenia źródeł.**

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

---

## 4. Problemy znalezione w danych

| kanał | problem | szczegóły |
|---|---|---|
| `zewn` | **zawieszony odczyt 37,5 °C** | 3.10 od 17:07 przez kilkanaście minut dokładnie ta sama wartość co do 0,1° |
| `rh` | **mediana 92,9%** | bardzo wysoko; w pokoju niemożliwe (skraplanie). DHT22 jest raczej w kotłowni lub na zewnątrz |
| `ogrz` | 49 skoków > 6 °C | część to nakładanie dwóch urządzeń 15.10 (11:54–13:00), nie usterka czujnika |

---

## 5. Czego jeszcze nie wiem

Te punkty wymagają potwierdzenia przez użytkownika — nie wpisuję ich jako faktu:

1. **Gdzie dokładnie w przystawce siedzi czujnik `trociny`** — w płaszczu wodnym, czy
   w komorze paleniska trocin? Wartości wskazują na płaszcz (albo na komorę zimnej
   przystawki).
2. **Czy płaszcz przystawki jest odcięty zaworem**, gdy przystawka nie pracuje? Jeśli
   nie — woda z pieca powinna go podgrzewać, a `trociny` pokazywałoby więcej niż 13 °C.
3. **Gdzie jest DHT22** — kotłownia czy zewnątrz?
4. **Na której ścianie jest `zewn`** — nasłonecznionej czy północnej? Ma to znaczenie
   dla interpretacji (37,5 °C mogło być słońcem).
5. **Który to pokój** dla czujnika `pokoj` — blisko pieca czy daleko?

---

## 6. Powiązane dokumenty

- `ANALIZA_PIECA_BEZWLADNOSC_2026-10-23.md` — pełna analiza danych (rozdział 13:
  dlaczego czujnik na rurze zmienia interpretację stałej czasowej)
- `ARCHIWUM_SOLARNE_STAN.md` — stan archiwum solarnego (osobny układ, zbiornik 100 l)
- `narzedzia/analiza_pieca.py` — skrypt analizy (mapa kanałów w stałej `CH`)
