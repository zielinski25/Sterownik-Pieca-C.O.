#!/usr/bin/env python3
"""rozbicie.py <binarka> <zrzut pomiar_zrzut> [--glebia N] [--stos]
Kto trzyma pamiec: sumuje bloki wg pierwszej (liczac od malloc) funkcji z
katalogu src/ firmware'u; --glebia 2 dokleja funkcje, ktora ja wywolala."""
import subprocess, sys, collections, re, os
bin_, zrzut = sys.argv[1], sys.argv[2]
glebia = int(sys.argv[sys.argv.index('--glebia') + 1]) if '--glebia' in sys.argv else 1
pokaz_stos = '--stos' in sys.argv
wiersze = []
adresy = set()
for l in open(zrzut):
    t = l.split()
    esp, bajty, bloki, lacznie = int(t[0]), int(t[1]), int(t[2]), int(t[3])
    ramki = [int(x, 16) - 1 for x in t[4:]]
    wiersze.append((esp, bajty, bloki, lacznie, ramki))
    adresy.update(ramki)
adresy = sorted(adresy)
sym = {}
if adresy:
    wy = subprocess.run(['addr2line', '-f', '-C', '-e', bin_] + [hex(a) for a in adresy], capture_output=True, text=True).stdout.split('\n')
    for i, a in enumerate(adresy):
        fn = wy[2 * i]; pl = wy[2 * i + 1]
        sym[a] = (fn, pl)
def krotko(fn):
    fn = re.sub(r'\(.*', '', fn)
    return fn
def kategoria(pl):
    if '/src/' in pl and '/LCD-Centrali/src/' in pl or re.search(r'/(src|v1[0-9]+[a-z]?)/(main\.cpp|dashboard_lvgl_poc\.cpp|lvgl_port\.cpp|siec_nieblokujaca_LCD\.h|diagnostyka_panelu\.h)', pl): return 'fw'
    if re.search(r'(main\.cpp|dashboard_lvgl_poc\.cpp|lvgl_port\.cpp|siec_nieblokujaca_LCD\.h|diagnostyka_panelu\.h):', pl): return 'fw'
    if '/lvgl/' in pl: return 'lvgl'
    if 'ArduinoJson' in pl: return 'json'
    if '/mock/' in pl: return 'atrapa'
    if 'symulator_panelu' in pl: return 'test'
    return 'inne'
grupy = collections.defaultdict(lambda: [0, 0, 0])
razem = [0, 0, 0]
for esp, bajty, bloki, lacznie, ramki in wiersze:
    wl = []
    posr = set()
    for a in ramki:
        fn, pl = sym[a]
        k = kategoria(pl)
        if k == 'fw':
            wl.append(krotko(fn))
            if len(wl) >= glebia: break
        elif not wl:
            posr.add(k)
    if not wl: wl = ['(poza firmware: %s)' % ','.join(sorted(posr))]
    przez = 'LVGL' if 'lvgl' in posr else ('JSON' if 'json' in posr else ('String/inne' ))
    klucz = (' <- '.join(wl), przez)
    g = grupy[klucz]; g[0] += esp; g[1] += bloki; g[2] += bajty
    razem[0] += esp; razem[1] += bloki; razem[2] += bajty
    if pokaz_stos and esp >= 2000:
        print('--- %d B, %d blokow' % (esp, bloki))
        for a in ramki: print('     %s  %s' % (krotko(sym[a][0]), sym[a][1].split('/')[-1]))
print('%8s %6s  %s' % ('B (ESP)', 'blokow', 'kto'))
for (kto, przez), g in sorted(grupy.items(), key=lambda kv: -kv[1][0])[:int(os.environ.get('ILE', '45'))]:
    print('%8d %6d  %s  [%s]' % (g[0], g[1], kto, przez))
print('%8d %6d  RAZEM' % (razem[0], razem[1]))
