#!/usr/bin/env python3
"""Model czasu: ile realnie trwa jedno pobranie pogody i co z tego widzi nadzor.

Model NIE jest przepisem "jak mi sie wydaje". Kazde ogniwo ma zrodlo:

  * wartosci liczbowe       -> z firmware 3.57.8 (zbierz_parametry.py czyta zrodlo),
  * semantyka funkcji       -> z arduino-esp32 2.0.17 (patrz ZRODLA-BIBLIOTEKI.md),
  * progi nadzoru           -> z firmware (NADZOR_PROG_MS itd.).

Uruchomienie:
    python3 model_czasu.py --src /sciezka/main_centrala.cpp
Bez --src bierze wartosci domyslne = to, co jest w 3.57.8 (4000).
"""
import argparse
import json
import os
import subprocess
import sys
from dataclasses import dataclass, field

TU = os.path.dirname(os.path.abspath(__file__))

OK, FAIL = [], []


def spr(warunek, nazwa, szczegol=""):
    (OK if warunek else FAIL).append((nazwa, szczegol))
    print(("  OK   " if warunek else "  FAIL ") + nazwa + (f" - {szczegol}" if szczegol else ""))


def sek(ms):
    return ms / 1000.0


def fmt_czas(ms):
    s = ms / 1000.0
    if s < 1:
        return f"{ms:.0f} ms"
    if s < 120:
        return f"{s:.1f} s"
    if s < 7200:
        return f"{s/60:.1f} min ({s:.0f} s)"
    return f"{s/3600:.1f} h ({s:.0f} s)"


