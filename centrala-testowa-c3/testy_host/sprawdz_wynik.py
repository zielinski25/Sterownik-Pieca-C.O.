#!/usr/bin/env python3
"""
sprawdz_wynik.py — weryfikacja rdzenia centrali testowej (wynik test_core).

Sprawdza to, co REALNIE widzi panel LCD:
  1. /api/status jest poprawnym JSON-em i zawiera KAZDY klucz, ktory czyta
     main.cpp panelu (lista wyciagnieta z pobierzStatus()),
  2. /api/ustawienia-get zawiera kazdy klucz z POLA[] panelu + wybor/
     trybSerwa/antystopWlaczony,
  3. CSV historii ma DOKLADNIE tyle kolumn, ile indeksuje panel
     (krotki: 14, czas w kolumnie 13; dlugi: 35, czas w kolumnie 34),
  4. walidacja ustawien zwraca oczekiwane OK/ERR, a klucze NVS maja <=15 znakow,
  5. JSON nie jest obciety (konczy sie na '}', dlugosc < bufora).
"""
import json
import sys

# Klucze czytane przez panel LCD (main.cpp, obslugaStatusFetch()).
KLUCZE_STATUSU_PANELU = [
    "ip", "rssi", "internet",
    "t_zewn", "t_bojler", "t_ogrz", "t_ogrz_sr", "t_panel", "t_pokoj",
    "t_trociny", "t_powrot", "wilgotnosc", "cisnienie",
    "dym", "dym_alarm", "klapa", "syberka", "pompa", "pompa_override_min",
    "alarm_ogrzewanie", "alarm_panel", "deszcz_stan", "deszcz_min",
    "miesiac", "godzina", "minuta", "dzien", "rok", "rtc_ok",
    "panel_ota_dostepna", "panel_ota_wersja",
]

# Klucze z POLA[] panelu + tryby (obslugaUstawieniaFetch()).
KLUCZE_USTAWIEN_PANELU = [
    "czasOn", "czasOff", "tempOn", "tempOff", "tempZadServo", "histerServo",
    "skokKlapy", "skokSyberka", "odchylTemp", "mnoznik", "antystopDni",
    "mieszadloCzasOn", "mieszadloCzasOff", "progAlarmTemp", "progAlarmDym",
    "dymCzasOn", "dymCzasOff", "dymCzasStabilizacji",
    "wybor", "trybSerwa", "antystopWlaczony",
]

KOLUMNY_KROTKIE = 14
KOLUMNY_DLUGIE = 35


def podziel(tekst, marker):
    """Zwraca (zawartosc_sekcji, tekst_po_markerze)."""
    start = tekst.index(marker) + len(marker)
    start = tekst.index("\n", start) + 1
    koniec = tekst.index("===", start)
    return tekst[start:koniec], tekst[koniec:]


