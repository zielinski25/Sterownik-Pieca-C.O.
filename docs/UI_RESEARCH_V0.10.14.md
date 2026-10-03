# UI RESEARCH — STEROWNIK CO v0.10.14

Data: 2026-10-01

## Wnioski z inspiracji

### Grafana
Interakcje, które warto naśladować: crosshair + tooltip, wspólna orientacja czasu, pan/zoom oraz czytelny zakres czasu. Grafana opisuje też współdzielenie crosshair/tooltip pomiędzy panelami.
Źródło: https://grafana.com/docs/grafana/latest/visualizations/panels-visualizations/visualizations/time-series/

### Material 3
Dla przycisków ważne są wyraźne stany pressed/selected oraz ripple/state layer. Android Developers opisuje ripple jako część aktualnego zachowania Material 3.
Źródło: https://developer.android.com/develop/ui/compose/designsystems/material3

### MPAndroidChart
Biblioteka ma gotowe elementy, których obecny custom Canvas potrzebuje: pinch zoom, drag/pan, dual axes, highlighting z popup/markerem oraz animacje wykresów. Aktualne README projektu podaje wydanie 4.0.0.
Źródło: https://github.com/PhilJay/MPAndroidChart

### Vico
Vico rozwija nowoczesne podejście do zoom/scroll state i animowanego zoomu; jego dokumentacja pokazuje też utrzymanie pozycji viewportu przy aktualizacji danych.
Źródło: https://github.com/patrykandpatrick/vico/releases

## Decyzja v0.10.14

Na razie nie dokładamy biblioteki wykresowej. Rozszerzamy obecny custom View, bo dane i filtr jakości są już zaimplementowane. Jeżeli UX nadal będzie odstawał po testach v0.10.14, następnym kandydatem do eksperymentu jest MPAndroidChart 4.0.0 w osobnej gałęzi/wersji testowej.
