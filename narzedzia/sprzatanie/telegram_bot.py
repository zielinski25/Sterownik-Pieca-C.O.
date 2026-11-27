# -*- coding: utf-8 -*-
"""
Bot Telegram do rozmowy z agentem - WOLNY, ale dziala z telefonu.

===============================================================================
  PRZECZYTAJ TO PRZED URUCHOMIENIEM
===============================================================================
  Ten bot LAMIE zasade "wszystko zostaje na laptopie":

    Telegram (serwer w chmurze)  ->  Twoj laptop  ->  odpowiedz

  Kazda wiadomosc, ktora wyslesz, przechodzi przez serwery Telegramu.
  Gdy bot odpowie "masz 2 GB cache w Chrome", ta informacja tez tamtedy
  przechodzi. Nie sa to Twoje pliki, ale sa to nazwy folderow i rozmiary.

  Jesli to Ci przeszkadza: uzyj czat.py (konsola) - dziala w 100% offline.

  Zabezpieczenia w tym pliku:
    - bot odpowiada TYLKO na Twoj chat_id (lista w --dozwoleni)
    - przenoszenie plikow wymaga wpisania "tak" w Telegramie
    - kategorie "duplikaty" i "duze pliki" sa zablokowane
    - watki: pytania obsluguje jeden watek, zeby nie zjadly 16 GB RAM

===============================================================================
"""

import argparse
import json
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ollama_klient as AI                         # noqa: E402
from narzedzia_czatu import Narzedzia, OPIS_NARZEDZI  # noqa: E402
from czat import SYSTEM                            # noqa: E402

API = "https://api.telegram.org/bot{token}/{metoda}"


# ------------------------------------------------------------------ telegram
def tg(token: str, metoda: str, **params):
    dane = urllib.parse.urlencode(params).encode("utf-8")
    req = urllib.request.Request(API.format(token=token, metoda=metoda), data=dane)
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            return json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        print(f"  [telegram] blad {e.code}: {e.read()[:200]}")
        return {"ok": False}
    except (urllib.error.URLError, OSError) as e:
        print(f"  [telegram] brak polaczenia: {e}")
        return {"ok": False}


def wyslij(token: str, chat_id: int, tekst: str):
    # Telegram tnie wiadomosci na 4096 znakach - dzielimy przez bezpieczenstwo
    for i in range(0, len(tekst), 4000):
        tg(token, "sendMessage", chat_id=chat_id, text=tekst[i:i + 4000])


# --------------------------------------------------------------------- agent
def odpowiedz(model: str, narz: Narzedzia, tekst: str, czeka_na_zgode: dict) -> str:
    """Jedno pytanie -> odpowiedz. Uzywa tej samej logiki co czat.py."""
    wiadomosci = [{"role": "system", "content": SYSTEM},
                  {"role": "user", "content": tekst}]

    for _ in range(3):
        odp = AI.czat(wiadomosci, model, narzedzia=OPIS_NARZEDZI)
        if odp is None:
            return "Model nie odpowiada. Sprawdz, czy dziala 'ollama serve'."
        if not odp["wywolania"]:
            return odp["tresc"] or "(brak odpowiedzi)"

        for w in odp["wywolania"]:
            nazwa, argumenty = w["nazwa"], w["argumenty"]

            # przenoszenie: zapamietujemy zadanie i pytamy uzytkownika
            if nazwa == "przenies_do_kwarantanny":
                narz.skanuj_dysk()          # zbierz plan w tle
                czeka_na_zgode.clear()
                czeka_na_zgode.update({"nazwa": nazwa, "argumenty": argumenty})
                return ("Chce przeniesc do kwarantanny: cache, buildy i stare instalki.\n"
                        "Nic nie zniknie - pliki beda do przywrocenia.\n\n"
                        "Wpisz TAK, zeby przeniesc, albo cokolwiek innego, zeby anulowac.")

            wynik = narz.wywolaj(nazwa, argumenty)
            wiadomosci.append({"role": "tool", "name": nazwa,
                               "content": str(wynik)[:2000]})

        odp = AI.czat(wiadomosci, model, narzedzia=OPIS_NARZEDZI)
        if odp and odp["tresc"]:
            return odp["tresc"]
    return "(nie udalo sie dokonczyc)"