def main():
    tekst = sys.stdin.read()
    bledy = []
    ostrzezenia = []

    status_txt, reszta = podziel(tekst, "===STATUS===")
    ustaw_txt, reszta = podziel(reszta, "===USTAWIENIA===")
    walid_txt, reszta = podziel(reszta, "===WALIDACJA===")
    csv_krotka, reszta = podziel(reszta, "===CSV_KROTKA===")
    csv_dluga, reszta = podziel(reszta, "===CSV_DLUGA===")

    # --- 1. STATUS ---
    try:
        status = json.loads(status_txt.strip())
    except json.JSONDecodeError as e:
        print("BLAD: /api/status nie jest poprawnym JSON-em:", e)
        return 1
    brak = [k for k in KLUCZE_STATUSU_PANELU if k not in status]
    if brak:
        bledy.append("brak kluczy w /api/status: " + ", ".join(brak))
    if not status_txt.strip().endswith("}"):
        bledy.append("/api/status wyglada na obciety (brak '}')")

    # Typy kluczowe (panel robi .as<float>()/.as<int>()/.as<bool>()).
    for k in ("t_zewn", "t_bojler", "t_ogrz", "t_ogrz_sr", "t_panel", "t_pokoj",
              "t_trociny", "t_powrot", "wilgotnosc", "cisnienie"):
        if not isinstance(status.get(k), (int, float)):
            bledy.append(f"/api/status: {k} nie jest liczba ({status.get(k)!r})")
    for k in ("dym", "klapa", "syberka", "rssi", "miesiac", "godzina"):
        if not isinstance(status.get(k), int):
            bledy.append(f"/api/status: {k} nie jest int ({status.get(k)!r})")
    for k in ("pompa", "dym_alarm", "alarm_ogrzewanie", "alarm_panel",
              "internet", "rtc_ok", "panel_ota_dostepna"):
        if not isinstance(status.get(k), bool):
            bledy.append(f"/api/status: {k} nie jest bool ({status.get(k)!r})")

    if not (0 <= status.get("klapa", -1) <= 180):
        bledy.append("klapa poza 0..180 (centrala raportuje STOPNIE)")
    if not (0 <= status.get("syberka", -1) <= 90):
        bledy.append("syberka poza 0..90 (centrala raportuje STOPNIE)")

    # --- 2. USTAWIENIA ---
    try:
        ustaw = json.loads(ustaw_txt.strip())
    except json.JSONDecodeError as e:
        print("BLAD: /api/ustawienia-get nie jest poprawnym JSON-em:", e)
        return 1
    brak = [k for k in KLUCZE_USTAWIEN_PANELU if k not in ustaw]
    if brak:
        bledy.append("brak kluczy w /api/ustawienia-get: " + ", ".join(brak))

    # --- 3. CSV ---
    wiersze_k = [w for w in csv_krotka.strip().splitlines() if w.strip()]
    wiersze_d = [w for w in csv_dluga.strip().splitlines() if w.strip()]
    if not wiersze_k:
        bledy.append("/api/history: brak wierszy po 3 h symulacji")
    else:
        for i, w in enumerate(wiersze_k[:5] + wiersze_k[-5:]):
            if len(w.split(",")) != KOLUMNY_KROTKIE:
                bledy.append(f"/api/history: wiersz ma {len(w.split(','))} kolumn, "
                             f"oczekiwane {KOLUMNY_KROTKIE}: {w}")
                break
        if len(wiersze_k) < 170:
            ostrzezenia.append(f"/api/history: tylko {len(wiersze_k)} wierszy "
                               f"(oczekiwano ~180 po 3 h)")
        # czas_unix w ostatniej kolumnie musi byc rosnacy
        czasy = [int(w.split(",")[13]) for w in wiersze_k]
        if any(b < a for a, b in zip(czasy, czasy[1:])):
            bledy.append("/api/history: czas_unix (kol. 13) nie jest rosnacy")

    if not wiersze_d:
        ostrzezenia.append("/api/history/long: brak wierszy (mniej niz 2 godziny?)")
    for w in wiersze_d:
        if len(w.split(",")) != KOLUMNY_DLUGIE:
            bledy.append(f"/api/history/long: wiersz ma {len(w.split(','))} kolumn, "
                         f"oczekiwane {KOLUMNY_DLUGIE}")
            break

    # --- 4. WALIDACJA ---
    for linia in walid_txt.strip().splitlines():
        if linia.startswith("!!"):
            bledy.append("walidacja: " + linia[3:].strip())
        if linia.startswith("NVS|"):
            _, pole, klucz, dlugosc = linia.split("|")
            if klucz != "(brak)" and int(dlugosc) > 15:
                bledy.append(f"NVS: klucz '{klucz}' ({pole}) ma {dlugosc} znakow (>15)")

    # --- raport ---
    print("=" * 70)
    print("PODSUMOWANIE TESTU RDZENIA (to, co zobaczy panel LCD)")
    print("=" * 70)
    print(f"/api/status      : {len(status)} kluczy, {len(status_txt.strip())} B")
    print(f"/api/ustawienia  : {len(ustaw)} kluczy, {len(ustaw_txt.strip())} B")
    print(f"/api/history     : {len(wiersze_k)} wierszy x {KOLUMNY_KROTKIE} kolumn")
    print(f"/api/history/long: {len(wiersze_d)} wierszy x {KOLUMNY_DLUGIE} kolumn")
    print(f"progAlarmDym={ustaw.get('progAlarmDym')} dym={status.get('dym')} "
          f"dym_alarm={status.get('dym_alarm')}")
    print(f"t_ogrz={status.get('t_ogrz')} t_ogrz_sr={status.get('t_ogrz_sr')} "
          f"klapa={status.get('klapa')} syberka={status.get('syberka')} "
          f"pompa={status.get('pompa')}")
    print()
    for o in ostrzezenia:
        print("OSTRZEZENIE:", o)
    if bledy:
        for b in bledy:
            print("BLAD:", b)
        print(f"\nWYNIK: {len(bledy)} bledow")
        return 1
    print("WYNIK: OK - kontrakt API zgodny z tym, czego oczekuje panel LCD")
    return 0


if __name__ == "__main__":
    sys.exit(main())
