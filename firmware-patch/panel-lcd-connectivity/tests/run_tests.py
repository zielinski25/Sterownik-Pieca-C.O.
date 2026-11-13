#!/usr/bin/env python3
"""Hostowy test patcha LCD; ARDUINOJSON_SRC wskazuje katalog src ArduinoJson 7."""
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

base = Path(__file__).resolve().parents[1]
repo = base.parents[1]
aj = Path(os.environ.get('ARDUINOJSON_SRC', '/tmp/ArduinoJson-7.2.1/src'))
if not (aj / 'ArduinoJson.h').is_file():
    raise SystemExit('Ustaw ARDUINOJSON_SRC na katalog src biblioteki ArduinoJson 7')
with tempfile.TemporaryDirectory(prefix='lcd-test-') as temp:
    dest = Path(temp)
    with zipfile.ZipFile(repo / 'LCD Centrali.zip') as archive:
        archive.extractall(dest)
    subprocess.run(['patch', '-p1', '-i', str(base / 'lcd-connectivity.patch')], cwd=dest, check=True)
    (dest / 'src' / 'SiecIkony.h').write_bytes((base / 'src' / 'SiecIkony.h').read_bytes())
    dashboard = (dest / 'src' / 'dashboard_lvgl_poc.cpp').read_text()
    start = dashboard.index('void lvgl_dashboard_poc_aktualizuj_siec(')
    end = dashboard.index('static void dym_ikona_odswiez(', start)
    (dest / 'render_fragment.inc').write_text(dashboard[start:end])
    main = (dest / 'src' / 'main.cpp').read_text()
    handler = main[main.index('void obslugaStatusFetch() {'):main.index('void pobierzUstawienia() {')]
    assert handler.count('lvgl_dashboard_poc_aktualizuj_siec(false, lcdSygnalCentrali(), stan.internet);') == 2
    assert 'SiecIkony::odczytaj(doc)' in handler
    assert 'WiFi.status() == WL_CONNECTED ? WiFi.RSSI()' in main
    assert 'lvgl_dashboard_poc_aktualizuj_siec(stan.polaczono, rssiLcd, stan.internet)' in main
    assert 'lvgl_dashboard_poc_aktualizuj(stan.polaczono' in handler
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I', str(aj), '-I', str(dest / 'src'), '-I', str(dest),
                    str(base / 'tests' / 'SiecIkonyTest.cpp'), '-o', str(dest / 'test')], check=True)
    subprocess.run([str(dest / 'test')], check=True)

    central = dest / 'central'
    (central / 'src').mkdir(parents=True)
    (central / 'src' / 'main_centrala.cpp').write_bytes(
        (repo / 'firmware-patch/v3.54.0-offline-buffer-toggle/main_centrala.cpp').read_bytes())
    subprocess.run(['patch', '-p1', '-i', str(base / 'centrala-connectivity.patch')], cwd=central, check=True)
    cm = (central / 'src' / 'main_centrala.cpp').read_text()
    assert '#include "InternetProbe.h"' in cm
    assert 'wanProbeTick();' in cm[cm.index('void loop() {'):]
    assert 'internet_wan' in cm
    subprocess.run(['g++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I', str(base / 'centrala'), str(base / 'tests' / 'InternetProbeTest.cpp'),
                    '-o', str(dest / 'test_wan')], check=True)
    subprocess.run([str(dest / 'test_wan')], check=True)
