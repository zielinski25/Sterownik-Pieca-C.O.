# -*- coding: utf-8 -*-
"""
Narzedzia dla czatu - JEDYNE miejsce, w ktorym model moze wplynac na swiat.

Model nie ma dostepu do dysku. Moze tylko poprosic o wykonanie jednej z tych
funkcji, a kod sprawdza argumenty, wykonuje i zwraca wynik. Jesli model
wymysli nazwe albo poda sciezke spoza listy dozwolonych - odmawiamy.

Zasada: model proponuje, kod decyduje.
"""

import json
from pathlib import Path

import reguly as R
from skaner import Skaner, chroniony, gb

# Kategorie, ktore wolno przenosic z czatu. Duplikaty i duze pliki CELOWO
# poza lista - przy 305 zdjeciach decyzja musi byc reczna, nie "bot mi kazal".
KATEGORIE_CZAT = {
    "cache_i_temp", "katalogi_builda", "stare_instalki", "stare_logi",
}

OPIS_NARZEDZI = [
    {
        "type": "function",
        "function": {
            "name": "skanuj_dysk",
            "description": "Skanuje dysk i zwraca liste znalezisk z rozmiarami. "
                           "Nic nie zmienia. Uzywaj, gdy uzytkownik pyta co zajmuje miejsce.",
            "parameters": {
                "type": "object",
                "properties": {
                    "katalog": {
                        "type": "string",
                        "description": "Katalog do skanowania. Puste = domyslne foldery uzytkownika.",
                    }
                },
                "required": [],
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "ile_wolnego",
            "description": "Zwraca ile miejsca jest zajete i wolne na dysku C.",
            "parameters": {"type": "object", "properties": {}, "required": []},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "kwarantanna_info",
            "description": "Mowi, ile GB lezy w kwarantannie (do zwolnienia po weryfikacji).",
            "parameters": {"type": "object", "properties": {}, "required": []},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "przenies_do_kwarantanny",
            "description": "Przenosi WYLACZNIE cache, buildy, stare instalki i logi "
                           "do kwarantanny. NIE dotyka duplikatow ani duzych plikow. "
                           "Pliki da sie przywrocic.",
            "parameters": {
                "type": "object",
                "properties": {
                    "kategorie": {
                        "type": "array",
                        "items": {"type": "string"},
                        "description": "Ktore kategorie przeniesc. Puste = wszystkie dozwolone.",
                    }
                },
                "required": [],
            },
        },
    },
]


class Narzedzia:
    """Wykonuje polecenia modelu. Kazda metoda zwraca dict z wynikiem."""

    def __init__(self, home: Path | None = None):
        self.home = Path(home or Path.home())
        self.ostatni_plan: dict | None = None

    # ------------------------------------------------------------- odczyt
    def ile_wolnego(self) -> dict:
        import shutil
        d = shutil.disk_usage(self.home)
        return {
            "wolne_GB": round(d.free / 1024 ** 3, 2),
            "zajete_GB": round(d.used / 1024 ** 3, 2),
            "razem_GB": round(d.total / 1024 ** 3, 2),
            "procent_zajete": round(100 * d.used / d.total, 1),
        }

    def kwarantanna_info(self) -> dict:
        kwar = R.katalog_kwarantanny(self.home)
        if not kwar.exists():
            return {"GB": 0.0, "pozycji": 0}
        suma = 0
        ile = 0
        for p in kwar.rglob("*"):
            if p.is_file():
                try:
                    suma += p.stat().st_size
                    ile += 1
                except OSError:
                    pass
        return {"GB": round(suma / 1024 ** 3, 2), "pozycji": ile}

    def skanuj_dysk(self, katalog: str = "") -> dict:
        korzenie = []
        if katalog:
            p = Path(katalog).expanduser()
            if not p.is_dir() or chroniony(p):
                return {"blad": f"nie skanuje tego katalogu: {katalog}"}
            korzenie = [str(p)]
        else:
            korzenie = [str(self.home / n) for n in
                        ("Downloads", "Pobrane", "Documents", "Dokumenty",
                         "Desktop", "Pulpit", "Pictures", "Obrazy")]
            korzenie = [k for k in korzenie if Path(k).is_dir()]
        if not korzenie:
            # profil bez typowych folderow - skanujemy caly domowy, zeby czat
            # nie stanąl na "brak katalogow" zamiast cokolwiek powiedziec
            korzenie = [str(self.home)]

        plan = Skaner(korzenie).uruchom()
        self.ostatni_plan = plan
        grupy = [{"kategoria": g["kategoria"],
                  "GB": round(g["rozmiar_MB"] / 1024, 2),
                  "pozycji": len(g["pozycje"]),
                  "ryzyko": g["ryzyko"]}
                 for g in plan["grupy"]]
        return {"dysk": plan["dysk"], "grupy": grupy,
                "do_odzyskania_GB": round(plan["suma_do_odzyskania_MB"] / 1024, 2)}

    # -------------------------------------------------------------- zapis
    def przenies_do_kwarantanny(self, kategorie: list[str] | None = None,
                                zatwierdzone: bool = False) -> dict:
        """Bez zatwierdzone=True NIC nie rusza - tylko zwraca, co by zrobilo."""
        if not zatwierdzone:
            return {"wymaga_zatwierdzenia": True,
                    "komunikat": "Powiedz uzytkownikowi, co chcesz przeniesc, "
                                 "i poczekaj na jego zgode."}
        if self.ostatni_plan is None:
            self.skanuj_dysk()
        if not self.ostatni_plan:
            return {"blad": "brak planu - najpierw skanuj"}

        zadane = set(kategorie or KATEGORIE_CZAT)
        dozwolone = zadane & KATEGORIE_CZAT
        odrzucone = zadane - KATEGORIE_CZAT
        if not dozwolone:
            return {"blad": f"tych kategorii nie przenosze z czatu: "
                            f"{', '.join(sorted(odrzucone))}"}

        from wykonawca import wykonaj
        wynik = wykonaj(self.ostatni_plan, probne=False,
                        potwierdzone_kategorie=dozwolone)
        self.ostatni_plan = None
        return {
            "GB": round(wynik["odzyskane_MB"] / 1024, 2),
            "pozycji": len(wynik["ruchy"]),
            "pominietych_plikow": len(wynik.get("pominiete", [])),
            "kwarantanna": wynik["kwarantanna"],
            "odrzucone_kategorie": sorted(odrzucone),
        }

    # ------------------------------------------------------- dyspozytor
    def wywolaj(self, nazwa: str, argumenty: dict) -> dict:
        if nazwa == "ile_wolnego":
            return self.ile_wolnego()
        if nazwa == "kwarantanna_info":
            return self.kwarantanna_info()
        if nazwa == "skanuj_dysk":
            return self.skanuj_dysk(str(argumenty.get("katalog", "")))
        if nazwa == "przenies_do_kwarantanny":
            kat = argumenty.get("kategorie")
            if isinstance(kat, str):
                kat = [kat]
            return self.przenies_do_kwarantanny(kat)
        return {"blad": f"nieznane narzedzie: {nazwa}"}
