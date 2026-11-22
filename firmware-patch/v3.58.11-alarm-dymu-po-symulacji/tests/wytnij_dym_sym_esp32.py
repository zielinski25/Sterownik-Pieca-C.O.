#!/usr/bin/env python3
"""Samodzielny .cpp (Arduino.h + atrapy reszty centrali) z blokiem DYM-SYM i
fragmentem pozar() z main_centrala.cpp 3.58.11 - do sprawdzenia PRAWDZIWYMI
naglowkami ESP32 (String, F(), __FlashStringHelper, noTone, digitalWrite, millis).
Uzycie: wytnij_dym_sym_esp32.py <main_centrala.cpp> <wyjscie.cpp>"""
import sys, subprocess, os
src, dst = sys.argv[1], sys.argv[2]
tu = os.path.dirname(os.path.abspath(__file__))
tmp = dst + ".wycinek.h"
subprocess.check_call([sys.executable, os.path.join(tu, "wytnij_dym_sym.py"), src, tmp])
wycinek = open(tmp, encoding="utf-8").read()
os.remove(tmp)
out = '''// WYCINEK z main_centrala.cpp v3.58.11 (blok DYM-SYM + fragment pozar()) z naglowkami ESP32 - wygenerowany przez wytnij_dym_sym_esp32.py.
#include <Arduino.h>
// --- atrapy reszty centrali (typy/sygnatury jak w pliku centrali) ---
const int pinBuzer = 1, pinZasilanieBuzera = 2;
struct AlarmMuteTimer { uint32_t a; };
AlarmMuteTimer alarmMuteDym;
int32_t alarmMuteRemainingMs(AlarmMuteTimer&, bool) { return 0; }
void zapiszLogKrytyczny(const String &tresc) { (void)tresc; }
byte czyAlarmAktywny = 0;
uint32_t dymSkasowanieAlarmuMs = 0;
bool dymNadzorPoSkasowaniuAktywny = false;
bool dymKrokSyberkaUstawiony = false;
uint32_t dymOstatniKrokSyberkaMs = 0;
byte fazaAlarmuDzwiekowego = 0;
bool czujnikDymuSwiezyOdczyt = false;
int czujnikDymuOdczyt = 0;
int progAlarmowyDymu = 500;
const int HISTEREZA_DYMU = 30;
unsigned long czasNastepnejAkcjiDymu = 0;
bool rozpalanieTrybAktywny = false, rozpalanieSerwaAktywne = false;
static bool rozpalanieSerwaWymuszenieAktywne() { return false; }
struct SerwoRuch { bool aktywny; int docelowa; };
SerwoRuch ruchKlapy = {false, 0}, ruchSyberka = {false, 0};
struct RawServo { int x; };
RawServo serwoKlapa, serwoSyberek;
int pozycjaKlapa = 0, pozycjaSyberek = 0;
void zakonczTrybRozpalania(const char*, bool) {}
static void zatrzymajRuchSerwa(SerwoRuch&) {}
void zlecServoRuch(SerwoRuch&, RawServo&, int&, int, int, int, const char*) {}

''' + wycinek + '''
void uzyj_wszystkiego() { dymAlarmPoKoncuSymulacji(true); dymAlarmPoKoncuSymulacji(false); krokAlarmuDymu(false); dymAlarmSkasuj(nullptr); }
'''
open(dst, "w", encoding="utf-8").write(out)
print("wytnij_dym_sym_esp32.py: zapisano", dst)
