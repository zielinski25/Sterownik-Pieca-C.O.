# ESP-local panel: Android-like charts and 1:1 `Piec.html` — base v3.38.0, output v3.42.0

This source patch updates the HTML served by the ESP and keeps `Cenatrala Pieca.zip` untouched.

## Result

- `GET /` serves the current `Piec.html` byte-for-byte from `WIFI_LOCAL_PANEL_HTML` in `PROGMEM`, so ESP and browser use the same page, CSS, and JavaScript.
- The chart data selector now matches Android: **TEMPERATURY / SERWA / KORELACJA**. Temperature and boiler series are selectable in TEMPERATURY; servo series in SERWA; KORELACJA exposes both groups so any combination can be selected together.
- Individual series chips and the SERIE settings sheet allow multiple active series within the selected view, retain at least one active series, and persist the choice. The settings sheet keeps the Android actions **Tylko główna** and **Wszystkie**.
- `/` sends the page directly from flash with `beginResponse_P()` and marks it `Cache-Control: no-store` rather than copying the 455 KB page into a temporary `String` in heap RAM.
- Existing firmware APIs and the servo/smoke algorithm are unchanged. The source-reported version is **v3.42.0** (advanced from v3.41.0).

## Apply

The patch expects the **v3.38.0 base** `main_centrala.cpp` at `src/main_centrala.cpp`. Applying it produces source reporting **v3.42.0**. Confirm the base source matches v3.38.0 before applying:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply it directly to `Cenatrala Pieca.zip`; that archive is not the matching v3.38.0 source base and remains unchanged. Building/flashing firmware and publishing a matching OTA release are separate steps. This preparation did **not** build or flash firmware, upload firmware, or send commands to the heater.

## Verification

- Embedded `WIFI_LOCAL_PANEL_HTML` compared byte-for-byte with the current `Piec.html`.
- Inline page JavaScript passed `node --check`.
- Mixed-series selection was checked against the Android modes; `KORELACJA` exposes both temperature and servo catalogs.
- Patch dry-run and apply against the exact v3.38.0 base passed; applying it reproduced the prepared C++ source byte-for-byte (`cmp`).
- Firmware build and live-device testing were not performed.