# ══════════════════════════════════════════════════════════════════════════
# Semantyka biblioteki — odczytana ze zrodla arduino-esp32 2.0.17
# ══════════════════════════════════════════════════════════════════════════
@dataclass(frozen=True)
class Biblioteka:
    connect_timeout_ms: int = 5000  # HTTPCLIENT_DEFAULT_TCP_TIMEOUT

    def handshake_ms(self, parametr: int) -> int:
        """WiFiClientSecure.cpp: sslclient->handshake_timeout = parametr * 1000.

        Parametr jest w SEKUNDACH, a pole trzyma milisekundy — 4 s = 4000.
        """
        return parametr * 1000

    def read_timeout_ms(self, http_timeout_ms: int) -> int:
        """HTTPClient.cpp: _client->setTimeout((timeout + 500) / 1000) — sekundy."""
        return ((http_timeout_ms + 500) // 1000) * 1000


LIB = Biblioteka()


@dataclass(frozen=True)
class Konfiguracja:
    nazwa: str
    handshake_param: int
    http_timeout_ms: int


@dataclass(frozen=True)
class Scenariusz:
    symbol: str
    opis: str
    dns_ms: int = 30
    tcp_ms: int | None = 60        # None = SYN bez odpowiedzi
    tls_ms: int | None = 400       # None = serwer nie odpowiada na ClientHello
    odpowiedz_ms: int = 120
    strumien: tuple[int, int] | None = None  # (przerwa miedzy bajtami, lacznie)


@dataclass
class Wynik:
    czas_ms: int
    status: str      # ok / tcp_timeout / tls_timeout / read_timeout
    faza: str        # gdzie to stoi


def symuluj(konf: Konfiguracja, sc: Scenariusz) -> Wynik:
    t = sc.dns_ms

    # 1. TCP connect — limit _connectTimeout (domyslnie 5000 ms)
    if sc.tcp_ms is None:
        return Wynik(t + LIB.connect_timeout_ms, "tcp_timeout", "TCP connect")
    if sc.tcp_ms > LIB.connect_timeout_ms:
        return Wynik(t + LIB.connect_timeout_ms, "tcp_timeout", "TCP connect")
    t += sc.tcp_ms

    # 2. TLS handshake — limit handshake_timeout (parametr * 1000 ms)
    limit_hs = LIB.handshake_ms(konf.handshake_param)
    if sc.tls_ms is None:
        return Wynik(t + limit_hs, "tls_timeout", "TLS handshake")
    if sc.tls_ms > limit_hs:
        return Wynik(t + limit_hs, "tls_timeout", "TLS handshake")
    t += sc.tls_ms

    # 3. Odczyt odpowiedzi — limit BEZCZYNNOSCI (nie calego zadania)
    limit_od = LIB.read_timeout_ms(konf.http_timeout_ms)
    if sc.strumien is not None:
        przerwa, lacznie = sc.strumien
        if przerwa > limit_od:
            return Wynik(t + limit_od, "read_timeout", "odczyt (bezczynnosc)")
        return Wynik(t + lacznie, "ok", "-")
    t += sc.odpowiedz_ms
    return Wynik(t, "ok", "-")


@dataclass
class Nadzor:
    prog_ms: int = 300000
    max_restartow: int = 3
    licznik_wazny_ms: int = 1800000
    rozbieg_ms: int = 180000


def ocena_nadzoru(konf, sc, n: Nadzor):
    """Czy nadzor zareaguje na to pobranie i co zostanie w raporcie."""
    w = symuluj(konf, sc)
    restart = w.czas_ms > n.prog_ms
    return {
        "wynik": w,
        "restart": restart,
        "checkpoint": "BEFORE" if restart else "AFTER",
        "po_ilu_s": n.prog_ms / 1000 if restart else None,
    }


def seria_bootow(konf, sc, n: Nadzor, bramka_ms: int = 900000, horyzont_s: int = 7200):
    """Symulacja N pobran w czasie: restart -> start -> znowu pobranie itd.

    Odwzorowuje licznik restartow z firmware: zwiekszany przy restarcie,
    czyszczony po 30 min nieprzerwanej pracy, a przy licznik >= 3 nadzor
    przechodzi w NADZOR_LIMIT ("poddalem sie") — ale po wyzerowaniu licznika
    (30 min pracy) znowu restartuje.
    """
    t = 0.0
    boot = 0.0
    licznik = 0
    zdarzenia = []
    pobrania = 0
    restarty = 0
    zawieszony = False
    na_pobranie = bramka_ms / 1000  # pierwsze pobranie: bramka liczona od startu

    while t < horyzont_s:
        if zawieszony:
            if licznik > 0 and t < boot + n.licznik_wazny_ms / 1000:
                t = boot + n.licznik_wazny_ms / 1000
                licznik = 0
                zdarzenia.append((t, "licznik wyzerowany po 30 min pracy"))
                # nastepny tik nadzoru (1 s) widzi wiek >= 300 s, licznik 0
                t += 1
                licznik = 1
                restarty += 1
                boot, zawieszony = t, False
                na_pobranie = boot + bramka_ms / 1000
                zdarzenia.append((t, "RESTART (po wyzerowaniu licznika)"))
                continue
            if licznik == 0:
                t_clear = boot + n.licznik_wazny_ms / 1000
                if t < t_clear:
                    t = t_clear
                    zdarzenia.append((t, "licznik wyzerowany po 30 min pracy"))
                    continue
            t += 1
            continue

        # najblizsze zdarzenie: pobranie albo wyzerowanie licznika
        kandydaci = []
        if na_pobranie is not None:
            kandydaci.append((na_pobranie, "pobranie"))
        if licznik > 0:
            kandydaci.append((boot + n.licznik_wazny_ms / 1000, "licznik"))
        if not kandydaci:
            break
        kandydaci.sort()
        t, co = kandydaci[0]
        if t >= horyzont_s:
            break
        if co == "licznik":
            licznik = 0
            zdarzenia.append((t, "licznik wyzerowany po 30 min pracy"))
            continue

        pobrania += 1
        w = symuluj(konf, sc)
        if w.czas_ms <= n.prog_ms:
            zdarzenia.append((t, f"pobranie OK ({fmt_czas(w.czas_ms)})"))
            na_pobranie = t + bramka_ms / 1000
            continue

        t_hang = t + n.prog_ms / 1000
        zdarzenia.append((t, f"pobranie WISI ({w.status}) — {w.faza}"))
        if licznik >= n.max_restartow:
            zdarzenia.append((t_hang, "NADZOR_LIMIT — bez restartu"))
            zawieszony = True
            na_pobranie = None
            t = t_hang
            continue
        licznik += 1
        restarty += 1
        zdarzenia.append((t_hang, f"RESTART (licznik={licznik})"))
        boot = t_hang
        t = t_hang
        na_pobranie = boot + bramka_ms / 1000

    return {"zdarzenia": zdarzenia, "pobrania": pobrania, "restarty": restarty,
            "horyzont_s": horyzont_s}


# ══════════════════════════════════════════════════════════════════════════
SCENARIUSZE = [
    Scenariusz("S1", "normalny serwer", dns_ms=30, tcp_ms=60, tls_ms=400, odpowiedz_ms=120),
    Scenariusz("S2", "TLS stoi (serwer przyjal TCP, milczy na ClientHello)",
               dns_ms=30, tcp_ms=60, tls_ms=None),
    Scenariusz("S3", "brak trasy (SYN bez odpowiedzi)", dns_ms=30, tcp_ms=None),
    Scenariusz("S4", "wolny TLS 3,2 s", dns_ms=30, tcp_ms=60, tls_ms=3200, odpowiedz_ms=120),
    Scenariusz("S5", "wolny TLS 4,5 s", dns_ms=30, tcp_ms=60, tls_ms=4500, odpowiedz_ms=120),
    Scenariusz("S6", "wolny TLS 9 s", dns_ms=30, tcp_ms=60, tls_ms=9000, odpowiedz_ms=120),
    Scenariusz("S7", "odpowiedz kapie: 1 bajt co 3 s przez 400 s",
               dns_ms=30, tcp_ms=60, tls_ms=400, strumien=(3000, 400000)),
]


def tabela(konf, nadzor):
    print(f"\n--- {konf.nazwa}: setHandshakeTimeout({konf.handshake_param}) "
          f"=> limit TLS {fmt_czas(LIB.handshake_ms(konf.handshake_param))} ---")
    print(f"{'':4} {'scenariusz':52} {'czas':>16} {'status':>12} {'nadzor':>22}")
    wyniki = {}
    for sc in SCENARIUSZE:
        o = ocena_nadzoru(konf, sc, nadzor)
        w = o["wynik"]
        nadz = f"RESTART po {o['po_ilu_s']:.0f} s" if o["restart"] else "brak restartu"
        print(f"{sc.symbol:4} {sc.opis:52} {fmt_czas(w.czas_ms):>16} {w.status:>12} {nadz:>22}")
        wyniki[sc.symbol] = o
    return wyniki


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=None, help="main_centrala.cpp z 3.57.8 (stan PRZED)")
    ap.add_argument("--po", default=None, help="main_centrala.cpp z 3.57.9 (stan PO)")
    args = ap.parse_args()

    param = 4000          # tyle jest w 3.57.8
    http_to = 4000
    zrodlo_info = "wartosci domyslne (to, co jest w 3.57.8)"
    if args.src:
        out = subprocess.run([sys.executable, os.path.join(TU, "zbierz_parametry.py"),
                              "--src", args.src, "--json"], capture_output=True, text=True)
        if out.returncode != 0:
            print(out.stderr)
            return 1
        d = json.loads(out.stdout)
        param = d["deszcz"]["setHandshakeTimeout"][0]
        http_to = d["deszcz"]["http_setTimeout"][0]
        zrodlo_info = f"{args.src} (SHA-256 {d['zrodlo']['sha256'][:16]}...)"

    # Stan PO: jesli podano plik 3.57.9, bierzemy z niego REALNA wartosc,
    # a nie zakladana. Model ma mierzyc to, co jest w pliku.
    param_po = 8
    if args.po:
        out_po = subprocess.run([sys.executable, os.path.join(TU, "zbierz_parametry.py"),
                                 "--src", args.po, "--json"], capture_output=True, text=True)
        if out_po.returncode != 0:
            print(out_po.stderr)
            return 1
        d_po = json.loads(out_po.stdout)
        param_po = d_po["deszcz"]["setHandshakeTimeout"][0]
        zrodlo_info += f" | PO: {args.po} (SHA-256 {d_po['zrodlo']['sha256'][:16]}...)"

    przed = Konfiguracja("PRZED (3.57.8)", param, http_to)
    # DECYZJA UZYTKOWNIKA (2026-11-01): limit handshake TLS = 8 s.
    po = Konfiguracja(f"PO (3.57.9 — w pliku {param_po} s)", param_po, http_to)
    po4 = Konfiguracja("wariant odrzucony (4 s)", 4, http_to)
    nadzor = Nadzor()

    print("MODEL CZASU SIECI — 3.57.8 -> 3.57.9")
    print(f"zrodlo parametrow: {zrodlo_info}")
    print(f"semantyka: setHandshakeTimeout(x) => x*1000 ms; http.setTimeout(x) => x ms, "
          f"klient dostaje (x+500)/1000 s")

    w_przed = tabela(przed, nadzor)
    w_po = tabela(po, nadzor)
    w_po4 = tabela(po4, nadzor)

    # ── asercje: PRZED ───────────────────────────────────────────────────
    print("\n=== Asercje: PRZED (3.57.8, setHandshakeTimeout(4000)) ===")
    o = w_przed["S1"]
    spr(o["wynik"].status == "ok" and o["wynik"].czas_ms < 2000 and not o["restart"],
        "S1 normalny: pobranie konczy sie w milisekundach, bez restartu",
        fmt_czas(o["wynik"].czas_ms))
    o = w_przed["S2"]
    spr(o["wynik"].status == "tls_timeout", "S2 TLS stoi: konczy sie timeoutem TLS")
    spr(o["wynik"].czas_ms == 4000090,
        "S2 limit TLS to 4 000 000 ms (66 min), nie 4 s", fmt_czas(o["wynik"].czas_ms))
    spr(o["restart"] is True and o["checkpoint"] == "BEFORE",
        "S2 => nadzor restartuje po 300 s, ostatni znacznik DESZCZ / BEFORE",
        f"restart={o['restart']}, checkpoint={o['checkpoint']}")
    o = w_przed["S3"]
    spr(o["wynik"].status == "tcp_timeout" and not o["restart"],
        "S3 brak trasy: 5 s i konczy sie sam, bez restartu", fmt_czas(o["wynik"].czas_ms))
    o = w_przed["S7"]
    spr(o["restart"] is True,
        "S7 wolna odpowiedz (kapie): restart, bo caly odczyt trwa > 300 s",
        fmt_czas(o["wynik"].czas_ms))

    # ── asercje: PO (8 s — decyzja uzytkownika) ─────────────────────────
    print("\n=== Asercje: PO (setHandshakeTimeout(8) — decyzja z 1.11) ===")
    if args.po:
        spr(param_po == 8, "w pliku 3.57.9 stoi setHandshakeTimeout(8)", f"jest {param_po}")
    o = w_po["S2"]
    spr(o["wynik"].status == "tls_timeout" and o["wynik"].czas_ms == 8090,
        "S2 TLS stoi: konczy sie po 8 s (a nie po 66 min)", fmt_czas(o["wynik"].czas_ms))
    spr(o["restart"] is False and o["checkpoint"] == "AFTER",
        "S2 => brak restartu, znacznik AFTER zostaje zapisany")
    spr(o["wynik"].czas_ms * 37 < nadzor.prog_ms,
        "S2: limit 8 s to nadal 37x mniej niz prog nadzoru 300 s")
    o = w_po["S1"]
    spr(o["wynik"].status == "ok" and o["wynik"].czas_ms == 610,
        "S1 normalny: bez zmian (610 ms)", fmt_czas(o["wynik"].czas_ms))
    o = w_po["S3"]
    spr(o["wynik"].czas_ms == 5030, "S3 brak trasy: bez zmian (5 s)", fmt_czas(o["wynik"].czas_ms))
    o = w_po["S4"]
    spr(o["wynik"].status == "ok", "S4 wolny TLS 3,2 s: przechodzi", fmt_czas(o["wynik"].czas_ms))
    o = w_po["S5"]
    spr(o["wynik"].status == "ok",
        "S5 wolny TLS 4,5 s: przechodzi (to przewaga 8 s nad 4 s)", fmt_czas(o["wynik"].czas_ms))
    o = w_po4["S5"]
    spr(o["wynik"].status == "tls_timeout",
        "wariant 4 s: ten sam przypadek NIE przechodzi — dlatego odrzucony")
    o = w_po["S6"]
    spr(o["wynik"].status == "tls_timeout" and o["wynik"].czas_ms == 8090,
        "S6 wolny TLS 9 s: odcięty po 8 s, bez restartu", fmt_czas(o["wynik"].czas_ms))
    # S7 (odpowiedz "kapie") jest z zalozenia nieograniczony czasowo — limit
    # odczytu resetuje sie przy kazdym bajcie, wiec liczymy max dla S1-S6.
    najgorszy = max(symuluj(po, sc).czas_ms for sc in SCENARIUSZE if sc.strumien is None)
    spr(najgorszy < nadzor.prog_ms,
        "najgorszy czas jednego GET po zmianie (S1-S6) < 300 s",
        f"max={fmt_czas(najgorszy)} = TCP 5 s + TLS 8 s + odczyt 4 s")
    o = w_po["S7"]
    spr(o["restart"] is True,
        "S7 wolna odpowiedz (kapie): 8 s tego NIE naprawia — osobny temat",
        f"restart={o['restart']}")

    # ── hipoteza numer jeden: petla ponowien ─────────────────────────────
    print("\n=== Hipoteza 1: czy kod ponawia probe (3 x 125 s = 375 s)? ===")
    if args.src:
        d = json.loads(subprocess.run(
            [sys.executable, os.path.join(TU, "zbierz_parametry.py"), "--src", args.src, "--json"],
            capture_output=True, text=True).stdout)
        dd = d["deszcz"]
        spr(dd["get_w_funkcji"] == 1, "w funkcji jest dokladnie jedno http.GET()",
            f"GET={dd['get_w_funkcji']}")
        spr(dd["get_w_petli"] == 0, "zadne GET nie jest w petli",
            f"GET w petli={dd['get_w_petli']}")
        spr(dd["setConnectTimeout"] == [], "setConnectTimeout nie jest wolane (domyslne 5000 ms)")
    print("       Gdyby kod robil 3 proby: 3 x 125 s = 375 s > 300 s -> restart.")
    print("       Ale kod robi JEDNO GET, wiec hipoteza 'petla ponowien' NIE tlumaczy")
    print("       objawu. Tlumaczy go limit TLS: 4000 trafia jako SEKUNDY, nie ms.")

    # ── seria N pobran w czasie ──────────────────────────────────────────
    print("\n=== N pobran w 2 h (S2, pobranie co 15 min po starcie) ===")
    s_przed = seria_bootow(przed, SCENARIUSZE[1], nadzor)
    s_po = seria_bootow(po, SCENARIUSZE[1], nadzor)
    for nazwa, s in (("PRZED", s_przed), ("PO   ", s_po)):
        print(f"  {nazwa}: pobran={s['pobrania']}, restartow={s['restarty']}")
    print("  os PRZED (pierwsze 10 zdarzen):")
    for t, co in s_przed["zdarzenia"][:10]:
        print(f"    t={t:7.0f} s  {co}")

    spr(s_po["restarty"] == 0, "PO: zero restartow w ciagu 2 h",
        f"restartow={s_po['restarty']}")
    spr(s_przed["restarty"] >= 3, "PRZED: seria restartow w ciagu 2 h",
        f"restartow={s_przed['restarty']}")

    # ── podsumowanie ─────────────────────────────────────────────────────
    print(f"\nRAZEM: {len(OK)} OK / {len(FAIL)} FAIL")
    for nazwa, szczegol in FAIL:
        print(f"  FAIL: {nazwa} - {szczegol}")
    return 0 if not FAIL else 1


if __name__ == "__main__":
    sys.exit(main())