def main():
    ap = argparse.ArgumentParser(description="Bot Telegram dla agenta sprzatajacego")
    ap.add_argument("--token", required=True,
                    help="token bota od @BotFather")
    ap.add_argument("--dozwoleni", required=True,
                    help="Twoj chat_id (liczby, mozna kilka po przecinku)")
    ap.add_argument("--model", default=None)
    a = ap.parse_args()

    dozwoleni = {int(x) for x in a.dozwoleni.replace(" ", "").split(",") if x}

    if not AI.dostepna():
        print("Ollama nie odpowiada. Uruchom 'ollama serve' i sprobuj ponownie.")
        return 1
    model = AI.wybierz_model(a.model)
    if not model:
        print("Brak modeli w Ollama. Pobierz model i sprobuj ponownie.")
        return 1

    print("=" * 62)
    print(f"Bot dziala. Model: {model}")
    print(f"Dozwolone chat_id: {sorted(dozwoleni)}")
    print("Pisz do bota w Telegramie. Ctrl+C konczy.")
    print("=" * 62)
    print("\nUWAGA: wiadomosci przechodza przez serwery Telegramu.")

    narz = Narzedzia()
    offset = None
    czeka_na_zgode: dict = {}

    while True:
        try:
            params = {"timeout": 50}
            if offset:
                params["offset"] = offset
            odp = tg(a.token, "getUpdates", **params)
        except KeyboardInterrupt:
            print("\nKoniec.")
            break

        if not odp.get("ok"):
            time.sleep(3)
            continue

        for upd in odp.get("result", []):
            offset = upd["update_id"] + 1
            wiad = upd.get("message") or {}
            chat_id = (wiad.get("chat") or {}).get("id")
            tekst = (wiad.get("text") or "").strip()
            if not chat_id or not tekst:
                continue

            if chat_id not in dozwoleni:
                wyslij(a.token, chat_id,
                       "Ten bot jest prywatny. Twoj chat_id to "
                       f"{chat_id} - dopisz go do --dozwoleni, jesli to Ty.")
                continue

            print(f"  [{chat_id}] {tekst}")

            # czy to odpowiedz na prosbe o zgode?
            if czeka_na_zgode:
                if tekst.strip().lower() in ("tak", "t", "yes", "y"):
                    wynik = narz.przenies_do_kwarantanny(
                        czeka_na_zgode["argumenty"].get("kategorie"),
                        zatwierdzone=True)
                    czeka_na_zgode.clear()
                    if "blad" in wynik:
                        wyslij(a.token, chat_id, f"Nie udalo sie: {wynik['blad']}")
                    else:
                        wyslij(a.token, chat_id,
                               f"Gotowe. Przeniesiono {wynik['GB']} GB "
                               f"({wynik['pozycji']} pozycji).\n"
                               f"Pliki sa w kwarantannie, da sie przywrocic.\n"
                               f"Pominietych plikow: {wynik['pominietych_plikow']} "
                               f"(w uzyciu).")
                else:
                    czeka_na_zgode.clear()
                    wyslij(a.token, chat_id, "Anulowane. Nic nie zmienione.")
                continue

            try:
                odpowiedz_txt = odpowiedz(model, narz, tekst, czeka_na_zgode)
            except Exception as e:              # bot nie moze sie wywrocic
                odpowiedz_txt = f"Blad: {type(e).__name__}: {e}"
            wyslij(a.token, chat_id, odpowiedz_txt)

    return 0


if __name__ == "__main__":
    sys.exit(main())
