# -*- coding: utf-8 -*-
"""
Agent sprzatajacy dysk - petla: skanuj -> ocen -> pokaz -> potwierdz -> przenies.

Uzycie (Windows 11, PowerShell):
    python agent.py                 # skan + raport + pytanie o potwierdzenie
    python agent.py --tylko-raport   # nic nie rusza, tylko raport
    python agent.py --bez-ai         # same reguly, zero modelu (najszybsze)
    python agent.py --model qwen2.5:7b-instruct-q4_K_M
    python agent.py --przywroc        # cofa ostatnia kwarantanne

Podzial odpowiedzialnosci jest celowy:
  - skaner.py     mierzy dysk (czysty Python, szybko)
  - reguly.py     decyduje, co jest smieciem (czytelne, edytowalne)
  - ollama_klient ocenia TYLKO sporne przypadki (ryzyko 3)
  - wykonawca.py  przenosi do kwarantanny, po Twoim "tak"
Model nigdy nie dotyka systemu plikow - dostaje liste i zwraca JSON.
"""

import argparse
import datetime as dt
import json
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, str(Path(__file__).resolve().parent))
import reguly as R                    # noqa: E402
import ollama_klient as AI            # noqa: E402
from skaner import Skaner, gb, mb     # noqa: E402
from wykonawca import wykonaj         # noqa: E402


# --------------------------------------------------------------------- wyswietl
def naglowek(t: str):
    print("\n" + "=" * 78)
    print(t)
    print("=" * 78)


def pokaz_plan(plan: dict):
    d = plan["dysk"]
    naglowek("RAPORT DYSKU")
    print(f"  Dysk:     {d['uzyte_GB']} GB uzyte / {d['razem_GB']} GB "
          f"({d['zajete_proc']}%)")
    print(f"  Wolne:    {d['wolne_GB']} GB")
    print(f"  Skan:     {plan['czas_skanu_s']} s, wygenerowano {plan['wygenerowano']}")

    etykieta_ryzyka = {1: "bezpieczne", 2: "odtwarzalne", 3: "dane - decyzja"}
    naglowek("ZNALEZISKA")
    for g in plan["grupy"]:
        print(f"\n  [{g['ryzyko']}] {g['kategoria'].replace('_', ' ').upper()} "
              f"- {g['rozmiar_MB']/1024:.2f} GB, {len(g['pozycje'])} poz. "
              f"({etykieta_ryzyka.get(g['ryzyko'], '?')}, akcja: {g['akcja']})")
        print(f"      {g['opis']}")
        for p in g["pozycje"][:5]:
            wiek = f", {p['wiek_dni']} dni" if p.get("wiek_dni") is not None else ""
            sciezka = p["sciezka"]
            if len(sciezka) > 96:
                sciezka = "..." + sciezka[-93:]
            print(f"        - {sciezka}  ({p['rozmiar_MB']} MB{wiek})")
        if len(g["pozycje"]) > 5:
            print(f"        ... i jeszcze {len(g['pozycje']) - 5} pozycji")

    print(f"\n  SUMA DO ODZYSKANIA (bez danych uzytkownika): "
          f"{plan['suma_do_odzyskania_MB']/1024:.2f} GB")


def pokaz_oceny(oceny: dict):
    if not oceny:
        return
    naglowek("OCENA MODELU (tylko sporne pozycje)")
    for sciezka, o in oceny.items():
        werdykt = {"usun": "USUN", "zostaw": "ZOSTAW", "nie_wiem": "?"}.get(
            o["werdykt"], o["werdykt"])
        print(f"  [{werdykt} {o['pewnosc']}%] {sciezka}")
        if o["powod"]:
            print(f"      {o['powod']}")


