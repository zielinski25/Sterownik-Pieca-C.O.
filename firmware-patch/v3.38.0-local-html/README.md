# ESP-local charts — base v3.38.0, output v3.44.0

This patch makes the ESP **Wykresy** tab read its history from the controller's own PSRAM/LittleFS archive. The page remains shared with `Piec.html`; the main-checkout `Cenatrala Pieca.zip` is not modified.

## Local chart behavior

- **6 h / 24 h:** fetch `GET /api/history` (the controller's 60-second ring, persisted in `/hist_short.csv`).
- **7 days / 30 days:** fetch `GET /api/history/long` plus the latest 24 hours from `/api/history`; merge and filter both by the selected time window. The long history is the controller's local event archive (up to 8,000 entries), not a Firebase shard.
- On an ESP-served page, the available historical series, pump indicator, refresh button, and periodic refresh use these local CSV endpoints. Firebase chart-history requests and Firebase live-point appends are bypassed while Wykresy is open. A local API error is shown as an error; the chart does **not** fall back to Firebase.
- Existing local CSV data includes temperatures, servo positions, pressure, humidity, and the short-range pump state. The existing archive does not record `t_trociny` or `dym`, so those optional series have no local historical points; they are not substituted with cloud values.
- The long CSV keeps its original first 35 columns and appends a 36th pump-state column (`0`/`1`). The firmware reader remains compatible with old 35-column files; historic rows without a pump value stay unknown (`-1`), never falsely STOP.

The source-reported firmware version advances from **v3.43.0** to **v3.44.0**. No heater-control, smoke-alarm, servo, or pump-control algorithm was changed. No firmware build or flash was performed.

## Apply

The patch is against the v3.38.0 source at `src/main_centrala.cpp`. In a separate copy of that source tree, verify and apply:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply the patch to the modified `Cenatrala Pieca.zip` in the main checkout. Firmware build, flashing, and any OTA release are separate operations.

## Verification

- The `WIFI_LOCAL_PANEL_HTML` raw string was refreshed from the current `Piec.html` and byte-compared.
- Inline JavaScript syntax and local short/long CSV parsing were checked with Node.js, including backward-compatible 35-column history and the new optional pump column.
- The cumulative patch was dry-run/applied against the v3.38.0 source and reproduced the prepared v3.44.0 source byte-for-byte.
- No firmware build, upload, flash, or heater command was run.
