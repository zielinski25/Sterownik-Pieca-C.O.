#!/usr/bin/env python3
# Wycina z main_centrala.cpp funkcje symulacjaUstawOdczytOdRazu() (bez komentarza nad nia).
import sys
s = open(sys.argv[1], encoding='utf-8').read()
a = s.index('static void symulacjaUstawOdczytOdRazu(const String& pole, float wartosc) {')
b = s.index('\n}\n', a) + 3
sys.stdout.write(s[a:b])
