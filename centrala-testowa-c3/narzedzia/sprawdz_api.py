#!/usr/bin/env python3
"""
sprawdz_api.py — test kontraktu API centrali testowej (ESP32-C3) PO SIECI.

Uruchamiaj z telefonu/PC podlaczonego do sieci "PiecCO_AP" (tej, ktora stawia
C3). Skrypt odpytuje urzadzenie DOKLADNIE tak, jak robi to panel LCD, i mówi,
czy wszystko, czego panel potrzebuje, jest na miejscu.

    python3 sprawdz_api.py                # domyslnie http://192.168.4.1
    python3 sprawdz_api.py 192.168.4.1    # albo inny adres

Sprawdza:
  1. /api/status      — poprawny JSON + komplet kluczy czytanych przez panel,
  2. /api/ustawienia-get — komplet kluczy z POLA[] panelu + tryby,
  3. /api/ustawienia-set — zapis dziala, a bledna wartosc jest ODRZUCANA
     (walidacja taka sama jak w prawdziwej centrali),
  4. /api/servo-manual  — odmowa w trybie AUTO, przyjecie w trybie RECZNY,
  5. /api/pompa-manual  — wymuszenie WL/WYL + powrot do AUTO,
  6. /api/history (14 kol.) i /api/history/long (35 kol.) — liczba kolumn,
  7. /api/rtc-sync      — ustawienie czasu (opcjonalnie, --czas).
"""
import argparse
import json
import sys
import urllib.error
import urllib.parse
import urllib.request

KLUCZE_STATUSU = [
    "ip", "rssi", "internet", "t_zewn", "t_bojler", "t_ogrz", "t_ogrz_sr",
    "t_panel", "t_pokoj", "t_trociny", "t_powrot", "wilgotnosc", "cisnienie",
    "dym", "dym_alarm", "klapa", "syberka", "pompa", "pompa_override_min",
    "alarm_ogrzewanie", "alarm_panel", "deszcz_stan", "deszcz_min", "miesiac",
    "godzina", "minuta", "dzien", "rok", "rtc_ok", "panel_ota_dostepna",
    "panel_ota_wersja",
]
KLUCZE_USTAWIEN = [
    "czasOn", "czasOff", "tempOn", "tempOff", "tempZadServo", "histerServo",
    "skokKlapy", "skokSyberka", "odchylTemp", "mnoznik", "antystopDni",
    "mieszadloCzasOn", "mieszadloCzasOff", "progAlarmTemp", "progAlarmDym",
    "dymCzasOn", "dymCzasOff", "dymCzasStabilizacji", "wybor", "trybSerwa",
    "antystopWlaczony",
]


