# ESP-local panel: 1:1 `Piec.html` parity — base v3.38.0, output v3.40.0

This is a separate source patch for the HTML served by the ESP. It does not edit or replace `Cenatrala Pieca.zip`.

## Result

- `GET /` serves the current `Piec.html` snapshot byte-for-byte from `WIFI_LOCAL_PANEL_HTML` in `PROGMEM`. The HTML structure, CSS, and JavaScript are the same as the source page at patch preparation time.
- The ESP sends the page directly from flash with `beginResponse_P()` and marks it `Cache-Control: no-store`; it does not copy the 453 KB page into a temporary `String` in heap RAM.
- Existing firmware APIs and control logic are retained. The page uses the same Firebase and local-API flows as `Piec.html`; this change does not alter the servo/smoke algorithm.
- The source-reported firmware version is bumped from **v3.38.0** to **v3.40.0** (MINOR: full local-panel UI parity).

The legacy `wifiZbudujStrone()` builder remains in the source, but the `/` route no longer serves its older UI. If `Piec.html` changes later, regenerate the embedded snapshot and patch to keep the two pages identical.

## Apply

The patch expects the **v3.38.0 base** `main_centrala.cpp` at `src/main_centrala.cpp`. Applying it produces source reporting **v3.40.0**. Confirm the base source matches v3.38.0 before applying:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply it directly to `Cenatrala Pieca.zip`; that archive is not the matching v3.38.0 source base and remains unchanged. A v3.40.0 build and matching OTA release are separate steps. This preparation did **not** build or flash firmware, upload firmware, or send commands to the heater.

## Verification

- Extracted `WIFI_LOCAL_PANEL_HTML` compared byte-for-byte with the current working-tree `Piec.html`.
- The inline JavaScript from `Piec.html` passed `node --check`.
- Patch dry-run and apply against the exact v3.38.0 base passed; applying the patch reproduced the prepared C++ source byte-for-byte (`cmp`).
- Firmware build and live-device testing were not performed.
