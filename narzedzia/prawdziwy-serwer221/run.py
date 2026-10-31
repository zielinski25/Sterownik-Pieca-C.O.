#!/usr/bin/env python3
# [ETAP 221] Brama: prawdziwy serwer HTTP biblioteki na komputerze.
#
# Co robi:
#   1. sprawdza, ze wyciagniety kod to DOKLADNIE tekst z v3.56.4 (zrob_ekstrakcje.py),
#   2. sprawdza SHA naszych naglowkow wobec zrodel w repo (nic nie podmienione),
#   3. kompiluje PRAWDZIWE zrodla biblioteki (WebServer/WebRequest/WebHandlers/
#      WebResponses + naglowki) i nasz kod z v3.56.4 jednym kompilatorem g++,
#   4. uruchamia zadania HTTP przez prawdziwy parser i sprawdza odpowiedzi,
#   5. dla kazdego mutanta: psuje jedno miejsce i sprawdza, ze test siadzie.
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
BIB = os.path.join(TU, "biblioteka")
PLAT = os.path.join(TU, "platforma")
NASZ = os.path.join(TU, "nasz")

INCLUDE = [
    "-I", PLAT,
    "-I", os.path.join(BIB, "espasyncwebserver", "src"),
    "-I", os.path.join(BIB, "arduino-core-api", "api"),
    "-I", os.path.join(BIB, "esp32-core"),
    "-I", os.path.join(BIB, "libb64", "include"),
    "-I", NASZ,
]

BIBLIOTEKA_CPP = ["WebServer", "WebRequest", "WebHandlers", "WebResponses", "AsyncWebHeader", "Middleware", "AsyncWebServerRequest"]
API_CPP = ["String", "Print", "Stream", "IPAddress", "Common"]
PLATFORMA_CPP = ["Platform", "AsyncTCP", "FS", "AuthZaslepki"]

# Naglowki, ktore test kompiluje: skad pochodza i jaki maja miec SHA.
NAGLOWKI = {
    "HistoriaPobranie.h": os.path.join(REPO, "narzedzia", "od-przycisku216", "HistoriaPobranie.h"),
    "HistoriaPin.h": os.path.join(REPO, "narzedzia", "wysylka-zapinka211", "HistoriaPin.h"),
    "HistoriaBilety.h": os.path.join(REPO, "narzedzia", "panel-pobrania213", "HistoriaBilety.h"),
    "CheckedHistoryIO.h": os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie", "CheckedHistoryIO.h"),
}


def uruchom(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)


