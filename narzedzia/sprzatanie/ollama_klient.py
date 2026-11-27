# -*- coding: utf-8 -*-
"""
Ollama - cienki klient HTTP na bibliotece standardowej (bez pip install).

Jesli Ollama nie dziala, kazda funkcja zwraca None i program dziala dalej
na regulach. To jest celowe: sprzatanie nie moze zalezec od tego, czy model
wstal.
"""

import json
import urllib.error
import urllib.request

import reguly as R


def _post(sciezka: str, dane: dict, timeout: int = 300):
    req = urllib.request.Request(
        R.OLLAMA_URL + sciezka,
        data=json.dumps(dane).encode("utf-8"),
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def _get(sciezka: str, timeout: int = 5):
    with urllib.request.urlopen(R.OLLAMA_URL + sciezka, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8"))


def dostepna() -> bool:
    try:
        _get("/api/tags")
        return True
    except (urllib.error.URLError, OSError, ValueError):
        return False


def modele() -> list[str]:
    try:
        return [m["name"] for m in _get("/api/tags").get("models", [])]
    except (urllib.error.URLError, OSError, ValueError, KeyError):
        return []


def wybierz_model(preferowany: str | None = None) -> str | None:
    """Preferowany -> domyslny -> pierwszy dostepny qwen/llama."""
    dostepne = modele()
    if not dostepne:
        return None
    if preferowany:
        for m in dostepne:
            if m == preferowany or m.split(":")[0] == preferowany.split(":")[0]:
                return m
    for kandydat in (R.MODEL_DOMYSLNY, R.MODEL_SZYBKI):
        for m in dostepne:
            if m.split(":")[0] == kandydat.split(":")[0]:
                return m
    for m in dostepne:
        if any(k in m.lower() for k in ("qwen", "llama", "mistral", "gemma", "phi")):
            return m
    return dostepne[0]


def generuj(prompt: str, model: str, system: str | None = None,
            temperatura: float = 0.1, timeout: int = 300) -> str | None:
    dane = {
        "model": model,
        "prompt": prompt,
        "stream": False,
        "options": {"temperature": temperatura, "num_ctx": 8192,
                    "num_thread": 8},   # 4750U = 8 watkow
    }
    if system:
        dane["system"] = system
    try:
        return _post("/api/generate", dane, timeout=timeout).get("response", "")
    except (urllib.error.URLError, OSError, ValueError, KeyError):
        return None


SYSTEM_OCENA = """Jestes asystentem sprzatajacym dysk na Windows 11.
Dostajesz ZWYKLA LISTE plikow i katalogow z rozmiarem i wiekiem.
Twoje zadanie: wskazac, ktore pozycje sa zbedne, i uzasadnic w jednym zdaniu.

Zasady, ktorych nie wolno lamac:
- Nigdy nie proponuj usuniecia .git, OneDrive, Google Drive, Dropbox, Program Files.
- Dokumenty uzytkownika (.docx, .pdf, .xlsx, .jpg rodzinne) sa WARTOSCIOWE.
- Plik, ktorego nie rozumiesz, zostaw i napisz "nie wiem".
- Nie zgaduj zawartosci pliku po nazwie, jesli nazwa jest niejednoznaczna.

Odpowiedz TYLKO w formacie JSON, bez komentarza:
{"oceny":[{"sciezka":"...","werdykt":"usun|zostaw|nie_wiem","pewnosc":0-100,"powod":"..."}]}
"""


def ocen_sporne(sporne: list[dict], model: str) -> dict[str, dict]:
    """Pyta model o pozycje o ryzyku 3 (dane uzytkownika). Zwraca mape sciezka->ocena."""
    if not sporne:
        return {}

    # Model dostaje minimum metadanych - nie widzi zawartosci plikow.
    linie = []
    for p in sporne[:80]:      # limit kontekstu i czasu na CPU
        linie.append(
            f"- {p['sciezka']} | {p['rozmiar_MB']} MB | "
            f"wiek {p['wiek_dni']} dni | {p.get('powod', '')}"
        )
    prompt = ("Lista pozycji na dysku. Ocen kazda po kolei.\n\n" + "\n".join(linie))

    odp = generuj(prompt, model, system=SYSTEM_OCENA)
    if not odp:
        return {}

    start, koniec = odp.find("{"), odp.rfind("}")
    if start < 0 or koniec <= start:
        return {}
    try:
        dane = json.loads(odp[start:koniec + 1])
    except ValueError:
        return {}

    wynik = {}
    for o in dane.get("oceny", []):
        sciezka = str(o.get("sciezka", ""))
        if sciezka:
            wynik[sciezka.lower()] = {
                "werdykt": str(o.get("werdykt", "nie_wiem")),
                "pewnosc": int(o.get("pewnosc", 0) or 0),
                "powod": str(o.get("powod", "")),
            }
    return wynik


# ------------------------------------------------------------------ czat
# Ollama ma osobny endpoint /api/chat, ktory przyjmuje historie rozmowy
# w formacie OpenAI: [{"role": "user"|"assistant"|"system", "content": "..."}].
# Dla 7B na CPU trzymamy historie krotka (6 tur) - inaczej kazde pytanie
# przetwarza od nowa coraz dluzszy prompt i robi sie wolno.

def czat(wiadomosci: list[dict], model: str, narzedzia: list | None = None,
         temperatura: float = 0.2, timeout: int = 300) -> dict | None:
    """Jedna tura rozmowy. Zwraca {"tresc": str, "wywolania": [...]} albo None.

    wywolania to lista {"nazwa": ..., "argumenty": {...}} - decyzje modelu,
    ktore wykonuje kod, nie model. Model nigdy nie dotyka systemu plikow.
    """
    dane = {
        "model": model,
        "messages": wiadomosci,
        "stream": False,
        "options": {"temperature": temperatura, "num_ctx": 4096, "num_thread": 8},
    }
    if narzedzia:
        dane["tools"] = narzedzia

    try:
        odp = _post("/api/chat", dane, timeout=timeout)
    except (urllib.error.URLError, OSError, ValueError):
        return None
    if not odp:
        return None

    wiadomosc = (odp.get("message") or {})
    wywolania = []
    for w in (wiadomosc.get("tool_calls") or []):
        funkcja = w.get("function") or {}
        nazwa = funkcja.get("name")
        argumenty = funkcja.get("arguments")
        if isinstance(argumenty, str):      # czesc modeli zwraca JSON jako tekst
            try:
                argumenty = json.loads(argumenty)
            except ValueError:
                argumenty = {}
        if nazwa:
            wywolania.append({"nazwa": nazwa, "argumenty": argumenty or {}})

    return {"tresc": (wiadomosc.get("content") or "").strip(),
            "wywolania": wywolania}
