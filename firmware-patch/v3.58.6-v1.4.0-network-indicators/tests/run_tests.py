#!/usr/bin/env python3
"""Test patchy na oryginalnych ZIP-ach; podaj LCD.zip CENTRALA.zip."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

if len(sys.argv) != 3:
    raise SystemExit('Uzycie: python3 tests/run_tests.py LCD.zip CENTRALA.zip')
base = Path(__file__).resolve().parents[1]
aj = Path(os.environ.get('ARDUINOJSON_SRC', '/tmp/ArduinoJson-7.2.1/src'))
if not (aj / 'ArduinoJson.h').is_file():
    raise SystemExit('Ustaw ARDUINOJSON_SRC na katalog src biblioteki ArduinoJson 7')
with tempfile.TemporaryDirectory(prefix='ikony-test-') as temp:
    root = Path(temp)
    for name, archive in [('lcd', sys.argv[1]), ('centrala', sys.argv[2])]:
        dest = root / name
        dest.mkdir()
        with zipfile.ZipFile(archive) as z:
            z.extractall(dest)
        subprocess.run(['patch', '--fuzz=0', '-p1', '-i', str(base / (name + '.patch'))], cwd=dest, check=True)
    lcd, central = root / 'lcd', root / 'centrala'
    (lcd / 'src' / 'SiecIkony.h').write_bytes((base / 'lcd' / 'SiecIkony.h').read_bytes())
    dashboard = (lcd / 'src' / 'dashboard_lvgl_poc.cpp').read_text()
    start = dashboard.index('void lvgl_dashboard_poc_aktualizuj_siec(')
    end = dashboard.index('static void dym_ikona_odswiez(', start)
    (lcd / 'render_fragment.inc').write_text(dashboard[start:end])
    main = (lcd / 'src' / 'main.cpp').read_text()
    assert 'SiecIkony::odczytaj(doc)' in main
    assert 'WiFi.status() == WL_CONNECTED ? WiFi.RSSI()' in main
    assert main.count('lcdSygnalCentrali(),\n                                 stan.internet,') == 3
    assert 'lvgl_dashboard_poc_aktualizuj_siec(stan.polaczono, lcdSygnalCentrali(), stan.internet)' in main
    assert 'g_polaczono = polaczono;' not in dashboard[start:end]
    cm = (central / 'src' / 'main_centrala.cpp').read_text()
    assert '#include "InternetProbe.h"' in cm and 'internet_wan' in cm
    assert 'wanProbeTick();' in cm[cm.index('void loop() {'):]
    for source, includes, exe in [
        ('SiecIkonyTest.cpp', [aj, lcd / 'src', lcd], 'lcd-test'),
        ('InternetProbeTest.cpp', [base / 'centrala'], 'wan-test'),
    ]:
        args = ['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror']
        for inc in includes:
            args += ['-I', str(inc)]
        subprocess.run(args + [str(base / 'tests' / source), '-o', str(root / exe)], check=True)
        subprocess.run([str(root / exe)], check=True)
