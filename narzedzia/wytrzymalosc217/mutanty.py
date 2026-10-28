#!/usr/bin/env python3
"""217: KONTROLE NEGATYWNE - celowo zepsute naglowki musza wywalic test.

Brama, ktora nigdy nie jest czerwona, nic nie znaczy. Tutaj psujemy dokladnie
to, co ten etap ma pilnowac - zwalnianie uchwytu i czyszczenie flagi pobrania -
i sprawdzamy, ze soak to lapie:

  M1  ~Odpowiedz nie zamyka zapinki        -> wyciek uchwytu (G01/G02)
  M2  ~Odpowiedz nie czysci flagi zajetosci -> drugie pobranie zawodzi (G01)
  M3  Pin::close() nie zamyka uchwytu VFS   -> wyciek (G01/G06/G09)

Kazdy mutant: kopia naglowka z jedna zmiana, ten sam Test.cpp, kompilacja
i uruchomienie. Oczekiwane: kompiluje sie i konczy kodem 1 z "FAIL wytrzymalosc217".

Uruchomienie:
    python3 -B narzedzia/wytrzymalosc217/mutanty.py
"""
import os
import shutil
import subprocess
import sys
import tempfile

TU = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(TU, "..", ".."))
N = os.path.join(REPO, "narzedzia")
D211 = os.path.join(N, "wysylka-zapinka211")
D212 = os.path.join(N, "pobranie-trasa212")
D213 = os.path.join(N, "panel-pobrania213")
FW = os.path.join(REPO, "firmware-patch", "v3.55.8-blokada-fs-po-starcie")

FLAGS = ["-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", "-pthread",
         "-fno-exceptions", "-fno-rtti"]

M1_STARY = """    if (pin_ != nullptr) {
      pin_->close();
      delete pin_;
      pin_ = nullptr;
    }
"""
M1_NOWY = """    if (pin_ != nullptr) {
      pin_ = nullptr;   // MUTANT M1: zapinka NIE jest zamykana
    }
"""

M2_STARY = """    zajeta_ = false;
  }
  bool _sourceValid() const override { return !awaria_ && pin_ != nullptr && pin_->active(); }
"""
M2_NOWY = """    // MUTANT M2: flaga zajetosci NIE jest czyszczona
  }
  bool _sourceValid() const override { return !awaria_ && pin_ != nullptr && pin_->active(); }
"""

M3_STARY = """    const int fd = fd_;
    history193::Calls *io = io_;
    fd_ = -1;
    io_ = nullptr;
    const int rc = io->close(fd);
    return rc == 0 && io->error() == 0;
"""
M3_NOWY = """    fd_ = -1;        // MUTANT M3: uchwyt zostaje otwarty w VFS
    io_ = nullptr;
    return true;
"""

MUTANTY = [
    ("M1", "HistoriaPobranie.h", M1_STARY, M1_NOWY,
     "~Odpowiedz nie zamyka zapinki"),
    ("M2", "HistoriaPobranie.h", M2_STARY, M2_NOWY,
     "~Odpowiedz nie czysci flagi zajetosci"),
    ("M3", "HistoriaPin.h", M3_STARY, M3_NOWY,
     "Pin::close() nie zamyka uchwytu VFS"),
]


def main():
    bledy = []
    for kod, plik, stary, nowy, opis in MUTANTY:
        zrodlo = os.path.join(D211 if plik == "HistoriaPin.h" else D213, plik)
        with open(zrodlo, "r", encoding="utf-8", newline="") as f:
            tekst = f.read()
        if tekst.count(stary) != 1:
            bledy.append("%s: nie ma dokladnie jednego fragmentu do podmiany" % kod)
            continue
        tmp = tempfile.mkdtemp(prefix="mut%s-" % kod)
        try:
            with open(os.path.join(tmp, plik), "w", encoding="utf-8", newline="") as f:
                f.write(tekst.replace(stary, nowy, 1))
            shutil.copyfile(os.path.join(TU, "Test.cpp"), os.path.join(tmp, "Test.cpp"))
            exe = os.path.join(tmp, "test")
            include = ["-I" + tmp, "-I" + TU, "-I" + D213, "-I" + os.path.join(D213, "host"),
                       "-I" + D211, "-I" + os.path.join(D211, "host"),
                       "-I" + D212, "-I" + os.path.join(D212, "host"), "-I" + FW]
            budowa = subprocess.run(["c++", *FLAGS, *include,
                                     os.path.join(tmp, "Test.cpp"), "-o", exe],
                                    capture_output=True, text=True, timeout=900)
            if budowa.returncode != 0:
                bledy.append("%s: mutant sie nie kompiluje:\n%s" % (kod, budowa.stderr[-1500:]))
                continue
            uruchom = subprocess.run([exe], capture_output=True, text=True, timeout=900)
            if uruchom.returncode == 0:
                bledy.append("%s (%s): test PRZESZEDL na zepsutym kodzie - brama jest slepa"
                             % (kod, opis))
                continue
            if "FAIL wytrzymalosc217" not in uruchom.stdout:
                bledy.append("%s: kod wyjscia %d, ale brak podsumowania FAIL"
                             % (kod, uruchom.returncode))
                continue
            podsumowanie = [l for l in uruchom.stdout.split("\n")
                            if l.startswith("FAIL wytrzymalosc217")]
            print("PASS mutant %s (%s) -> %s" % (kod, opis, podsumowanie[-1][:90]))
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    for b in bledy:
        print("FAIL mutanty217 " + b)
    print("%s mutanty217 sprawdzen=%d bledow=%d"
          % ("FAIL" if bledy else "PASS", len(MUTANTY), len(bledy)))
    return 1 if bledy else 0


if __name__ == "__main__":
    sys.exit(main())
