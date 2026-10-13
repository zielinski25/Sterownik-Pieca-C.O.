# ESP-local panel: Android multi-select chart groups — base v3.38.0, output v3.43.0

This source patch updates the HTML served by the ESP and keeps the main-checkout `Cenatrala Pieca.zip` untouched.

## Result

- `GET /` serves the current `Piec.html` byte-for-byte from `WIFI_LOCAL_PANEL_HTML` in `PROGMEM`; ESP and browser therefore use the same page, styles, and JavaScript.
- The **DANE** row now matches Android: **TEMPERATURY**, **SERWA**, and **POMPA** are independent multi-select groups. You can enable any combination; the last enabled group cannot be turned off.
- Series remain independently selectable within enabled groups, including mixed temperature, servo, and pump plots. Series settings retain the **Tylko główna** and **Wszystkie** actions, require at least one active series, and persist group and series choices. Existing v3.42 exclusive-focus preferences migrate to the closest matching group selection.
- Pump history reads bit 0 of telemetry `state`: `100` means PRACA and `0` means STOP; missing/invalid state stays missing rather than being shown as STOP. Pump traces are drawn as step lines, with PRACA/STOP labels in pump-only view.
- `/` sends the page directly from flash with `beginResponse_P()` and marks it `Cache-Control: no-store`, rather than copying the 458 KB page into a temporary `String` in heap RAM.
- Existing firmware APIs and the servo/smoke algorithm are unchanged. The source-reported version is **v3.43.0**, advanced from v3.42.0.

## Apply

The patch expects the **v3.38.0 base** `main_centrala.cpp` at `src/main_centrala.cpp`. Applying it produces source reporting **v3.43.0**. Confirm the base source matches v3.38.0 before applying:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply it directly to `Cenatrala Pieca.zip`; that archive is not the matching v3.38.0 source base and remains unchanged. Building/flashing firmware and publishing a matching OTA release are separate steps. This preparation did **not** build or flash firmware, upload firmware, or send commands to the heater.

## Verification

- Embedded `WIFI_LOCAL_PANEL_HTML` compared byte-for-byte with the current `Piec.html`.
- Inline page JavaScript passed `node --check`.
- Group-selection smoke test checked the default temperature group, every multi-group combination, the last-group guard, and independent per-series toggles.
- Pump-state handling preserves unknown states and maps the `state` bit to PRACA/STOP values.
- Patch dry-run and apply against the exact v3.38.0 base passed; applying it reproduced the prepared C++ source byte-for-byte (`cmp`).
- Firmware build and live-device testing were not performed.