# ------------------------------------------------------------------ przywracanie
def przywroc():
    home = Path.home()
    katalog = R.katalog_raportow(home)
    manifesty = sorted(katalog.glob("manifest_*.json"))
    wykonane = [m for m in manifesty
                if json.loads(m.read_text(encoding="utf-8")).get("tryb") == "wykonany"]
    if not wykonane:
        print("Brak manifestu z wykonanej kwarantanny - nie ma czego przywracac.")
        return
    plik = wykonane[-1]
    dane = json.loads(plik.read_text(encoding="utf-8"))
    print(f"Przywracam z: {plik.name} ({len(dane['ruchy'])} pozycji)")
    ok = 0
    for r in dane["ruchy"]:
        z, na = Path(r["na"]), Path(r["z"])
        if not z.exists():
            continue
        try:
            na.parent.mkdir(parents=True, exist_ok=True)
            shutil_move(z, na)
            ok += 1
        except OSError as e:
            print(f"  BLAD {z}: {e}")
    print(f"Przywrocono {ok} pozycji.")


def shutil_move(z: Path, na: Path):
    import shutil
    shutil.move(str(z), str(na))


# ------------------------------------------------------------------ interakcja
def zapytaj_tak(pytanie: str) -> bool:
    try:
        odp = input(f"{pytanie} [t/N]: ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return False
    return odp in ("t", "tak", "y", "yes")


def main():
    ap = argparse.ArgumentParser(description="Agent sprzatajacy dysk (Windows 11)")
    ap.add_argument("--tylko-raport", action="store_true",
                    help="skan i raport, nic nie przenosi")
    ap.add_argument("--bez-ai", action="store_true", help="pomin model lokalny")
    ap.add_argument("--model", default=None, help="nazwa modelu Ollama")
    ap.add_argument("--korzen", action="append", default=None,
                    help="katalog do skanowania (mozna wiele)")
    ap.add_argument("--przywroc", action="store_true",
                    help="cofnij ostatnia kwarantanne")
    ap.add_argument("--auto", action="store_true",
                    help="bez pytania o kazda grupe (tylko z --tylko-raport NIE)")
    a = ap.parse_args()

    if a.przywroc:
        przywroc()
        return

    home = Path.home()
    korzenie = a.korzen or [str(home / n) for n in
                            ("Downloads", "Pobrane", "Documents", "Dokumenty",
                             "Desktop", "Pulpit", "Pictures", "Obrazy", "source",
                             "repos", "Projekty")]
    korzenie = [k for k in korzenie if Path(k).is_dir()] or [str(home)]

    # ---------------------------------------------------------- 1. skan
    print(f"Skanuje: {', '.join(korzenie)}")
    plan = Skaner(korzenie).uruchom()
    pokaz_plan(plan)

    # --------------------------------------------------- 2. ocena modelem
    sporne = []
    for g in plan["grupy"]:
        if g["ryzyko"] >= 3:
            sporne.extend(g["pozycje"][:40])
    if sporne and not a.bez_ai:
        if AI.dostepna():
            model = AI.wybierz_model(a.model)
            if model:
                print(f"\nPytam model lokalny {model} o {len(sporne)} spornych "
                      f"pozycji (na CPU to moze potrwac 1-3 min)...")
                oceny = AI.ocen_sporne(sporne, model)
                pokaz_oceny(oceny)
                for g in plan["grupy"]:
                    for p in g["pozycje"]:
                        o = oceny.get(p["sciezka"].lower())
                        if o:
                            p["ocena_modelu"] = o
            else:
                print("\nOllama dziala, ale nie ma zadnego modelu. "
                      "Uruchom: ollama pull qwen2.5:7b-instruct-q4_K_M")
        else:
            print("\nOllama nie odpowiada - pomijam ocene modelem "
                  "(reguly wystarcza do sprzatania cache i buildow).")

    # ------------------------------------------------------ 3. zapis planu
    katalog = R.katalog_raportow(home)
    katalog.mkdir(parents=True, exist_ok=True)
    znacznik = dt.datetime.now().strftime("%Y-%m-%d_%H%M%S")
    plik_planu = katalog / f"plan_{znacznik}.json"
    plik_planu.write_text(json.dumps(plan, ensure_ascii=False, indent=2),
                          encoding="utf-8")

    # -------------------------------------------------- 4. decyzja i ruch
    do_przeniesienia = [g for g in plan["grupy"] if g["akcja"] == "kwarantanna"]
    if not do_przeniesienia:
        print("\nNic do przeniesienia. Dysk jest w porzadku.")
        print(f"Raport: {plik_planu}")
        return

    naglowek("DO PRZENIESIENIA DO KWARANTANNY")
    for i, g in enumerate(do_przeniesienia, 1):
        print(f"  {i}. {g['kategoria']:<20} {g['rozmiar_MB']/1024:>6.2f} GB "
              f"({len(g['pozycje'])} poz.)")
    print(f"     {'RAZEM':<20} "
          f"{sum(g['rozmiar_MB'] for g in do_przeniesienia)/1024:>6.2f} GB")
    print(f"\n  Kwarantanna: {R.katalog_kwarantanny(home)}")
    print("  Pliki da sie przywrocic: python agent.py --przywroc")

    if a.tylko_raport:
        print(f"\nTryb --tylko-raport: nic nie ruszam. Plan: {plik_planu}")
        return

    # ------------------------------------------------- wybor kategorii
    # Domyslnie proponujemy wszystko, ale uzytkownik moze wykluczyc kategorie
    # numerem. "v" = pokaz pelna liste pozycji wybranej grupy (do przegladu).
    wybrane = [g["kategoria"] for g in do_przeniesienia]
    if not a.auto:
        print("\n  Aby WYKLUCZYC kategorie, podaj jej numery (np. 2 3).")
        print("  Enter = wszystko. 'v 2' = pokaz pelna liste pozycji z grupy 2.")
        try:
            odp = input("  Twoj wybor: ").strip().lower()
        except (EOFError, KeyboardInterrupt):
            odp = ""

        if odp.startswith("v"):
            try:
                nr = int(odp.split()[1]) - 1
                g = do_przeniesienia[nr]
            except (ValueError, IndexError):
                print("  Zly numer grupy.")
                return
            naglowek(f"PELNA LISTA: {g['kategoria']} ({len(g['pozycje'])} poz.)")
            for p in g["pozycje"]:
                wiek = f"{p['wiek_dni']} dni" if p.get("wiek_dni") is not None else "?"
                print(f"  {p['rozmiar_MB']:>8.1f} MB  {wiek:>8}  {p['sciezka']}")
            print(f"\n  Nic nie przeniesione. Aby przeniesc, uruchom agenta ponownie.")
            return

        if odp:
            try:
                numery = [int(x) for x in odp.replace(",", " ").split()]
            except ValueError:
                print("  Rozumiem tylko numery grup. Anulowane.")
                return
            wykluczone = {do_przeniesienia[n - 1]["kategoria"]
                          for n in numery if 0 < n <= len(do_przeniesienia)}
            wybrane = [k for k in wybrane if k not in wykluczone]
            if wykluczone:
                print(f"\n  Wykluczone: {', '.join(sorted(wykluczone))}")
            if not wybrane:
                print("  Nic nie zostalo do przeniesienia. Anulowane.")
                return

        suma = sum(g["rozmiar_MB"] for g in do_przeniesienia
                   if g["kategoria"] in wybrane) / 1024
        if not zapytaj_tak(f"\nPrzeniesc {suma:.2f} GB do kwarantanny?"):
            print(f"Anulowane. Nic nie zostalo zmienione. Plan: {plik_planu}")
            return

    wynik = wykonaj(plan, probne=False, potwierdzone_kategorie=wybrane)
    naglowek("WYNIK")
    print(f"  Przeniesiono:   {len(wynik['ruchy'])} pozycji")
    print(f"  Odzyskane:      {wynik['odzyskane_MB']/1024:.2f} GB")
    print(f"  Bledy:          {len(wynik['bledy'])}")
    for b in wynik["bledy"][:10]:
        print(f"    - {b['sciezka']}: {b['blad']}")
    print(f"\n  Manifest:    {wynik['manifest']}")
    print(f"  Kwarantanna: {wynik['kwarantanna']}")
    print("\n  Kwarantanne oproznij po tygodniu, jesli nic nie zginelo:")
    print(f"    Remove-Item -Recurse -Force \"{wynik['kwarantanna']}\"")
    print("  Przywrocenie: python agent.py --przywroc")


if __name__ == "__main__":
    main()
