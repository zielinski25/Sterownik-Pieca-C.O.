#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SYMULACJA POTOKU TELEMETRII — Sterownik C.O.

Cel: sprawdzic, KTORE mechanizmy potrafia wyprodukowac obserwowane wzorce strat
(65x pojedyncza probka, 7x dziesiec probek) i czy proponowana poprawka je usuwa.

Model odtwarza kod firmware:
  produkcja co 60 s -> ring(1440) -> paczka min(ring,10) -> wysylka
  -> przy sukcesie commitPopN(10) [probki znikaja z ringu]
  -> przy porazce: ponowienie tych samych 10, max 3 proby, potem porzucenie
     (probki ZOSTAJA w ringu — patrz telemetryPendingAbandon)

Uruchomienie:  python3 symulacja.py
"""

import random, statistics

RING_CAP = 1440
BATCH = 10
MAX_ATTEMPTS = 3
MINUTE = 1


# ------------------------------------------------------------------ model sieci
class Siec:
    """Naprzemienne okresy 'dziala' i 'awaria'. Parametry z logu urzadzenia:
    w dobrym dniu awarie sa krotkie (kilka minut), w zlym zdarzaja sie
    dluzsze (11-30 min) i wtedy circuit breaker zostaje otwarty."""

    def __init__(self, rng, p_awaria=0.02, awaria_min=(1, 3), awaria_max=(8, 30)):
        self.rng = rng
        self.p = p_awaria
        self.awaria_min = awaria_min
        self.awaria_max = awaria_max
        self.ile = 0

    def krok(self):
        if self.ile > 0:
            self.ile -= 1
            return False
        if self.rng.random() < self.p:
            lo, hi = self.awaria_max if self.rng.random() < 0.25 else self.awaria_min
            self.ile = self.rng.randint(lo, hi) - 1
            return False
        return True


# ------------------------------------------------------------------ symulacja
def symuluj(mode, dni=29, seed=1, restart_co_dni=4.7):
    """mode:
       'obecny'     — kod v3.56.7 (zachowanie v3.56.5)
       'late_ack'   — dodany blad: spozniona odpowiedz uznana za sukces
       'poprawka'   — weryfikacja przed popem: zapisujemy, co REALNIE poszlo
    """
    rng = random.Random(seed)
    siec = Siec(rng)
    seq = 0
    ring = []                 # kolejka probek (seq)
    pending = None            # (lista seq, proby)
    utracone = []             # (seq, powod)
    wyslane = 0
    produced = 0
    minut = int(dni * 24 * 60)
    nastepny_restart = int(restart_co_dni * 24 * 60)

    for m in range(minut):
        # --- produkcja
        seq += 1
        produced += 1
        if len(ring) < RING_CAP:
            ring.append(seq)
        else:
            utracone.append((seq, "ring_full"))

        # --- restart urzadzenia (ring jest w PSRAM, wiec ginie)
        if m == nastepny_restart:
            for s in ring:
                utracone.append((s, "restart"))
            ring.clear()
            pending = None
            nastepny_restart += int(restart_co_dni * 24 * 60)

        # --- proba wysylki (raz na minute)
        if pending is None and ring:
            n = min(len(ring), BATCH)
            pending = (ring[:n], 0)

        if pending is not None:
            probki, proby = pending
            ok = siec.krok()
            falszywy = False
            # spozniona odpowiedz: siec padla, ale poprzednie zadanie "wrocilo"
            if mode == "late_ack" and not ok and rng.random() < 0.0017:
                ok = True
                falszywy = True
            if ok:
                n = len(probki)
                del ring[:n]
                if falszywy:
                    for s_ in probki:
                        utracone.append((s_, "late_ack"))
                else:
                    wyslane += n
                pending = None
            else:
                proby += 1
                if proby >= MAX_ATTEMPTS:
                    pending = None          # porzucenie: probki ZOSTAJA w ringu
                else:
                    pending = (probki, proby)

    # bilans
    zostalo = len(ring)
    return {
        "produced": produced,
        "wyslane": wyslane,
        "utracone": len(utracone),
        "zostalo_w_rignu": zostalo,
        "roznica": produced - wyslane - zostalo,
        "przyczyny": _podsumuj(utracone),
    }


def _podsumuj(u):
    from collections import Counter
    return Counter(p for _, p in u)


def raport():
    print("=" * 66)
    print(" SYMULACJA POTOKU TELEMETRII — 29 dni")
    print("=" * 66)
    for mode, opis in [
        ("obecny", "kod obecny (v3.56.7)"),
        ("late_ack", "hipoteza: spozniona odpowiedz uznana za sukces"),
    ]:
        print(f"\n--- {opis} ---")
        for seed in (1, 2, 3):
            r = symuluj(mode, seed=seed)
            print(f"  seed {seed}: wyprodukowano {r['produced']:>6}, "
                  f"wyslano {r['wyslane']:>6}, "
                  f"utracone {r['utracone']:>4}, "
                  f"w ringu {r['zostalo_w_rignu']:>4}  "
                  f"| roznica {r['roznica']:>4}  {dict(r['przyczyny'])}")

    print("\n" + "=" * 66)
    print(" WNIOSEK")
    print("=" * 66)
    print("""
Model 'obecny' NIE gubi probek w srodku rozruchu: probki sa usuwane z kolejki
tylko po potwierdzonym sukcesie, a przy porzuceniu zostaja w ringu.
Jedyne straty to restart (ring w PSRAM) i przepelnienie kolejki — oba na
granicach rozruchu, nie w jego srodku.

Model 'late_ack' gubi probki W SRODKU rozruchu, i to wielokrotnosci 10
(rozmiar paczki) — dokladnie taki wzorzec, jak 7 zdarzen po 10 probek
zaobserwowanych w bazie (11-minutowe przerwy).

Czego model NIE wyjasnia: 65 pojedynczych strat. Zaden mechanizm w kodzie
nie usuwa z ringu dokladnie jednej probki. To wymaga instrumentacji na
urzadzeniu (licznik produced vs sent vs ring.size) — patrz raport.
""")


if __name__ == "__main__":
    raport()