class Klient:
    def __init__(self, baza):
        self.baza = baza.rstrip("/")

    def get(self, sciezka, timeout=6):
        with urllib.request.urlopen(self.baza + sciezka, timeout=timeout) as r:
            return r.status, r.read().decode("utf-8", "replace")

    def post(self, sciezka, pola, timeout=6):
        dane = urllib.parse.urlencode(pola).encode()
        req = urllib.request.Request(self.baza + sciezka, data=dane, method="POST",
                                     headers={"Content-Type": "application/x-www-form-urlencoded"})
        try:
            with urllib.request.urlopen(req, timeout=timeout) as r:
                return r.status, r.read().decode("utf-8", "replace")
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode("utf-8", "replace")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("host", nargs="?", default="192.168.4.1")
    ap.add_argument("--czas", action="store_true", help="ustaw czas z tego komputera")
    ap.add_argument("--pelny", action="store_true", help="pokaz pelne JSON-y")
    args = ap.parse_args()

    baza = args.host if args.host.startswith("http") else "http://" + args.host
    k = Klient(baza)
    bledy, ostrzezenia = [], []

    print(f"Centrala testowa: {baza}")

    # --- 1. /api/status ---
    try:
        kod, tekst = k.get("/api/status?panel_fw=vTEST&reset_powod=sw")
    except Exception as e:
        print(f"\nBLAD: nie moge polaczyc sie z {baza} ({e})")
        print("Sprawdz, czy jestes w sieci WiFi 'PiecCO_AP' (haslo: KociolCO2026).")
        return 2
    if kod != 200:
        bledy.append(f"/api/status zwrocilo HTTP {kod}")
    try:
        status = json.loads(tekst)
    except json.JSONDecodeError as e:
        print("BLAD: /api/status to nie JSON:", e)
        print(tekst[:300])
        return 1
    brak = [x for x in KLUCZE_STATUSU if x not in status]
    if brak:
        bledy.append("brak kluczy w /api/status: " + ", ".join(brak))

    print(f"  /api/status       OK  ({len(status)} kluczy, {len(tekst)} B)")
    print(f"    piec={status.get('t_ogrz')}C (sr {status.get('t_ogrz_sr')}C) "
          f"bojler={status.get('t_bojler')}C zewn={status.get('t_zewn')}C")
    print(f"    klapa={status.get('klapa')}st syberka={status.get('syberka')}st "
          f"pompa={status.get('pompa')} dym={status.get('dym')} "
          f"alarm_dym={status.get('dym_alarm')}")
    print(f"    czas: rtc_ok={status.get('rtc_ok')} "
          f"{status.get('rok')}-{status.get('miesiac')}-{status.get('dzien')} "
          f"{status.get('godzina')}:{status.get('minuta')}")
    if not status.get("rtc_ok"):
        ostrzezenia.append("rtc_ok=false — panel pokaze '--:--'. Ustaw czas: "
                           "python3 sprawdz_api.py --czas (albo przycisk na stronie 192.168.4.1)")
    if args.pelny:
        print(json.dumps(status, indent=2, ensure_ascii=False))

    # --- 2. /api/ustawienia-get ---
    kod, tekst = k.get("/api/ustawienia-get")
    try:
        ustaw = json.loads(tekst)
    except json.JSONDecodeError:
        bledy.append("/api/ustawienia-get to nie JSON")
        ustaw = {}
    brak = [x for x in KLUCZE_USTAWIEN if x not in ustaw]
    if brak:
        bledy.append("brak kluczy w /api/ustawienia-get: " + ", ".join(brak))
    print(f"  /api/ustawienia   OK  ({len(ustaw)} kluczy)")
    if args.pelny:
        print(json.dumps(ustaw, indent=2, ensure_ascii=False))

    # --- 3. /api/ustawienia-set: poprawna zmiana + odrzucenie ---
    kod, tekst = k.post("/api/ustawienia-set", {"pole": "czasOn", "wartosc": "12"})
    try:
        odp = json.loads(tekst)
    except json.JSONDecodeError:
        odp = {}
    if kod != 200 or not odp.get("ok"):
        bledy.append(f"/api/ustawienia-set czasOn=12 nie przyjete (HTTP {kod}: {tekst[:120]})")
    else:
        print("  /api/ustawienia-set  OK  (czasOn=12 przyjete)")
    # przywroc sensowna wartosc
    k.post("/api/ustawienia-set", {"pole": "czasOn", "wartosc": "10"})

    kod, tekst = k.post("/api/ustawienia-set", {"pole": "czasOn", "wartosc": "999"})
    if kod != 400:
        bledy.append(f"walidacja: czasOn=999 powinno byc odrzucone (HTTP 400), "
                     f"jest HTTP {kod}")
    else:
        print(f"  walidacja            OK  (czasOn=999 odrzucone: {tekst[:80]})")

    # --- 4. serwo: AUTO musi odmowic, RECZNY przyjac ---
    k.post("/api/ustawienia-set", {"pole": "trybSerwa", "wartosc": "1"})
    kod, tekst = k.post("/api/servo-manual", {"os": "klapa", "procent": "40"})
    if kod != 400:
        ostrzezenia.append("serwo-manual w trybie AUTO powinno zwrocic 400 (jak w centrali)")
    else:
        print("  servo AUTO           OK  (odmowa, jak w centrali)")

    k.post("/api/ustawienia-set", {"pole": "trybSerwa", "wartosc": "2"})
    kod, tekst = k.post("/api/servo-manual", {"os": "klapa", "procent": "40"})
    if kod != 200:
        bledy.append(f"serwo-manual w trybie RECZNY odrzucone (HTTP {kod}: {tekst[:120]})")
    else:
        print("  servo RECZNY         OK  (klapa 40% przyjete)")
    k.post("/api/ustawienia-set", {"pole": "trybSerwa", "wartosc": "1"})

    # --- 5. pompa: wymus + powrot do auto ---
    kod, tekst = k.post("/api/pompa-manual", {"stan": "wl"})
    if kod != 200:
        bledy.append(f"pompa-manual stan=wl odrzucone (HTTP {kod})")
    else:
        kod2, t2 = k.get("/api/status")
        s2 = json.loads(t2)
        if not s2.get("pompa"):
            bledy.append("po pompa-manual stan=wl /api/status nie pokazuje pompa=true")
        else:
            print(f"  pompa WYMUS WL       OK  (override {s2.get('pompa_override_min')} min)")
    k.post("/api/pompa-manual", {"stan": "auto"})
    print("  pompa AUTO           OK  (powrot do automatyki)")

    # --- 6. historia CSV ---
    for sciezka, kolumn in (("/api/history", 14), ("/api/history/long", 35)):
        kod, tekst = k.get(sciezka, timeout=12)
        wiersze = [w for w in tekst.strip().splitlines() if w.strip()]
        if kod != 200:
            bledy.append(f"{sciezka}: HTTP {kod}")
            continue
        if not wiersze:
            ostrzezenia.append(f"{sciezka}: brak wierszy (uruchom chwile dluzej)")
            continue
        zle = [w for w in wiersze[:20] if len(w.split(",")) != kolumn]
        if zle:
            bledy.append(f"{sciezka}: {len(zle[0].split(','))} kolumn zamiast {kolumn}: "
                         f"{zle[0][:80]}")
        else:
            print(f"  {sciezka:<20} OK  ({len(wiersze)} wierszy x {kolumn} kolumn)")

    # --- 7. czas (opcjonalnie) ---
    if args.czas:
        import datetime
        d = datetime.datetime.now()
        kod, tekst = k.get("/api/rtc-sync?" + urllib.parse.urlencode({
            "rok": d.year, "miesiac": d.month, "dzien": d.day,
            "godzina": d.hour, "minuta": d.minute, "sekunda": d.second}))
        if kod == 200:
            print(f"  czas ustawiony na    {d:%Y-%m-%d %H:%M:%S}")
        else:
            bledy.append(f"/api/rtc-sync odrzucone (HTTP {kod}: {tekst[:100]})")

    # --- raport ---
    print()
    for o in ostrzezenia:
        print("OSTRZEZENIE:", o)
    for b in bledy:
        print("BLAD:", b)
    if bledy:
        print(f"\nWYNIK: {len(bledy)} bledow — panel LCD moze dostawac niepelne dane.")
        return 1
    print("WYNIK: OK — centrala testowa odpowiada tak, jak oczekuje panel LCD.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