def sha256(sciezka):
    import hashlib
    with open(sciezka, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def kompiluj_zrodla(budowa):
    """Prawdziwe zrodla biblioteki + platforma. Raz na caly przebieg."""
    zadania = []
    for nazwa in BIBLIOTEKA_CPP:
        zadania.append((f"bib_{nazwa}.o", os.path.join(BIB, "espasyncwebserver", "src", nazwa + ".cpp"), INCLUDE))
    for nazwa in API_CPP:
        zadania.append((f"api_{nazwa}.o", os.path.join(BIB, "arduino-core-api", "api", nazwa + ".cpp"), INCLUDE))
    for nazwa in PLATFORMA_CPP:
        zadania.append((f"plat_{nazwa}.o", os.path.join(PLAT, nazwa + ".cpp"), INCLUDE))
    obiekty = []
    for cel, zrodlo, include in zadania:
        wyjscie = os.path.join(budowa, cel)
        r = uruchom(["g++", "-std=gnu++17", "-DHOST", "-O1", "-w", "-c", "-o", wyjscie, zrodlo] + include)
        if r.returncode != 0:
            raise SystemExit(f"BLAD kompilacji {zrodlo}:\n{r.stdout.decode('utf-8', 'replace')[:4000]}")
        obiekty.append(wyjscie)
    return obiekty


def zbuduj_test(katalog_testu, budowa, obiekty, binarka):
    """Test.cpp z danego katalogu (repo albo kopia mutanta) + linkowanie."""
    obj = os.path.join(budowa, "Test_%s.o" % os.path.basename(katalog_testu))
    # Katalog testu (dla mutanta: jego kopii) MUSI byc pierwszy - inaczej
    # kompilator wzialby naglowki z repo i mutant nie mialby zadnego efektu.
    include = ["-I", os.path.join(katalog_testu, "nasz")] + INCLUDE
    r = uruchom(["g++", "-std=gnu++17", "-DHOST", "-O1", "-w", "-c", "-o", obj,
                 os.path.join(katalog_testu, "Test.cpp")] + include)
    if r.returncode != 0:
        return None, r.stdout.decode("utf-8", "replace")
    r = uruchom(["g++", "-std=gnu++17", "-DHOST", "-o", binarka, obj] + obiekty)
    if r.returncode != 0:
        return None, r.stdout.decode("utf-8", "replace")
    return binarka, ""


def zrob_dysk(katalog):
    """'Flash' urzadzenia: katalog z dwoma plikami historii (prawdziwe pliki)."""
    os.makedirs(os.path.join(katalog, "littlefs"), exist_ok=True)
    krotki = "czas;temp_ogrz;temp_bojler\n2026-10-28 10:00;52.5;47.0\n2026-10-28 10:08;52.9;47.4\n2026-10-28 10:16;53.4;47.9\n"
    dlugi = "czas;temp_ogrz;temp_bojler\n2026-09-01 00:00;40.1;35.2\n2026-09-01 00:08;40.4;35.5\n2026-09-01 00:16;40.2;35.1\n2026-09-01 00:24;39.8;34.9\n"
    with open(os.path.join(katalog, "littlefs", "hist_short.csv"), "w", encoding="utf-8") as f:
        f.write(krotki)
    with open(os.path.join(katalog, "littlefs", "hist_long.csv"), "w", encoding="utf-8") as f:
        f.write(dlugi)
    return katalog


def uruchom_profil(binarka, dysk, token):
    srodowisko = dict(os.environ)
    srodowisko["HIST_ROOT"] = dysk
    if token is None:
        srodowisko.pop("ETAP221_TOKEN", None)
    else:
        srodowisko["ETAP221_TOKEN"] = token
    r = uruchom([binarka], env=srodowisko, timeout=300)
    tekst = r.stdout.decode("utf-8", "replace")
    kontrole = {}
    for linia in tekst.splitlines():
        if linia.startswith("KONTROLA "):
            czesci = linia.split()
            kontrole[czesci[1]] = (czesci[2] == "PASS", " ".join(czesci[3:]))
    podsumowanie = [l for l in tekst.splitlines() if l.startswith("PODSUMOWANIE")]
    return {"rc": r.returncode, "kontrole": kontrole, "podsumowanie": podsumowanie[-1] if podsumowanie else "", "wyjscie": tekst}


def main():
    start = time.time()
    wynik = {"etap": 221, "kiedy": time.strftime("%Y-%m-%d %H:%M:%S"), "kroki": [], "mutanty": [], "bledy": []}

    # --- 1. ekstrakcja: czy testujemy dokladnie to, co jest w v3.56.4? ---
    r = uruchom([sys.executable, "-B", os.path.join(TU, "zrob_ekstrakcje.py")])
    wynik["kroki"].append({"krok": "ekstrakcja", "rc": r.returncode, "opis": r.stdout.decode("utf-8", "replace").strip()})
    if r.returncode != 0:
        print(r.stdout.decode("utf-8", "replace"))
        return 1

    # --- 2. nasze naglowki: te same bajty co w repo ---
    for nazwa, zrodlo in NAGLOWKI.items():
        kopia = os.path.join(NASZ, nazwa)
        if sha256(kopia) != sha256(zrodlo):
            wynik["bledy"].append(f"{nazwa}: kopia w tescie rozni sie od zrodla {zrodlo}")
    if wynik["bledy"]:
        for b in wynik["bledy"]:
            print("BLAD:", b)
        return 1
    wynik["kroki"].append({"krok": "naglowki", "rc": 0, "opis": "4 naglowki zgodne co do bajtu ze zrodlami"})

    budowa = tempfile.mkdtemp(prefix="budowa221-")
    dysk = zrob_dysk(tempfile.mkdtemp(prefix="dysk221-"))
    try:
        obiekty = kompiluj_zrodla(budowa)
        wynik["kroki"].append({"krok": "kompilacja_biblioteki", "rc": 0,
                               "opis": f"{len(obiekty)} plikow .o z prawdziwych zrodel biblioteki i platformy"})

        binarka, blad = zbuduj_test(TU, budowa, obiekty, os.path.join(budowa, "bieg221"))
        if binarka is None:
            print("BLAD kompilacji testu:\n" + blad[:4000])
            return 1
        wynik["kroki"].append({"krok": "kompilacja_testu", "rc": 0,
                               "opis": "Test.cpp + wyciagniety kod v3.56.4 skompilowane g++ -std=gnu++17 -DHOST"})

        # --- 3. dwa profile: z tokenem i bez ---
        profil_on = uruchom_profil(binarka, dysk, "token-etapu-221")
        profil_off = uruchom_profil(binarka, dysk, None)
        wynik["profil_z_tokenem"] = {"podsumowanie": profil_on["podsumowanie"], "rc": profil_on["rc"],
                                     "kontrole": {k: v[0] for k, v in profil_on["kontrole"].items()}}
        wynik["profil_bez_tokenu"] = {"podsumowanie": profil_off["podsumowanie"], "rc": profil_off["rc"],
                                      "kontrole": {k: v[0] for k, v in profil_off["kontrole"].items()}}
        if profil_on["rc"] != 0 or profil_off["rc"] != 0:
            print(profil_on["wyjscie"])
            print(profil_off["wyjscie"])
            print("BLAD: profil nie przeszedl")
            return 1

        # --- 4. mutanty ---
        sys.path.insert(0, TU)
        import mutanty  # noqa: E402

        for mutant in mutanty.MUTANTY:
            katalog = tempfile.mkdtemp(prefix="mut221-")
            try:
                nazwa, oczekiwane = mutanty.przygotuj(TU, os.path.join(katalog, "test"), mutant)
                binarka_m, blad = zbuduj_test(os.path.join(katalog, "test"), budowa, obiekty,
                                              os.path.join(budowa, "mutant"))
                if binarka_m is None:
                    wynik["mutanty"].append({"nazwa": nazwa, "czerwone": ["KOMPILACJA"],
                                             "oczekiwane": oczekiwane, "ok": False, "blad": blad[:500]})
                    continue
                przebieg = uruchom_profil(binarka_m, dysk, "token-etapu-221")
                czerwone = sorted(k for k, v in przebieg["kontrole"].items() if not v[0])
                ok = all(any(k.startswith(o) for k in czerwone) for o in oczekiwane)
                wynik["mutanty"].append({"nazwa": nazwa, "czerwone": czerwone, "oczekiwane": oczekiwane,
                                         "ok": ok, "podsumowanie": przebieg["podsumowanie"]})
                if not ok:
                    wynik["bledy"].append(f"mutant {nazwa} nie zostal wykryty (czerwone: {czerwone}, oczekiwane: {oczekiwane})")
            finally:
                shutil.rmtree(katalog, ignore_errors=True)

        wynik["czas_s"] = round(time.time() - start, 1)
        wynik["ok"] = not wynik["bledy"]
        wynik["kontrol"] = len(profil_on["kontrole"]) + len(profil_off["kontrole"]) - 1  # S00 liczone raz
        katalog_wyniku = os.path.join(TU, "WYNIKI", "run001")
        os.makedirs(katalog_wyniku, exist_ok=True)
        with open(os.path.join(katalog_wyniku, "wynik.json"), "w", encoding="utf-8") as f:
            json.dump(wynik, f, indent=2, ensure_ascii=False)
            f.write("\n")

        print(profil_on["podsumowanie"] + " (profil z tokenem)")
        print(profil_off["podsumowanie"] + " (profil bez tokenu)")
        for m in wynik["mutanty"]:
            print(("  OK   " if m["ok"] else "  BLAD ") + m["nazwa"] + "  czerwone=" + ",".join(m["czerwone"][:4]))
        if wynik["ok"]:
            print(f"PASS brama221 kontrol={wynik['kontrol']} profili=2 mutantow={len(wynik['mutanty'])} czas={wynik['czas_s']}s")
            return 0
        for b in wynik["bledy"]:
            print("BLAD:", b)
        print("FAIL brama221")
        return 1
    finally:
        shutil.rmtree(budowa, ignore_errors=True)
        shutil.rmtree(dysk, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
