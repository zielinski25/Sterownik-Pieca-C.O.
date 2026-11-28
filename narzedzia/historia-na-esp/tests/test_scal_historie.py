#!/usr/bin/env python3
# Test scal_historie.py na syntetycznych plikach odwzorowujacych prawdziwe
# hist_short/hist_long z Drive (13.09 -> 31.10.2026): 35 i 36 kolumn, " 0" ze
# spacja, CRLF, naglowek, duplikaty z kilku kopii, urwany ostatni wiersz, smieci.
import os, subprocess, sys, tempfile, random
from datetime import datetime
random.seed(7)
SK = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'scal_historie.py')
T0 = 1789326000  # 13.09.2026 19:00 CEST (pierwszy wiersz prawdziwego hist_long (5).csv)

def dlugi(t, kol=36, spacja=False):
    et = '%02d.%02d %02d:%02d' % ((t // 86400) % 28 + 1, 9 + (t // (86400 * 28)) % 3, (t // 3600) % 24, (t // 60) % 60)
    w = [et] + ['%.1f' % (20 + (t % 97) / 10.0)] * 27 + [(' 0' if spacja else '0')] * 3 + ['8'] * 3 + [str(t)]
    if kol == 36: w.append(str(t % 2))
    return ','.join(w)

def krotki(t):
    return '%02d:%02d,11.5,31.8,26.2,26.2,20.0,16.8,15.0,0,1024.2,96,180,90,%d' % ((t // 3600) % 24, (t // 60) % 60, t)

bledy = 0
def spr(ok, co):
    global bledy; print(('  [ OK ] ' if ok else '  [BLAD] ') + co); bledy += 0 if ok else 1

d = tempfile.mkdtemp()
# hist_long (5): 10 400 wierszy co 5 min (36 dni, jak prawdziwy 1,46 MB), 35 kolumn, czesc z " 0"
L5 = [dlugi(T0 + i * 300, 35, spacja=(i % 5 == 0)) for i in range(10400)]
open(os.path.join(d, 'hist_long (5).csv'), 'w', newline='').write('\n'.join(L5) + '\n')
# hist_long (6): zaczyna sie w srodku (5), co minute, 36 kolumn, CRLF, urwany ostatni wiersz
t6 = T0 + 10000 * 300
L6 = [dlugi(t6 + i * 60, 36) for i in range(3000)]
open(os.path.join(d, 'hist_long (6).csv'), 'w', newline='').write('\r\n'.join(L6) + '\r\n' + dlugi(t6 + 3000 * 60)[:40])
# hist_long (7): nakladka na (6) + nowe + NAGLOWEK + wiersz-smiec + ten sam czas, inna tresc
L7 = [dlugi(t6 + i * 60, 36) for i in range(2000, 5000)]
tresc7 = 'etykieta,t_zewn_sr,t_zewn_min\n' + '\n'.join(L7) + '\nto,nie,jest,wiersz\n' + dlugi(t6 + 2500 * 60, 36).replace(',1\n', ',0\n') + '\n'
open(os.path.join(d, 'hist_long (7).csv'), 'w', newline='').write(tresc7)
# hist_short (8) i (9): 2 x 2500 wierszy co minute z nakladka 500, plus wiersze z czasem 0 (brak RTC)
ts = t6 + 3 * 86400
S8 = [krotki(ts + i * 60) for i in range(2500)]
S9 = [krotki(ts + i * 60) for i in range(2000, 4500)] + ['12:00,1,2,3,4,5,6,7,0,1000,50,0,0,0']
open(os.path.join(d, 'hist_short (8).csv'), 'w', newline='').write('\n'.join(S8) + '\n')
open(os.path.join(d, 'hist_short (9).csv'), 'w', newline='').write('\n'.join(S9) + '\n')
open(os.path.join(d, 'cos_innego.csv'), 'w').write('a,b,c\n1,2,3\n')

out = os.path.join(d, 'data')
r = subprocess.run([sys.executable, SK, d, '-o', out], capture_output=True, text=True)
print(r.stdout[-1800:])
spr(r.returncode == 0, 'skrypt konczy sie kodem 0')
lng = open(os.path.join(out, 'hist_long.csv'), 'rb').read()
sht = open(os.path.join(out, 'hist_short.csv'), 'rb').read()
ll = lng.decode().split('\n'); assert ll[-1] == ''; ll = ll[:-1]
sl = sht.decode().split('\n'); assert sl[-1] == ''; sl = sl[:-1]
spr(len(ll) == 8000, 'hist_long.csv: dokladnie 8000 wierszy (limit firmware), bylo %d unikalnych' % (10400 + 5000 - 1000 - 80))
spr(len(sl) == 1440, 'hist_short.csv: dokladnie 1440 wierszy')
spr(b'\r' not in lng and b'\r' not in sht and b'\x00' not in lng, 'LF, bez CR i bajtow zerowych')
spr(all(len(l.split(',')) in (35, 36) for l in ll), 'kazdy wiersz dlugi ma 35 albo 36 kolumn')
spr(all(len(l.split(',')) == 14 for l in sl), 'kazdy wiersz krotki ma 14 kolumn')
czasy = [int(l.split(',')[34]) for l in ll]
spr(czasy == sorted(czasy) and len(set(czasy)) == len(czasy), 'wiersze dlugie rosnaco po czas_unix, bez duplikatow')
czs = [int(l.split(',')[13]) for l in sl]
spr(czs == sorted(czs) and len(set(czs)) == len(czs) and czs[-1] == ts + 4499 * 60 and czs[0] == ts + (4500 - 1440) * 60, 'krotki = ostatnie 1440 minut z obu kopii (nakladka zliczona raz)')
spr(czasy[-1] == t6 + 4999 * 60, 'ostatni wiersz dlugi = najnowszy z (7)')
spr(not any(l.startswith('etykieta') or l.startswith('to,nie') for l in ll), 'naglowek i smiec odrzucone')
spr(any(', 0,' in l for l in ll), 'tresc wierszy nietknieta (" 0" ze spacja zostaje - firmware toFloat to zje)')
k2500 = [l for l in ll if l.split(',')[34] == str(t6 + 2500 * 60)]
spr(len(k2500) == 1, 'ten sam czas_unix w dwoch kopiach -> jeden wiersz')
spr('duplikatow (ten sam czas_unix)' in r.stdout and 'przerwy dluzsze' not in r.stdout, 'raport: duplikaty policzone, brak falszywych przerw')
spr('bez_czasu=1' in r.stdout, 'wiersz z czas_unix=0 (brak RTC) zgloszony i pominiety')
# brak plikow -> kod 2
r2 = subprocess.run([sys.executable, SK, os.path.join(d, 'nie_ma')], capture_output=True, text=True)
spr(r2.returncode == 2, 'nieistniejaca sciezka -> kod 2')
# --- --na-teraz: przesuniecie w czasie do podgladu wykresow ---
import time as _time
out2 = os.path.join(d, 'data_teraz')
r3 = subprocess.run([sys.executable, SK, d, '-o', out2, '--na-teraz'], capture_output=True, text=True)
spr(r3.returncode == 0 and 'PRZESUNIETA w czasie' in r3.stdout, '--na-teraz: kod 0 i ostrzezenie o przesunieciu w raporcie')
ll2 = open(os.path.join(out2, 'hist_long.csv')).read().split('\n')[:-1]
sl2 = open(os.path.join(out2, 'hist_short.csv')).read().split('\n')[:-1]
cz2 = [int(l.split(',')[34]) for l in ll2]; cs2 = [int(l.split(',')[13]) for l in sl2]
teraz = int(_time.time()) // 60 * 60
spr(len(ll2) == 8000 and len(sl2) == 1440 and abs(max(cz2[-1], cs2[-1]) - teraz) <= 60, '--na-teraz: ta sama liczba wierszy, najnowszy wiersz = biezaca minuta')
delta = cs2[-1] - czs[-1]
spr(delta % 60 == 0 and all(b - a == delta for a, b in zip(czasy, cz2)) and all(b - a == delta for a, b in zip(czs, cs2)),
    '--na-teraz: jeden wspolny offset (pelne minuty) dla obu plikow, odstepy miedzy wierszami bez zmian')
spr(all(l.split(',')[1:34] == m.split(',')[1:34] for l, m in zip(ll, ll2)) and all(l.split(',')[1:13] == m.split(',')[1:13] for l, m in zip(sl, sl2)),
    '--na-teraz: wartosci pomiarow nietkniete, zmienione tylko etykieta i czas_unix')
# etykieta musi pasowac do czas_unix w czasie polskim - tak sprawdza firmware (historiaNormalizujLegacyUnix)
def etyk_ok(unix, etyk, z_data):
    try:
        from zoneinfo import ZoneInfo
        tz = ZoneInfo('Europe/Warsaw')
    except Exception:
        return True   # bez tzdata nie da sie sprawdzic niezaleznie
    t = datetime.fromtimestamp(unix, tz=tz)
    return etyk == t.strftime('%d.%m %H:%M' if z_data else '%H:%M')
spr(all(etyk_ok(int(l.split(',')[34]), l.split(',')[0], True) for l in ll2[::97]) and all(etyk_ok(int(l.split(',')[13]), l.split(',')[0], False) for l in sl2[::53]),
    '--na-teraz: etykiety HH:MM / DD.MM HH:MM przepisane z nowego czasu polskiego (CET/CEST) - zgodne z czas_unix')
print('\n' + ('NIEZALICZONY: %d BLAD' % bledy if bledy else 'ZALICZONY: 21 sprawdzen OK'))
sys.exit(1 if bledy else 0)
