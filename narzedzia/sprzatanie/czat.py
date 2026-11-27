# -*- coding: utf-8 -*-
"""
Czat z agentem w konsoli - calkowicie offline, bez Telegrama, bez chmury.

Uzycie:
    python czat.py                 # model domyslny (qwen2.5:7b)
    python czat.py --model qwen2.5:3b-instruct-q4_K_M   # szybciej
    python czat.py --potwierdzaj   # kazda zmiana na dysku wymaga "tak"

Jak to dziala:
  1. Ty piszesz pytanie po polsku.
  2. Model decyduje, czy potrzebuje narzedzia (np. skanuj_dysk).
  3. Kod wykonuje narzedzie i oddaje wynik modelowi.
  4. Model odpowiada zdaniem.

Model NIE MA dostepu do dysku. Jedyne, co moze, to poprosic o wykonanie
funkcji z narzedzia_czatu.py - a kod sprawdza argumenty i wykonuje.
Kategoria "duplikaty" i "duze pliki" sa zablokowane dla czatu: przy 305
zdjeciach decyzja musi byc Twoja, nie bota.
"""

import argparse
import sys
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ollama_klient as AI        # noqa: E402
import reguly as R                # noqa: E402
from narzedzia_czatu import Narzedzia, OPIS_NARZEDZI  # noqa: E402

SYSTEM = """Jestes lokalnym asystentem od porzadkow na dysku Windows 11.
Mowisz po polsku, krotko i konkretnie. Uzytkownik ma 16 GB RAM i CPU bez GPU,
wiec nie rozwlekaj sie.

Zasady:
- Zanim powiesz, ile miejsca zajmuja smieci, UZYJ narzedzia skanuj_dysk.
- Nigdy nie twierdz, ze cos przeniosles, jesli narzedzie tego nie potwierdzilo.
- Przed przenoszeniem powiedz, co i ile, i popros o zgode uzytkownika.
- Kategorii "duplikaty" i "duze_pliki" nie przenosisz - one wymagaja recznej decyzji.
- Jesli nie wiesz - powiedz "nie wiem", nie zgaduj."""

POZEGNANIA = {"exit", "quit", "koniec", "wyjdz", "q"}


def main():
    ap = argparse.ArgumentParser(description="Czat z lokalnym agentem")
    ap.add_argument("--model", default=None)
    ap.add_argument("--potwierdzaj", action="store_true",
                    help="kazda zmiana na dysku wymaga wpisania 'tak'")
    a = ap.parse_args()

    if not AI.dostepna():
        print("Ollama nie odpowiada. Uruchom w drugim oknie: ollama serve")
        print("Albo zainstaluj model:  ollama pull qwen2.5:7b-instruct-q4_K_M")
        return 1

    model = AI.wybierz_model(a.model)
    if not model:
        print("Brak modeli. Pobierz:  ollama pull qwen2.5:7b-instruct-q4_K_M")
        return 1

    narz = Narzedzia()
    wiadomosci = [{"role": "system", "content": SYSTEM}]

    print("=" * 66)
    print(f"Czat lokalny - model: {model}")
    print("Wszystko zostaje na tym laptopie. 'exit' konczy.")
    print("=" * 66)
    print("\nPrzyklady: 'ile mam wolnego?', 'co zajmuje miejsce?',")
    print("           'posprzataj cache', 'ile jest w kwarantannie?'\n")

    while True:
        try:
            pytanie = input("Ty > ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not pytanie:
            continue
        if pytanie.lower() in POZEGNANIA:
            break

        wiadomosci.append({"role": "user", "content": pytanie})

        # maks. 3 obroty narzedzi na jedno pytanie - zabezpieczenie przed petla
        for proba in range(3):
            odp = AI.czat(wiadomosci, model, narzedzia=OPIS_NARZEDZI)
            if odp is None:
                print("(model nie odpowiedzial - sprawdz 'ollama serve')\n")
                break

            if not odp["wywolania"]:
                if odp["tresc"]:
                    print(f"\nAgent > {odp['tresc']}\n")
                    wiadomosci.append({"role": "assistant", "content": odp["tresc"]})
                break

            # model poprosil o narzedzie - wykonujemy i oddajemy wynik
            for w in odp["wywolania"]:
                nazwa = w["nazwa"]
                argumenty = w["argumenty"]
                print(f"  [uzywam: {nazwa}]", flush=True)

                if nazwa == "przenies_do_kwarantanny" and a.potwierdzaj:
                    try:
                        zgoda = input("  Przeniesc pliki do kwarantanny? [tak/N]: ")
                    except (EOFError, KeyboardInterrupt):
                        zgoda = ""
                    if zgoda.strip().lower() not in ("tak", "t", "y", "yes"):
                        wynik = {"anulowane": "uzytkownik nie wyrazil zgody"}
                        print("  anulowane\n")
                    else:
                        wynik = narz.wywolaj(nazwa, argumenty)
                else:
                    wynik = narz.wywolaj(nazwa, argumenty)
                    wynik["_zatwierdzone_przez"] = "uzytkownik w czacie"

                wiadomosci.append({
                    "role": "tool",
                    "content": str(wynik)[:2000],
                    "name": nazwa,
                })

            # po wyniku narzedzi model musi dostac szanse odpowiedziec
            odp = AI.czat(wiadomosci, model, narzedzia=OPIS_NARZEDZI)
            if odp and odp["tresc"]:
                print(f"\nAgent > {odp['tresc']}\n")
                wiadomosci.append({"role": "assistant", "content": odp["tresc"]})
            break

        # historia: system + ostatnie 6 tur (dluzej = wolno na CPU)
        if len(wiadomosci) > 13:
            wiadomosci = [wiadomosci[0]] + wiadomosci[-12:]

    print("Do zobaczenia.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
