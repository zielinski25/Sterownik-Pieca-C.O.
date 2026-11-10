'use strict';
// ─────────────────────────────────────────────────────────────────────────────
// Fixture statusu — pola 1:1 wg zbudujJsonStatusPelny() z main_centrala.cpp
// (v3.58.0) + pola dodawane lokalnie przez GET /api/lan/status
// (lan / lan_api / internet — patrz handler w wifiZarejestrujTrasy()).
// ─────────────────────────────────────────────────────────────────────────────

function buildStatus(opts = {}) {
  const s = {
    wifi_mode: 'STA',
    ip: opts.ip || '10.178.89.36',
    wifi_ssid: 'Dom_Test',
    wifi_rssi: -58,
    t_zewn: 11.8, t_bojler: 48.6, t_ogrz: 58.4, t_ogrz_sr: 57.1,
    t_panel: 36.2, t_pokoj: 21.4, t_trociny: 31.0, t_powrot: 44.2,
    wilgotnosc: 54, cisnienie: 1009.25, dym: 412,
    dym_swiezy: true, dym_wlaczony: true, dym_alarm: false,
    alarm_dym_wyciszony: false, alarm_dym_wyciszenie_ms: 0,
    alarm_ogrz_wyciszony: false, alarm_ogrz_wyciszenie_ms: 0,
    alarm_sound_dym: 'cw2', alarm_sound_ogrz: 'cw2', alarm_sound_both: 'cw2',
    klapa: 81, syberka: 27, pompa: true,
    alarm_ogrzewanie: false, alarm_panel: false,
    rozpalanie_serwa_dostepne: true, rozpalanie_serwa_aktywne: false,
    rozpalanie_serwa_blokada_dymu: false,
    tryb_serwa: 1, wybor: 3, trybSerwa: 1,
    czasOn: 10, czasOff: 30, tempOn: 60, tempOff: 50,
    antystopWlaczony: true, antystopDni: 10,
    mieszadloWlaczony: true, mieszadloCzasOn: 30, mieszadloCzasOff: 5,
    rozpalanie: false, rozpalanie_potwierdzone: false, rozpalanie_pozostalo_s: 0,
    tempZadServo: 60, histerServo: 2, skokKlapy: 10, skokSyberka: 10,
    odchylTemp: 5, mnoznik: 2,
    progAlarmTemp: 80, progAlarmDym: 1000, dymCzasOn: 30, dymCzasOff: 120,
    dymCzasStabilizacji: 10, dymProgTemp: 40, dymTrybPracy: 0,
    pompa_override_min: 0, serwo_override_min: 0, mieszadlo_override_min: 0,
    mieszadlo: false,
    hist_fs: 1,
    tel_cloud_seq: 1234, tel_cloud_ts: 1762752000, tel_cloud_ago: 5,
    tel_ring: 1234, tel_pending: 0, tel_sent: 1234,
    tel_gap_events: 0, tel_gap_samples: 0,
    fw: 'v3.58.0', uptime: 3600,
    dzien: 10, miesiac: 11, rok: 2026,
    online: true, night: false, rtc_ok: true,
    updatedAt: 1762752000
  };
  // flagi symulacji (symFb() w builderze) — wszystkie nieaktywne
  ['dym', 'ogrz', 'bojler', 'panel', 'pokoj', 'zewn', 'ogrz_powrot', 'ogrz_trociny'].forEach(p => {
    s['symulacja_' + p] = false;
    s['symulacja_' + p + '_min'] = 0;
  });
  return Object.assign(s, opts.override || {});
}

// Status "starej" strony (wifiZbudujJsonStatus() — GET /api/status)
function buildOldStatus(opts = {}) {
  return Object.assign({
    wifi_mode: 'STA',
    ip: opts.ip || '10.178.89.36',
    online: true,
    internet: false,
    t_ogrz: 58.4, t_bojler: 48.6, t_zewn: 11.8, t_panel: 36.2, t_pokoj: 21.4,
    t_powrot: 44.2, t_trociny: 31.0,
    pompa: true, mieszadlo: false, klapa: 81, syberka: 27,
    tryb_serwa: 1, wybor: 3,
    dym: 412, dym_alarm: false,
    wifi_rssi: -58, rtc_ok: true,
    fw: 'v3.58.0', uptime: 3600
  }, opts.override || {});
}

module.exports = { buildStatus, buildOldStatus };
