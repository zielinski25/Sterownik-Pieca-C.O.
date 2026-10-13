# ESP-local panel HTML parity patch — base v3.38.0, output v3.39.0

This is a **separate source patch** for the HTML embedded in the ESP firmware. It does not edit or replace `Cenatrala Pieca.zip`.

Applying the patch updates the source-reported firmware version from **v3.38.0** to **v3.39.0** (MINOR: new local UI/API functionality). A matching v3.39.0 firmware build and GitHub Release are separate steps; neither is created or flashed here.

## Changes

- The local Servo dialog always shows the current flap and damper positions, refreshed from `/api/status`; values are hidden when the status is stale.
- Adds a local **Rozpalanie serw** action. The button fails closed unless it has a fresh status, a fresh/reliable boiler temperature, a complete alarm status, an available cold-start window, and no active safety alarm or ignition sequence. The new POST endpoint rechecks safety by calling the existing `uruchomRozpalanieSerwa()` helper; it does not change the servo/smoke algorithm. The endpoint requires a per-boot CSRF token, and duplicate active requests are rejected.
- Adds a chart inspector to the local SVG charts: hover/tap sample readout and crosshair, horizontal drag to pan without dragging the crosshair, pinch/wheel zoom, and double-tap/double-click reset. Vertical page scrolling remains available.
- Marks the local page and live status as non-cacheable so stale status or an expired per-boot token cannot enable a control.

## Apply

The patch expects the **v3.38.0 base** `main_centrala.cpp` source at `src/main_centrala.cpp`; applying it produces source reporting **v3.39.0**. Apply only after confirming the base source matches v3.38.0:

```sh
patch --dry-run -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
patch -p1 < /path/to/v3.38.0-local-html/main_centrala.cpp.patch
```

Do not apply it directly to `Cenatrala Pieca.zip` in the main checkout; that archive's source does not match this v3.38.0 patch base. Keep the archive unchanged. Building/flashing firmware is a separate step; this patch preparation did **not** flash the ESP, upload firmware, or send commands to the heater.

## Verification performed

- Extracted embedded page JavaScript: `node --check` passed.
- Synthetic-DOM smoke test: chart sample inspection, wheel/pinch zoom, horizontal pan, and double-tap reset passed; the active crosshair stayed on its sample during drag.
- Patch dry-run against the exact v3.38.0 source passed; applying the patch reproduced the prepared source byte-for-byte (`cmp`).
- Firmware build and live-device testing were not performed.
