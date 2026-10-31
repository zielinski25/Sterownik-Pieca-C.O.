#!/usr/bin/env python3
# [ETAP 222] Brama: klik w przycisku panelu -> plik CSV na dysku, przez prawdziwe TCP.
#
# Kroki:
#   1. sprawdza, ze pliki w nasz/ sa te same co w etapie 221 (i te same co zrodla),
#   2. sprawdza, ze wyciete fragmenty to nadal tekst z v3.56.4,
#   3. kompiluje prawdziwe zrodla biblioteki + mostek (Most.cpp),
#   4. uruchamia mostek i puszcza przez niego PRAWDZIWY skrypt panelu (node:vm),
#      w dwoch profilach: z tokenem i bez tokenu,
#   5. psuje po kolei 6 miejsc i sprawdza, ze test siada.
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
STARY = os.path.join(REPO, "narzedzia", "prawdziwy-serwer221")
BIB = os.path.join(STARY, "biblioteka")
PLAT = os.path.join(STARY, "platforma")

INCLUDE = [
    "-I", PLAT,
    "-I", os.path.join(BIB, "espasyncwebserver", "src"),
    "-I", os.path.join(BIB, "arduino-core-api", "api"),
    "-I", os.path.join(BIB, "esp32-core"),
    "-I", os.path.join(BIB, "libb64", "include"),
    "-I", os.path.join(TU, "nasz"),
]

BIBLIOTEKA_CPP = ["WebServer", "WebRequest", "WebHandlers", "WebResponses", "AsyncWebHeader", "Middleware", "AsyncWebServerRequest"]
API_CPP = ["String", "Print", "Stream", "IPAddress", "Common"]
PLATFORMA_CPP = ["Platform", "AsyncTCP", "FS", "AuthZaslepki"]

MUTANTY = [
    ("M1_panel_zawsze_krotka_trasa", "Panel222.html",
     "var trasa = (strumien === 'long') ? '/history/long' : '/history';",
     "var trasa = '/history';"),
    ("M2_panel_inna_dlugosc_biletu", "Panel222.html",
     "if (bilet.length !== 32) throw new Error('centrala nie dała biletu');",
     "if (bilet.length !== 31) throw new Error('centrala nie dała biletu');"),
    ("M3_trasa_history_goly_napis", "nasz/trasy221.inc",
     'wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY,',
     'wifiWebServer.on("/history", HTTP_ANY,'),
    ("M4_bez_content_disposition", "nasz/HistoriaPobranie.h",
     '    odp->addHeader("Content-Disposition",',
     '    if (false) odp->addHeader("Content-Disposition",'),
    ("M5_pusty_bufor_odpowiedzi", "nasz/HistoriaPobranie.h",
     "    if (r.code == historia211::ReadCode::Data) return r.bytes;",
     "    if (r.code == historia211::ReadCode::Data) return 0;"),
    ("M6_bilet_nie_jednorazowy", "nasz/HistoriaBilety.h",
     "    if (trafienie == kMiejsc) return false;",
     "    if (trafienie == kMiejsc) return false;\n    if (true) return true;  // mutant: bilet NIE jest zuzywany"),
]


def uruchom(cmd, **kw):
    return subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kw)


