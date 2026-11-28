#!/usr/bin/env python3
# Wycina z main_centrala.cpp linie C++ dodane w 3.58.14 (ZEWN-ZRODLO) i sklada je
# w male funkcje do kompilacji na PC (test_zrodlo_zew.cpp). Kazdy fragment musi
# wystapic w pliku dokladnie raz - inaczej blad (test wykrywa "zgubiona" zmiane).
import sys
s = open(sys.argv[1], encoding='utf-8').read()
def jeden(start, stop):
    if s.count(start) != 1: sys.exit('BLAD: fragment "%s" wystepuje %d razy' % (start.strip()[:60], s.count(start)))
    a = s.index(start); b = s.index(stop, a)
    return s[a:b]
galaz = jeden('  } else if (pole == "zewnZrodlo") {\n', '  } else if (pole == "mieszadloWlaczony") {\n')
setup = jeden('  zewnZrodlo               = config.getUChar("zewnZrodlo", 1);', '  Czas_ON ')
ja    = jeden('  ja("\\"zewnZrodlo\\":"', '\n') + '\n'
lan   = jeden('  j += F("\\"zewnZrodlo\\":");', '\n') + '\n'
ust   = jeden('    json += ",\\"zewnZrodlo\\":"', '\n') + '\n'
dekl  = jeden('byte zewnZrodlo = 1;', '\n') + '\n'
print('// wyciete automatycznie z ' + sys.argv[1])
print(dekl)
print('bool ustawZrodlo(const String& pole, long w, String& err) {\n  if (false) {\n' + galaz + '  } else { err = "nieznane pole"; return false; }\n  return true;\n}')
print('void wczytajZrodlo() {\n' + setup + '}')
print('String statusPelny() { String out; auto ja = [&](const String& x) { out += x; };\n' + ja + '  return out; }')
print('String statusLan() { String j;\n' + lan + '  return j; }')
print('String ustawieniaGet() { String json;\n' + ust + '  return json; }')
