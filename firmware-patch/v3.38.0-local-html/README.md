# ESP-local panel: 1:1 `Piec.html` parity — base v3.38.0, output v3.41.0

This is a separate source patch for the HTML served by the ESP. It does not edit or replace `Cenatrala Pieca.zip`.

## Result

- `GET /` serves the current `Piec.html` snapshot byte-for-byte from `WIFI_LOCAL_PANEL_HTML` in `PROGMEM`. The HTML structure, CSS, and JavaScript are the same as the source page at patch preparation time.
- The ESP sends the page directly from flash with `beginResponse_P()` and marks it `Cache-Control: no-store`; it does not copy the 453 KB page into a temporary `String` in heap RAM.
- The chart offers independent toggles for every series. Any subset of boiler/temperature and servo traces can be active together; the individual choice persists, all series are available at once, and mixed temperature/servo charts use automatic °C/% axes.
- Existing firmware APIs and control logic are retained. The page uses the same Firebase and local-API flows as `Piec.html`; this change does not alter the servo/smoke algorithm.
- The patch targets a v3.38.0 source tree and outputs `FIRMWARE_VERSION` **v3.41.0**. The chart feature advances the previous source version **v3.40.0 → v3.41.0** (MINOR).

The legacy `wifiZbudujStrone()` builder remains in the source, but the `/` route no longer serves its older UI. If `Piec.html` changes later, regenerate the embedded snapshot and patch to keep the two pages identical.

## Apply

The patch expects the **v3.38.0 base** `main_centrala.cpp` at `src/main_centrala.cpp`. Applying it produces source reporting **v3.41.0**. Confirm the base source matches v3.38.0 before applying:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply it directly to `Cenatrala Pieca.zip`; that archive is not the matching v3.38.0 source base and remains unchanged. A v3.41.0 build and matching OTA release are separate steps. This preparation did **not** build or flash firmware, upload firmware, or send commands to the heater.

## Verification

- Extracted `WIFI_LOCAL_PANEL_HTML` compared byte-for-byte with the current `Piec.html`.
- The inline JavaScript passed `node --check`; a multi-series smoke test confirmed that temperature, boiler, and servo series can be selected together.
- Patch dry-run and apply against the exact v3.38.0 base passed; applying the patch reproduced the prepared C++ source byte-for-byte (`cmp`).
- Firmware build and live-device testing were not performed.