def sha256(sciezka):
    import hashlib
    with open(sciezka, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def wolny_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


def kompiluj_obiekty(budowa):
    obiekty = []
    zadania = [(f"bib_{n}.o", os.path.join(BIB, "espasyncwebserver", "src", n + ".cpp")) for n in BIBLIOTEKA_CPP]
    zadania += [(f"api_{n}.o", os.path.join(BIB, "arduino-core-api", "api", n + ".cpp")) for n in API_CPP]
    zadania += [(f"plat_{n}.o", os.path.join(PLAT, n + ".cpp")) for n in PLATFORMA_CPP]
    for cel, zrodlo in zadania:
        wyjscie = os.path.join(budowa, cel)
        r = uruchom(["g++", "-std=gnu++17", "-DHOST", "-O1", "-w", "-c", "-o", wyjscie, zrodlo] + INCLUDE)
        if r.returncode != 0:
            raise SystemExit(f"BLAD kompilacji {zrodlo}:\n{r.stdout.decode('utf-8', 'replace')[:3000]}")
        obiekty.append(wyjscie)
    return obiekty


def zbuduj_mostek(katalog_zrodel, budowa, obiekty, binarka):
    include = ["-I", os.path.join(katalog_zrodel, "nasz")] + INCLUDE + ["-I", os.path.join(STARY, "nasz")]
    r = uruchom(["g++", "-std=gnu++17", "-DHOST", "-O1", "-w", "-o", binarka,
                 os.path.join(katalog_zrodel, "Most.cpp")] + obiekty + include)
    if r.returncode != 0:
        return r.stdout.decode("utf-8", "replace")
    return ""


def zrob_dysk(katalog):
    os.makedirs(os.path.join(katalog, "dysk", "littlefs"), exist_ok=True)
    os.makedirs(os.path.join(katalog, "pobrane"), exist_ok=True)
    with open(os.path.join(katalog, "dysk", "littlefs", "hist_short.csv"), "w", encoding="utf-8") as f:
        f.write("czas;temp_ogrz;temp_bojler\n2026-10-28 10:00;52.5;47.0\n2026-10-28 10:08;52.9;47.4\n2026-10-28 10:16;53.4;47.9\n")
    with open(os.path.join(katalog, "dysk", "littlefs", "hist_long.csv"), "w", encoding="utf-8") as f:
        f.write("czas;temp_ogrz;temp_bojler\n2026-09-01 00:00;40.1;35.2\n2026-09-01 00:08;40.4;35.5\n2026-09-01 00:16;40.2;35.1\n2026-09-01 00:24;39.8;34.9\n")
    return katalog


def start_mostu(binarka, dysk, token, port):
    srodowisko = dict(os.environ)
    srodowisko["HIST_ROOT"] = os.path.join(dysk, "dysk")
    srodowisko["ETAP222_PORT"] = str(port)
    if token is None:
        srodowisko.pop("ETAP222_TOKEN", None)
    else:
        srodowisko["ETAP222_TOKEN"] = token
    proces = subprocess.Popen([binarka], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=srodowisko)
    linia = proces.stdout.readline().decode("utf-8", "replace").strip()
    if not linia.startswith("NASLUCH"):
        proces.kill()
        raise SystemExit(f"mostek nie wystartowal: {linia}")
    return proces


def node_test(binarka_mostu, dysk, token, tryb, plik_panelu):
    port = wolny_port()
    proces = start_mostu(binarka_mostu, dysk, token, port)
    try:
        katalog = os.path.join(dysk, "pobrane-" + tryb)
        os.makedirs(katalog, exist_ok=True)
        try:
            r = uruchom(["node", os.path.join(TU, "panel222.mjs"), plik_panelu, str(port), katalog, tryb], cwd=TU, timeout=60)
        except subprocess.TimeoutExpired as e:
            tekst = (e.stdout or b"").decode("utf-8", "replace")
            return {"rc": 124, "wyjscie": tekst + "\nFAIL [panel222] node przekroczyl czas", "pobrane": {},
                    "podsumowanie": ["FAIL [panel222] node przekroczyl czas"]}
        tekst = r.stdout.decode("utf-8", "replace")
        pobrane = {}
        for nazwa in ("historia-krotka.csv", "historia-dluga.csv"):
            p = os.path.join(katalog, nazwa)
            if os.path.exists(p):
                pobrane[nazwa] = sha256(p)
        return {"rc": r.returncode, "wyjscie": tekst, "pobrane": pobrane,
                "podsumowanie": [l for l in tekst.splitlines() if l.startswith(("PASS panel222", "FAIL [panel222]"))][-1:]}
    finally:
        proces.terminate()
        try:
            proces.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proces.kill()


def main():
    start = time.time()
    wynik = {"etap": 222, "kiedy": time.strftime("%Y-%m-%d %H:%M:%S"), "mutanty": [], "bledy": []}

    # --- 1. nasze pliki: te same co w etapie 221 ---
    for nazwa in ("HistoriaPobranie.h", "HistoriaPin.h", "HistoriaBilety.h", "CheckedHistoryIO.h",
                  "bramka221.inc", "trasy221.inc"):
        if sha256(os.path.join(TU, "nasz", nazwa)) != sha256(os.path.join(STARY, "nasz", nazwa)):
            wynik["bledy"].append(f"nasz/{nazwa} rozni sie od etapu 221")
    if sha256(os.path.join(TU, "Panel222.html")) != sha256(os.path.join(REPO, "narzedzia", "panel-trasa218", "Panel218.html")):
        wynik["bledy"].append("Panel222.html rozni sie od panelu wycietego w etapie 218")
    if wynik["bledy"]:
        for b in wynik["bledy"]:
            print("BLAD:", b)
        return 1
    wynik["kroki"] = [{"krok": "zrodla", "rc": 0, "opis": "6 plikow naszych + panel zgodne co do bajtu z etapami 218/221"}]

    # --- 2. ekstrakcja v3.56.4 ---
    r = uruchom([sys.executable, "-B", os.path.join(STARY, "zrob_ekstrakcje.py")])
    wynik["kroki"].append({"krok": "ekstrakcja", "rc": r.returncode, "opis": r.stdout.decode("utf-8", "replace").strip()})
    if r.returncode != 0:
        print(r.stdout.decode("utf-8", "replace"))
        return 1

    budowa = tempfile.mkdtemp(prefix="budowa222-")
    dysk = zrob_dysk(tempfile.mkdtemp(prefix="dysk222-"))
    try:
        obiekty = kompiluj_obiekty(budowa)
        binarka = os.path.join(budowa, "most222")
        blad = zbuduj_mostek(TU, budowa, obiekty, binarka)
        if blad:
            print("BLAD kompilacji mostka:\n" + blad[:3000])
            return 1
        wynik["kroki"].append({"krok": "kompilacja", "rc": 0, "opis": f"{len(obiekty)} .o biblioteki + mostek"})

        # --- 3. dwa profile ---
        token = node_test(binarka, dysk, "token-etapu-222", "token", os.path.join(TU, "Panel222.html"))
        bez = node_test(binarka, dysk, None, "bez-tokenu", os.path.join(TU, "Panel222.html"))
        wynik["profil_z_tokenem"] = {"rc": token["rc"], "podsumowanie": token["podsumowanie"], "pobrane": token["pobrane"]}
        wynik["profil_bez_tokenu"] = {"rc": bez["rc"], "podsumowanie": bez["podsumowanie"], "pobrane": bez["pobrane"]}
        if token["rc"] != 0 or bez["rc"] != 0:
            print(token["wyjscie"])
            print(bez["wyjscie"])
            print("BLAD: profil nie przeszedl")
            return 1

        # --- 4. mutanty ---
        for nazwa, plik, szukane, zamiana in MUTANTY:
            katalog = tempfile.mkdtemp(prefix="mut222-")
            try:
                kopia = os.path.join(katalog, "etap")
                shutil.copytree(TU, kopia, ignore=shutil.ignore_patterns("bin", "__pycache__", "WYNIKI", "*.pyc"))
                cel = os.path.join(kopia, plik)
                tekst = open(cel, encoding="utf-8").read()
                if tekst.count(szukane) != 1:
                    wynik["bledy"].append(f"{nazwa}: napis wystepuje {tekst.count(szukane)} razy")
                    continue
                open(cel, "w", encoding="utf-8").write(tekst.replace(szukane, zamiana, 1))

                if plik.endswith(".html"):
                    binarka_m, panel_m = binarka, cel
                else:
                    binarka_m = os.path.join(budowa, "most_" + nazwa)
                    blad = zbuduj_mostek(kopia, budowa, obiekty, binarka_m)
                    if blad:
                        wynik["mutanty"].append({"nazwa": nazwa, "wykryty": True, "powod": "nie kompiluje sie"})
                        continue
                    panel_m = os.path.join(TU, "Panel222.html")
                przebieg = node_test(binarka_m, dysk, "token-etapu-222", "token", panel_m)
                wykryty = przebieg["rc"] != 0
                wynik["mutanty"].append({"nazwa": nazwa, "wykryty": wykryty,
                                         "pierwszy_blad": next((l for l in przebieg["wyjscie"].splitlines() if l.startswith("FAIL")), "")})
                if not wykryty:
                    wynik["bledy"].append(f"mutant {nazwa} nie zostal wykryty")
            finally:
                shutil.rmtree(katalog, ignore_errors=True)

        wynik["czas_s"] = round(time.time() - start, 1)
        wynik["ok"] = not wynik["bledy"]
        katalog_wyniku = os.path.join(TU, "WYNIKI", "run001")
        os.makedirs(katalog_wyniku, exist_ok=True)
        with open(os.path.join(katalog_wyniku, "wynik.json"), "w", encoding="utf-8") as f:
            json.dump(wynik, f, indent=2, ensure_ascii=False)
            f.write("\n")

        print(token["podsumowanie"][0] if token["podsumowanie"] else "")
        print(bez["podsumowanie"][0] if bez["podsumowanie"] else "")
        for m in wynik["mutanty"]:
            print(("  OK   " if m["wykryty"] else "  BLAD ") + m["nazwa"] + "  " + m.get("pierwszy_blad", "")[:70])
        if wynik["ok"]:
            print(f"PASS brama222 profili=2 mutantow={len(wynik['mutanty'])} czas={wynik['czas_s']}s")
            return 0
        for b in wynik["bledy"]:
            print("BLAD:", b)
        print("FAIL brama222")
        return 1
    finally:
        shutil.rmtree(budowa, ignore_errors=True)
        shutil.rmtree(dysk, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
