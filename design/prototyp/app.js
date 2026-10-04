/* ═══════════════════════════════════════════════════════════════════════════
   STEROWNIK CO — prototyp: logika pulpitu, kafelków, arkuszy ustawień, poleceń.
   Stan = makieta JSON-a statusu z Firebase (te same klucze, co w aplikacji).
   ═══════════════════════════════════════════════════════════════════════════ */
(function () {
  'use strict';
  const $ = (q, r) => (r || document).querySelector(q);
  const $$ = (q, r) => Array.from((r || document).querySelectorAll(q));
  const fmt1 = v => (Math.round(v * 10) / 10).toFixed(1);
  const pad2 = n => String(n).padStart(2, '0');

  // ───────────────────────── MAKIETA STANU (status JSON) ─────────────────────────
  const now = new Date();
  const S = {
    online: true, night: false,
    t_zewn: 11.8, t_ogrz: 58.4, t_ogrz_sr: 57.1, t_bojler: 48.6, t_panel: 36.2, t_pokoj: 21.4,
    cisnienie: 1009, wilgotnosc: 54, dym: 412,
    dym_wlaczony: true, dym_swiezy: true, dym_alarm: false, alarm_ogrzewanie: false, alarm_panel: false,
    pompa: true, mieszadlo: false, rozpalanie: false,
    tryb_serwa: 1, klapa: 81, syberka: 27,
    wybor: 3, pompa_override_min: 0, mieszadlo_override_min: 0, serwo_override_min: 0,
    rtc_ok: true, dzien: now.getDate(), miesiac: now.getMonth() + 1, rok: now.getFullYear(),
    // ustawienia (lustro)
    czasOn: 10, czasOff: 30, tempOn: 60, tempOff: 50, antystopWlaczony: true, antystopDni: 10,
    mieszadloWlaczony: true, mieszadloCzasOn: 30, mieszadloCzasOff: 5,
    tempZadServo: 60, histerServo: 2, skokKlapy: 10, skokSyberka: 10, odchylTemp: 5, mnoznik: 2,
    progAlarmTemp: 80, progAlarmDym: 1000, dymCzasOn: 30, dymCzasOff: 120, dymCzasStabilizacji: 10, dymProgTemp: 40, dymTrybPracy: 0,
    sym: {} // pole -> {wartosc, min}
  };
  const SYM_POLA = { zewn: 't_zewn', ogrz: 't_ogrz', bojler: 't_bojler', panel: 't_panel', pokoj: 't_pokoj', cisnienie: 'cisnienie', wilgotnosc: 'wilgotnosc', dym: 'dym' };
  const real = Object.assign({}, S); // bazowe odczyty do dryfu
  const symAktywna = pole => !!S.sym[pole];
  const val = (pole, key) => symAktywna(pole) ? S.sym[pole].wartosc : S[key];

  // ───────────────────────── DEFINICJE KAFELKÓW ─────────────────────────
  const TILES = [
    { id: 'zewn', grid: 'g1', title: 'Zewnętrzna', ico: 'zewn' },
    { id: 'ogrz', grid: 'g1', title: 'Piec C.O.', ico: 'ogrz' },
    { id: 'bojler', grid: 'g1', title: 'Bojler', ico: 'bojler' },
    { id: 'panel', grid: 'g1', title: 'Panel słon.', ico: 'panel' },
    { id: 'pokoj', grid: 'g1', title: 'Pomieszczenie', ico: 'pokoj' },
    { id: 'cisnienie', grid: 'g1', title: 'Ciśnienie', ico: 'cisnienie' },
    { id: 'wilgotnosc', grid: 'g1', title: 'Wilgotność', ico: 'wilgotnosc' },
    { id: 'pompa', grid: 'g2', title: 'Pompa', ico: 'pompa' },
    { id: 'serwo', grid: 'g2', title: 'Serwo', ico: 'serwo' },
    { id: 'mieszadlo', grid: 'g2', title: 'Mieszadło', ico: 'mieszadlo' },
    { id: 'dym', grid: 'g2', title: 'Czujnik dymu', ico: 'dym' }
  ];
  const tileEls = {}, icoKeys = {};

  function buildTiles() {
    TILES.forEach(t => {
      const el = document.createElement('div');
      el.className = 'tile c-none'; el.dataset.id = t.id;
      el.innerHTML = '<div class="well"></div><span class="badge">LIVE</span><div class="title">' + t.title + '</div><div class="value">—</div><div class="desc"></div><div class="bar"><i></i></div>';
      el.addEventListener('click', () => { el.classList.remove('flash'); void el.offsetWidth; el.classList.add('flash'); openMenu(t.id); });
      $('#' + t.grid).appendChild(el);
      tileEls[t.id] = el;
    });
  }

  function illustrate(id, state) {
    const def = ILU[TILES.find(t => t.id === id).ico];
    const well = $('.well', tileEls[id]);
    const key = def.key ? def.key(state) : '';
    if (!well.firstChild || icoKeys[id] !== key) { well.innerHTML = def.svg(state); icoKeys[id] = key; }
    if (def.update) def.update(well, state);
  }

  // stan kafelka: {cls, value, unit, desc, badge, badgeCls, frac}
  function setTile(id, o) {
    const el = tileEls[id];
    const stale = !S.online;
    el.className = 'tile ' + (o.cls || 'c-none') + (stale ? ' stale' : '') + (o.sim ? ' sim warn' : '') + (o.state ? ' ' + o.state : '');
    const v = $('.value', el);
    const txt = o.value + (o.unit ? '<small>' + o.unit + '</small>' : '');
    if (v.dataset.last !== txt) {
      if (v.dataset.last !== undefined && !stale) { el.classList.remove('flash'); void el.offsetWidth; el.classList.add('flash'); }
      v.innerHTML = txt; v.dataset.last = txt;
    }
    v.classList.toggle('sm', o.value.length > 9);
    v.style.color = (!stale && o.vcol) || '';
    $('.desc', el).textContent = o.desc || '';
    const b = $('.badge', el);
    b.textContent = stale ? 'STALE' : o.badge || 'LIVE';
    b.className = 'badge ' + (stale ? 'stale' : o.badgeCls || 'live');
    $('.bar i', el).style.width = Math.round(Math.max(0, Math.min(1, o.frac || 0)) * 100) + '%';
    illustrate(id, o.ill || S);
  }

  // ───────────────────────── RENDER PULPITU (= renderDashboard + odswiezKafelki) ─────────────────────────
  const trybNazwa = { 1: 'AUTO', 2: 'RĘCZNY', 3: 'BEZPIECZNY' };
  function render() {
    const symBadge = pole => symAktywna(pole) ? { badge: 'SYM ' + S.sym[pole].min + ' min', badgeCls: 'sim', sim: true } : null;
    const tz = val('zewn', 't_zewn');
    setTile('zewn', Object.assign({ value: fmt1(tz), unit: '°C', cls: tz < 0 ? 'c-flame2' : tz > 15 ? 'c-ember' : 'c-flame', frac: (tz + 20) / 60, desc: tz < 0 ? 'mróz' : tz > 25 ? 'upał' : '', vcol: tz < 0 ? '#7dd3fc' : tz > 15 ? '#ffb86b' : '', ill: { night: S.night, frost: tz < 0 } }, symBadge('zewn')));
    const to = val('ogrz', 't_ogrz');
    setTile('ogrz', Object.assign({ value: fmt1(to), unit: '°C', cls: 'c-ember', frac: to / 160, desc: 'śr. ' + fmt1(S.t_ogrz_sr) + '°C', state: S.alarm_ogrzewanie ? 'err' : '', badge: S.alarm_ogrzewanie ? 'ALARM' : 'LIVE', badgeCls: S.alarm_ogrzewanie ? 'alarm' : 'live', ill: { alarm: S.alarm_ogrzewanie } }, symBadge('ogrz')));
    const tb = val('bojler', 't_bojler');
    setTile('bojler', Object.assign({ value: fmt1(tb), unit: '°C', cls: 'c-ember', frac: (tb - 15) / 55, desc: tb > 45 ? 'woda gorąca' : tb > 30 ? 'woda ciepła' : 'woda chłodna', ill: { t_bojler: tb } }, symBadge('bojler')));
    const tp = val('panel', 't_panel');
    setTile('panel', Object.assign({ value: fmt1(tp), unit: '°C', cls: 'c-slonce', frac: tp / 120, desc: S.night ? 'noc' : 'nasłonecznienie ok', state: S.alarm_panel ? 'err' : '', badge: S.alarm_panel ? 'ALARM' : 'LIVE', badgeCls: S.alarm_panel ? 'alarm' : 'live', ill: { alarm: S.alarm_panel, night: S.night } }, symBadge('panel')));
    const tr = val('pokoj', 't_pokoj');
    setTile('pokoj', Object.assign({ value: fmt1(tr), unit: '°C', cls: 'c-ok', frac: (tr - 10) / 20, desc: '' }, symBadge('pokoj')));
    const pc = val('cisnienie', 'cisnienie');
    setTile('cisnienie', Object.assign({ value: String(Math.round(pc)), unit: 'hPa', cls: 'c-fiolet', frac: (pc - 970) / 70, desc: pc < 1000 ? 'niż — możliwy gorszy ciąg' : pc > 1020 ? 'wyż' : 'stabilnie', ill: { cisnienie: pc } }, symBadge('cisnienie')));
    const hu = val('wilgotnosc', 'wilgotnosc');
    setTile('wilgotnosc', Object.assign({ value: String(Math.round(hu)), unit: '%', cls: 'c-flame2', frac: hu / 100, desc: '' }, symBadge('wilgotnosc')));

    const strat = { 1: 'Trociniak', 2: 'Kopciuch', 3: 'Auto' }[S.wybor] || '';
    setTile('pompa', { value: S.pompa ? 'WŁĄCZONA' : 'WYŁĄCZONA', cls: 'c-flame', state: S.pompa ? 'ok' : '', frac: S.pompa ? 1 : 0, desc: strat + (S.pompa_override_min > 0 ? ' • ręcznie ' + S.pompa_override_min + ' min' : ''), badge: S.pompa ? 'PRACUJE' : 'POSTÓJ', badgeCls: S.pompa ? 'live' : 'stale', ill: { pompa: S.pompa } });
    const klapaPct = Math.round(S.klapa * 100 / 180), sybPct = Math.round(S.syberka * 100 / 90);
    setTile('serwo', { value: trybNazwa[S.tryb_serwa] || '—', cls: S.tryb_serwa === 1 ? 'c-flame' : 'c-none', state: S.tryb_serwa === 2 ? 'warn' : S.tryb_serwa === 3 ? 'err' : '', frac: klapaPct / 100, desc: 'klapa ' + S.klapa + '° • syberka ' + S.syberka + '°' + (S.serwo_override_min > 0 ? ' • ' + S.serwo_override_min + ' min' : ''), badge: 'K' + klapaPct + '% S' + sybPct + '%', badgeCls: 'active', ill: { tryb_serwa: S.tryb_serwa, klapa: S.klapa } });
    setTile('mieszadlo', { value: S.mieszadlo ? 'WŁĄCZONE' : 'WYŁĄCZONE', cls: 'c-ok', state: S.mieszadlo ? 'ok' : '', frac: S.mieszadlo ? 1 : 0, desc: S.rozpalanie ? 'override: rozpalanie' : S.mieszadloWlaczony ? 'cykl ' + S.mieszadloCzasOn + ' s / ' + S.mieszadloCzasOff + ' min' : 'automatyka wyłączona', badge: S.mieszadlo ? 'PRACUJE' : 'POSTÓJ', badgeCls: S.mieszadlo ? 'live' : 'stale', ill: { mieszadlo: S.mieszadlo } });
    const dymV = val('dym', 'dym');
    const dymDis = !S.dym_wlaczony && !symAktywna('dym');
    setTile('dym', Object.assign({
      value: dymDis ? 'OFF' : String(Math.round(dymV)), unit: dymDis ? '' : 'ADC', cls: 'c-none',
      state: dymDis ? 'dis' : S.dym_alarm ? 'err' : '', frac: dymDis ? 0 : dymV / 4095,
      desc: dymDis ? 'piec zimny — czujnik wyłączony' : S.dym_alarm ? 'próg ' + S.progAlarmDym + ' ADC przekroczony' : S.dym_swiezy ? 'próg alarmu ' + S.progAlarmDym : 'ostatni pomiar',
      badge: dymDis ? 'WYŁ.' : S.dym_alarm ? 'ALARM' : 'OK', badgeCls: dymDis ? 'stale' : S.dym_alarm ? 'alarm' : 'live', ill: { dym_alarm: S.dym_alarm, off: dymDis }
    }, symBadge('dym')));

    // HERO + pasek + baner + szybkie karty
    const anyAlarm = S.dym_alarm || S.alarm_ogrzewanie || S.alarm_panel;
    $('#heroTemp').textContent = fmt1(to) + ' °C'; $('#heroTemp').classList.toggle('err', S.alarm_ogrzewanie);
    $('#heroMode').textContent = trybNazwa[S.tryb_serwa] || '—'; $('#heroMode').className = S.tryb_serwa === 2 ? 'warn' : S.tryb_serwa === 3 ? 'err' : '';
    $('#heroPump').textContent = S.pompa ? 'ON' : 'OFF'; $('#heroPump').className = S.pompa ? 'ok' : '';
    $('#heroAlarm').textContent = anyAlarm ? 'ALARM' : 'OK'; $('#heroAlarm').className = anyAlarm ? 'err' : 'ok';
    ILU.heroBoiler.update($('#heroBoiler'), S);
    const st = $('#heroState');
    st.textContent = !S.online ? 'BRAK ŁĄCZNOŚCI' : anyAlarm ? 'ALARM' : to > 45 ? 'GRZANIE' : 'CZUWANIE';
    st.className = 'pill ' + (!S.online ? 'warn' : anyAlarm ? 'err' : 'live');
    $('#alarmBanner').classList.toggle('on', anyAlarm);
    $('#alarmText').textContent = S.dym_alarm ? 'ALARM DYMU — sprawdź kotłownię' : S.alarm_ogrzewanie ? 'PRZEGRZANIE PIECA — ' + fmt1(to) + ' °C' : 'PRZEGRZANIE PANELU — ' + fmt1(tp) + ' °C';
    const chip = $('#chip'); chip.textContent = S.online ? '● LIVE' : '● STALE'; chip.className = 'chip' + (S.online ? '' : ' stale');
    $('#pg-pulpit').classList.toggle('offline', !S.online);
    setQuick('qPompa', S.pompa ? 'WŁĄCZONA' : 'WYŁĄCZONA', S.pompa ? 'ok' : '');
    setQuick('qSerwo', trybNazwa[S.tryb_serwa] + ' • K' + klapaPct + '%', S.tryb_serwa === 2 ? 'warn' : S.tryb_serwa === 3 ? 'err' : '');
    setQuick('qMiesz', S.mieszadlo ? 'WŁĄCZONE' : 'WYŁĄCZONE', S.mieszadlo ? 'ok' : '');
    $('#clockVal').textContent = pad2(new Date().getHours()) + ':' + pad2(new Date().getMinutes());
    $('#clockDesc').textContent = S.rtc_ok ? S.dzien + '.' + S.miesiac + '.' + S.rok : 'RTC niegotowy';
    drawTrend();
  }
  function setQuick(id, text, cls) { const el = $('#' + id); $('span', el).textContent = text; el.className = 'qcard ' + cls; }

  // ───────────────────────── TREND (mini-wykres) ─────────────────────────
  const hist = []; for (let i = 0; i < 60; i++) hist.push({ o: 55 + Math.sin(i / 7) * 4, b: 46 + Math.sin(i / 9) * 2, z: 11 + Math.sin(i / 11) });
  function drawTrend() {
    const c = $('#trend'); const r = c.getBoundingClientRect(); if (!r.width) return;
    const dpr = window.devicePixelRatio || 1; c.width = r.width * dpr; c.height = 80 * dpr;
    const g = c.getContext('2d'); g.scale(dpr, dpr); g.clearRect(0, 0, r.width, 80);
    const series = [['o', '#ff9f43'], ['b', '#ffd166'], ['z', '#00d4f5']];
    series.forEach(([k, col]) => {
      const vals = hist.map(h => h[k]); const mn = Math.min(...vals) - 1, mx = Math.max(...vals) + 1;
      g.beginPath(); vals.forEach((v, i) => { const x = i / (vals.length - 1) * r.width, y = 72 - (v - mn) / (mx - mn) * 60; i ? g.lineTo(x, y) : g.moveTo(x, y); });
      g.strokeStyle = col; g.lineWidth = 1.6; g.lineJoin = 'round'; g.stroke();
      const last = vals[vals.length - 1]; g.fillStyle = col; g.beginPath(); g.arc(r.width - 1, 72 - (last - mn) / (mx - mn) * 60, 2.4, 0, 7); g.fill();
    });
    g.font = '600 8px Roboto,sans-serif'; g.fillStyle = 'rgba(142,166,186,.8)';
    g.fillText('● piec', 2, 10); g.fillStyle = '#ff9f43'; g.fillText('●', 2, 10);
    g.fillStyle = 'rgba(142,166,186,.8)'; g.fillText('● bojler', 36, 10); g.fillStyle = '#ffd166'; g.fillText('●', 36, 10);
    g.fillStyle = 'rgba(142,166,186,.8)'; g.fillText('● zewn.', 78, 10); g.fillStyle = '#00d4f5'; g.fillText('●', 78, 10);
  }

  // ───────────────────────── „ŻYWE” DANE (dryf co 2 s, jak polling 5 s) ─────────────────────────
  let tick = 0;
  function live() {
    tick++;
    if (S.online) {
      S.t_ogrz = real.t_ogrz + Math.sin(tick / 9) * 1.6 + (Math.random() - .5) * .3;
      S.t_ogrz_sr = S.t_ogrz - .9;
      S.t_bojler = real.t_bojler + Math.sin(tick / 15) * .8;
      S.t_zewn = real.t_zewn + Math.sin(tick / 21) * .4;
      S.t_panel = real.t_panel + Math.sin(tick / 12) * 1.2;
      S.t_pokoj = real.t_pokoj + Math.sin(tick / 30) * .2;
      S.wilgotnosc = real.wilgotnosc + Math.sin(tick / 17) * 2;
      S.dym = real.dym + Math.sin(tick / 5) * 25 + Math.random() * 10;
      if (S.dym_alarm) S.dym = S.progAlarmDym + 180 + Math.random() * 60;
      if (S.alarm_ogrzewanie) S.t_ogrz = S.progAlarmTemp + 4 + Math.random();
      if (S.alarm_panel) S.t_panel = S.progAlarmTemp + 6 + Math.random();
      S.dym_wlaczony = S.t_ogrz >= S.dymProgTemp || S.dym_alarm || symAktywna('dym');
      Object.keys(S.sym).forEach(p => { if (tick % 30 === 0) { S.sym[p].min--; if (S.sym[p].min <= 0) delete S.sym[p]; } });
      ['pompa', 'mieszadlo', 'serwo'].forEach(k => { const key = k + '_override_min'; if (S[key] > 0 && tick % 30 === 0) S[key]--; });
      hist.push({ o: S.t_ogrz, b: S.t_bojler, z: S.t_zewn }); if (hist.length > 60) hist.shift();
    }
    render();
  }

  // ───────────────────────── POLECENIA (sendCommand → dispatchCommand) ─────────────────────────
  const toast = $('#toast');
  let toastTimer = 0;
  function showToast(cmd, stage, cls) {
    clearTimeout(toastTimer);
    $('.c', toast).textContent = cmd;
    const ICO_OK = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12.5l4.5 4.5L19 7.5"/></svg>';
    const ICO_ERR = '<svg viewBox="0 0 24 24" width="15" height="15" fill="none" stroke="currentColor" stroke-width="2.6" stroke-linecap="round"><path d="M6 6l12 12M18 6L6 18"/></svg>';
    $('.s', toast).innerHTML = (cls === 'wait' ? '<i class="sp"></i>' : cls === 'ok' ? ICO_OK : ICO_ERR) + '<span>' + stage + '</span>';
    toast.className = 'on ' + cls;
    if (cls !== 'wait') toastTimer = setTimeout(() => toast.className = '', cls === 'ok' ? 2200 : 4500);
  }
  // Jak na WWW ESP: każde polecenie idzie od razu, status pokazuje mały toast (bez okna „Potwierdź”).
  // Gdyby jakieś polecenie miało jednak wymagać potwierdzenia — wpisać jego pierwsze słowo tutaj.
  const POTWIERDZAJ = new Set([]);
  function sendCommand(cmd) {
    if (!POTWIERDZAJ.has(cmd.trim().split(/\s+/)[0])) return dispatch(cmd);
    confirmDlg('Potwierdź polecenie', 'To polecenie zmienia zachowanie sterownika:\n\n<code>' + cmd + '</code>\n\nWysłać?', () => dispatch(cmd));
  }
  function dispatch(cmd) {
    if (!S.online) { showToast(cmd, 'Brak łączności — polecenie nie wysłane', 'err'); return; }
    showToast(cmd, 'Wysyłanie…', 'wait');
    setTimeout(() => showToast(cmd, 'Oczekiwanie na ACK', 'wait'), 500);
    setTimeout(() => {
      const r = apply(cmd);
      if (r === true) { showToast(cmd, 'Potwierdzone przez sterownik', 'ok'); if (navigator.vibrate) navigator.vibrate(12); }
      else showToast(cmd, 'Odrzucone: ' + (r || 'nieznana komenda'), 'err');
      render(); refreshSheet();
    }, 1500);
  }
  // Interpretacja poleceń jak w firmware (parser Firebase cmd)
  function apply(cmd) {
    const p = cmd.trim().split(/\s+/);
    switch (p[0]) {
      case 'pompa_wl': S.pompa = true; S.pompa_override_min = 30; return true;
      case 'pompa_wyl': S.pompa = false; S.pompa_override_min = 30; return true;
      case 'pompa_auto': S.pompa_override_min = 0; S.pompa = S.wybor !== 2 || S.t_ogrz >= S.tempOn; return true;
      case 'mieszadlo_wl': S.mieszadlo = true; S.mieszadlo_override_min = 30; return true;
      case 'mieszadlo_wyl': S.mieszadlo = false; S.mieszadlo_override_min = 30; return true;
      case 'mieszadlo_auto': S.mieszadlo_override_min = 0; S.mieszadlo = false; return true;
      case 'klapa': if (S.tryb_serwa !== 2) return 'serwo nie jest w trybie ręcznym'; S.klapa = Math.round(+p[1] * 1.8); S.serwo_override_min = 60; return true;
      case 'syberek': if (S.tryb_serwa !== 2) return 'serwo nie jest w trybie ręcznym'; S.syberka = Math.round(+p[1] * .9); S.serwo_override_min = 60; return true;
      case 'symuluj': { const pole = p[1]; if (!SYM_POLA[pole]) return 'nieznane pole'; S.sym[pole] = { wartosc: +p[2], min: +p[3] || 60 }; if (pole === 'dym') { S.dym_alarm = +p[2] >= S.progAlarmDym; } return true; }
      case 'symuluj_stop': delete S.sym[p[1]]; if (p[1] === 'dym') S.dym_alarm = false; return true;
      case 'ustaw': {
        const k = p[1], v = +p[2];
        if (!(k in S) || typeof S[k] === 'object') return 'nieznane pole ' + k;
        if (typeof S[k] === 'boolean') S[k] = v === 1; else S[k] = v;
        if (k === 'trybSerwa') { S.tryb_serwa = v; if (v !== 2) S.serwo_override_min = 0; if (v === 3) { S.klapa = 0; S.syberka = 0; } }
        return true;
      }
      default: return 'nieznana komenda';
    }
  }
  S.trybSerwa = S.tryb_serwa; // lustro ustawienia

  // ───────────────────────── DIALOG ─────────────────────────
  function confirmDlg(title, html, onOk) {
    const d = $('#dlg'); $('h4', d).textContent = title; $('p', d).innerHTML = html; d.classList.add('on');
    $('.ok', d).onclick = () => { d.classList.remove('on'); onOk(); };
    $('.cancel', d).onclick = () => d.classList.remove('on');
  }

  // ───────────────────────── ARKUSZ (showSheet) ─────────────────────────
  let sheetBuilder = null;
  function showSheet(title, icon, builder) {
    sheetBuilder = builder;
    $('#sheet h3').textContent = title;
    $('#sheet .hd .ic').innerHTML = ILU.nav[icon] || ILU.nav.settings;
    const body = $('#sheet .body'); body.innerHTML = ''; body.appendChild(builder());
    $('#scrim').classList.add('on'); $('#sheet').classList.add('on');
  }
  // odśwież tylko wartości (bez przebudowy, żeby nie gubić wpisywanych liczb) — jak ESP symZaladujStan()
  function refreshSheet() { if (!sheetBuilder) return; $$('[data-live]', $('#sheet')).forEach(el => { const f = liveFns[el.dataset.live]; if (f) f(el); }); }
  const liveFns = {};
  let liveId = 0;
  const live$ = (el, fn) => { const id = 'l' + (++liveId); el.dataset.live = id; liveFns[id] = fn; fn(el); return el; };
  function dismissSheet() { $('#scrim').classList.remove('on'); $('#sheet').classList.remove('on'); sheetBuilder = null; }
  $('#scrim').addEventListener('click', dismissSheet); $('#sheetClose').addEventListener('click', dismissSheet);

  // ── komponenty paneli (1:1 z ESP: .ust-sekcja / .seg / .row+ust-inp+Ustaw / checkbox / symulacja) ──
  const h = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };
  const note = t => h('div', 'note', t);
  const sectionHeader = t => h('div', 'ust-sekcja', '<b>' + t + '</b><i></i>');
  function seg(options, current, onPick) {
    const s = h('div', 'seg');
    options.forEach(([v, t]) => { const b = h('button', v === current ? 'on' : '', t); b.onclick = () => { $$('button', s).forEach(x => x.classList.toggle('on', x === b)); onPick(v); }; s.appendChild(b); });
    return s;
  }
  function numInput(name, desc, key, min, max) {
    const r = h('div', 'row', '<div class="l"><b>' + name + '</b>' + (desc ? '<span>' + desc + '</span>' : '') + '</div>');
    const inp = h('input', 'inp'); inp.type = 'number'; inp.min = min; inp.max = max; inp.value = S[key];
    live$(inp, el => { if (document.activeElement !== el) el.value = S[key]; });
    const b = h('button', 'ust-btn', 'Ustaw');
    b.onclick = () => { const v = parseInt(inp.value, 10); if (isNaN(v) || v < min || v > max) { showToast('ustaw ' + key, name + ': dozwolony zakres ' + min + '–' + max, 'err'); return; } sendCommand('ustaw ' + key + ' ' + v); };
    r.appendChild(inp); r.appendChild(b); return r;
  }
  function checkbox(name, key) {
    const l = h('label', 'chk'); const c = h('input'); c.type = 'checkbox'; c.checked = !!S[key];
    live$(c, el => el.checked = !!S[key]);
    c.onchange = () => sendCommand('ustaw ' + key + ' ' + (c.checked ? 1 : 0));
    l.appendChild(c); l.appendChild(document.createTextNode(name)); return l;
  }
  function manualRow(prefix) {
    const r = h('div', 'btnrow');
    [['Wymuś WŁ.', prefix + '_wl'], ['Wymuś WYŁ.', prefix + '_wyl'], ['Auto', prefix + '_auto']].forEach(([t, c]) => { const b = h('button', 'ust-btn', t); b.onclick = () => sendCommand(c); r.appendChild(b); });
    return r;
  }
  function slider(name, min, max, step, value, fmtFn, onChange, onCommit) {
    const w = h('div', 'slider', '<div class="h"><span>' + name + '</span><b></b></div>');
    const i = h('input'); i.type = 'range'; i.min = min; i.max = max; i.step = step; i.value = value;
    const b = $('b', w); const upd = () => { b.textContent = fmtFn(+i.value); if (onChange) onChange(+i.value); };
    i.oninput = upd; if (onCommit) i.onchange = () => onCommit(+i.value); upd(); w.appendChild(i); w.get = () => +i.value; return w;
  }
  function symulacjaSekcja(pole, etykieta, jedn, min, max, krok, key) {
    const wrap = h('div'); wrap.appendChild(sectionHeader('Symulacja — ' + etykieta));
    const toggle = h('button', 'sym-toggle', 'Symulacja'); const panel = h('div', 'sym-panel'); const info = h('div', 'sym-info');
    const paint = on => { toggle.classList.toggle('on', on); panel.classList.toggle('on', on); };
    toggle.onclick = () => paint(!panel.classList.contains('on'));
    const f = v => krok >= 1 ? String(Math.round(v)) : fmt1(v);
    const w = slider('Wartość', min, max, krok, val(pole, key), v => f(v) + ' ' + jedn);
    const c = slider('Czas trwania', 5, 180, 5, 60, v => v + ' min');
    panel.appendChild(w); panel.appendChild(c);
    const btns = h('div', 'btnrow'); const z = h('button', 'ust-btn', 'Zastosuj'); const x = h('button', 'ust-btn', 'Wyłącz teraz');
    z.onclick = () => sendCommand('symuluj ' + pole + ' ' + f(w.get()) + ' ' + c.get()); x.onclick = () => sendCommand('symuluj_stop ' + pole);
    btns.appendChild(z); btns.appendChild(x); panel.appendChild(btns);
    wrap.appendChild(toggle); wrap.appendChild(panel); wrap.appendChild(info);
    live$(info, el => { el.textContent = symAktywna(pole) ? 'Aktywna — jeszcze ok. ' + S.sym[pole].min + ' min' : ''; });
    paint(symAktywna(pole));
    return wrap;
  }

  // ── panele = dpOtworz*() ──
  const MENUS = {
    zewn: () => showSheet('Temperatura zewnętrzna', 'outside', () => symulacjaSekcja('zewn', 'Zewnętrzna', '°C', -30, 50, .5, 't_zewn')),
    bojler: () => showSheet('Bojler', 'thermo', () => symulacjaSekcja('bojler', 'Bojler', '°C', 0, 160, .5, 't_bojler')),
    pokoj: () => showSheet('Pomieszczenie', 'thermo', () => symulacjaSekcja('pokoj', 'Pomieszczenie', '°C', -50, 50, .5, 't_pokoj')),
    cisnienie: () => showSheet('Ciśnienie', 'settings', () => symulacjaSekcja('cisnienie', 'Ciśnienie', 'hPa', 970, 1040, 1, 'cisnienie')),
    wilgotnosc: () => showSheet('Wilgotność', 'settings', () => symulacjaSekcja('wilgotnosc', 'Wilgotność', '%', 0, 100, 1, 'wilgotnosc')),
    ogrz: () => overheat(false), panel: () => overheat(true),
    pompa: () => showSheet('Pompa', 'pump', () => {
      const b = h('div');
      b.appendChild(note('Aktywna strategia pracy pompy'));
      b.appendChild(seg([[1, 'Trociniak'], [2, 'Kopciuch'], [3, 'Automatyczny']], S.wybor, v => sendCommand('ustaw wybor ' + v)));
      b.appendChild(manualRow('pompa'));
      b.appendChild(live$(h('div', 'sym-info'), el => el.textContent = S.pompa_override_min > 0 ? 'Wymuszenie ręczne jeszcze ok. ' + S.pompa_override_min + ' min, potem powrót do strategii' : ''));
      b.appendChild(sectionHeader('Tryb czasowy (Trociniak)'));
      b.appendChild(numInput('Czas ON', 'Minuty pracy w cyklu', 'czasOn', 1, 180)); b.appendChild(numInput('Czas OFF', 'Minuty postoju w cyklu', 'czasOff', 1, 180));
      b.appendChild(sectionHeader('Tryb temperaturowy (Kopciuch)'));
      b.appendChild(numInput('Temp ON', '°C pieca — start pompy', 'tempOn', 0, 100)); b.appendChild(numInput('Temp OFF', '°C pieca — stop pompy', 'tempOff', 0, 100));
      b.appendChild(sectionHeader('Antystop'));
      b.appendChild(checkbox('Włączony', 'antystopWlaczony')); b.appendChild(numInput('Dni bez ruchu', 'Wymuszony puls 45s po X dniach', 'antystopDni', 1, 45));
      return b;
    }),
    serwo: () => showSheet('Serwo', 'servo', () => {
      const b = h('div'); b.appendChild(note('Tryb pracy klapy i syberka'));
      const manual = h('div', 'manual' + (S.tryb_serwa === 2 ? ' on' : ''));
      b.appendChild(seg([[1, 'Auto'], [2, 'Ręczny'], [3, 'Bezpieczna']], S.tryb_serwa, v => { manual.classList.toggle('on', v === 2); sendCommand('ustaw trybSerwa ' + v); }));
      manual.appendChild(h('h4', '', 'Ręczna pozycja'));
      manual.appendChild(slider('Klapa', 0, 100, 1, Math.round(S.klapa / 1.8), v => v + '%', null, v => sendCommand('klapa ' + v)));
      manual.appendChild(slider('Syberek', 0, 100, 1, Math.round(S.syberka / .9), v => v + '%', null, v => sendCommand('syberek ' + v)));
      manual.appendChild(live$(h('div', 'sym-info'), el => el.textContent = S.serwo_override_min > 0 ? 'Ręczna pozycja jeszcze ok. ' + S.serwo_override_min + ' min, potem powrót do Auto' : ''));
      b.appendChild(manual);
      b.appendChild(sectionHeader('Automatyka (tryb Auto)'));
      b.appendChild(numInput('Temperatura zadana', '°C — punkt odniesienia', 'tempZadServo', 0, 100));
      b.appendChild(numInput('Histereza', '°C — pasmo bez reakcji', 'histerServo', 0, 50));
      b.appendChild(numInput('Skok klapy', '% na jedno wywołanie', 'skokKlapy', 1, 100));
      b.appendChild(numInput('Skok syberka', '% na jedno wywołanie', 'skokSyberka', 1, 100));
      b.appendChild(numInput('Odchylenie przyspieszające', '°C — powyżej: krok × mnożnik', 'odchylTemp', 1, 50));
      b.appendChild(numInput('Mnożnik korekty', 'Mnożnik kroku przy dużym odchyleniu', 'mnoznik', 1, 10));
      return b;
    }),
    mieszadlo: () => showSheet('Mieszadło', 'mixer', () => {
      const b = h('div');
      b.appendChild(checkbox('Włączone', 'mieszadloWlaczony')); b.appendChild(manualRow('mieszadlo'));
      b.appendChild(live$(h('div', 'sym-info'), el => el.textContent = S.mieszadlo_override_min > 0 ? 'Ręczny override jeszcze ok. ' + S.mieszadlo_override_min + ' min, potem powrót do automatyki' : ''));
      b.appendChild(sectionHeader('Cykl pracy'));
      b.appendChild(numInput('Czas ON', 's — jak długo przekaźnik załączony', 'mieszadloCzasOn', 1, 255));
      b.appendChild(numInput('Czas OFF', 'min — przerwa między cyklami', 'mieszadloCzasOff', 1, 180));
      return b;
    }),
    dym: () => showSheet('Czujnik dymu', 'shield', () => {
      const b = h('div');
      b.appendChild(numInput('Próg alarmu', 'Surowy odczyt ADC czujnika (0-4095)', 'progAlarmDym', 0, 4095));
      b.appendChild(sectionHeader('Aktywacja od temperatury pieca'));
      b.appendChild(numInput('Próg temperatury', '°C — poniżej tej temp. pieca czujnik jest wyłączony (histereza 3°C)', 'dymProgTemp', 0, 200));
      b.appendChild(h('div', 'row', '<div class="l"><b>Tryb pracy po aktywacji</b></div>'));
      b.appendChild(seg([[0, 'Impulsowo'], [1, 'Ciągle']], S.dymTrybPracy, v => sendCommand('ustaw dymTrybPracy ' + v)));
      b.appendChild(sectionHeader('Cykl pracy czujnika (tryb Impulsowo)'));
      b.appendChild(numInput('Czas WŁ.', 's — rozgrzewanie + pomiar', 'dymCzasOn', 20, 600));
      b.appendChild(numInput('Czas WYŁ.', 's — między pomiarami', 'dymCzasOff', 10, 600));
      b.appendChild(numInput('Czas stabilizacji', 's — część Czasu WŁ. zanim odczyt zaufany (dotyczy obu trybów)', 'dymCzasStabilizacji', 5, 590));
      b.appendChild(symulacjaSekcja('dym', 'Dym', 'ADC (surowy odczyt)', 0, 4095, 1, 'dym'));
      return b;
    }),
    czas: () => showSheet('Data i czas', 'clock', () => {
      const b = h('div'); const d = new Date();
      b.appendChild(h('div', 'note', 'CZAS TELEFONU'));
      b.appendChild(live$(h('div', 'bigtime'), el => { const n = new Date(); el.textContent = pad2(n.getDate()) + '.' + pad2(n.getMonth() + 1) + '.' + n.getFullYear() + ' ' + pad2(n.getHours()) + ':' + pad2(n.getMinutes()) + ':' + pad2(n.getSeconds()); }));
      b.appendChild(sectionHeader('Zegar sterownika (RTC)'));
      b.appendChild(h('div', 'row', '<div class="l"><b>' + (S.rtc_ok ? 'RTC gotowy' : 'RTC niegotowy') + '</b><span>data w sterowniku: ' + S.dzien + '.' + S.miesiac + '.' + S.rok + '</span></div>'));
      const br = h('div', 'btnrow'); const sync = h('button', 'ust-btn', 'Synchronizuj z telefonem'); sync.onclick = () => sendCommand('ustaw rtc ' + Math.floor(Date.now() / 1000)); br.appendChild(sync); b.appendChild(br);
      return b;
    })
  };
  function overheat(panel) {
    showSheet(panel ? 'Próg przegrzania — Panel słoneczny' : 'Próg przegrzania — Piec C.O.', 'thermo', () => {
      const b = h('div');
      b.appendChild(note('Wspólny próg dla pieca C.O. i panelu słonecznego — histereza: piec -10°C, panel -4°C.'));
      b.appendChild(numInput('Próg alarmu przegrzania', '°C', 'progAlarmTemp', 0, 100));
      b.appendChild(panel ? symulacjaSekcja('panel', 'Panel słoneczny', '°C', -30, 160, .5, 't_panel') : symulacjaSekcja('ogrz', 'Piec C.O.', '°C', 0, 160, .5, 't_ogrz'));
      return b;
    });
  }
  S.rtc = 0;
  function openMenu(id) { (MENUS[id] || (() => {}))(); }

  // ───────────────────────── NAWIGACJA ─────────────────────────
  const PAGES = ['pulpit', 'wykresy', 'pogoda', 'ustawienia', 'wiecej'];
  function navigate(i) {
    PAGES.forEach((p, k) => $('#pg-' + p).classList.toggle('on', k === i));
    $$('.nav button').forEach((b, k) => b.classList.toggle('on', k === i));
    if (i === 0) drawTrend();
  }
  $$('.nav button').forEach((b, i) => b.onclick = () => navigate(i));

  // ───────────────────────── DEMO (scenariusze) ─────────────────────────
  function buildDemo() {
    const d = $('#demo');
    const chk = (label, get, set) => { const l = h('label'); const c = h('input'); c.type = 'checkbox'; c.checked = get(); c._get = get; c.onchange = () => { set(c.checked); render(); }; l.appendChild(c); l.appendChild(document.createTextNode(label)); return l; };
    const rng = (label, key, min, max, step, unit) => { const w = h('div', 'rng', '<span>' + label + '<b></b></span>'); const i = h('input'); i.type = 'range'; i.min = min; i.max = max; i.step = step; i.value = real[key]; const b = $('b', w); const u = () => { b.textContent = i.value + ' ' + unit; }; i.oninput = () => { real[key] = +i.value; S[key] = +i.value; u(); render(); }; u(); w.appendChild(i); return w; };
    d.appendChild(h('h3', '', 'SCENARIUSZE DEMO <button id="demoX" aria-label="Zamknij">' + ILU.nav.close + '</button>'));
    d.appendChild(h('p', '', 'Przełączniki zmieniają makietę statusu, żeby obejrzeć każdy stan kafelków i paneli. W aplikacji te stany przychodzą z Firebase.'));
    d.appendChild(h('h5', '', 'Łączność i pora'));
    d.appendChild(chk('Sterownik online (LIVE)', () => S.online, v => S.online = v));
    d.appendChild(chk('Noc (księżyc na kafelku Zewnętrzna)', () => S.night, v => S.night = v));
    d.appendChild(h('h5', '', 'Urządzenia'));
    d.appendChild(chk('Pompa pracuje', () => S.pompa, v => S.pompa = v));
    d.appendChild(chk('Mieszadło pracuje', () => S.mieszadlo, v => S.mieszadlo = v));
    d.appendChild(chk('Rozpalanie (override mieszadła)', () => S.rozpalanie, v => S.rozpalanie = v));
    d.appendChild(h('p', '', 'Tryb serwa:'));
    d.appendChild(seg([[1, 'Auto'], [2, 'Ręczny'], [3, 'Bezp.']], S.tryb_serwa, v => { S.tryb_serwa = v; S.trybSerwa = v; render(); }));
    d.appendChild(h('h5', '', 'Alarmy i czujniki'));
    d.appendChild(chk('ALARM dymu', () => S.dym_alarm, v => S.dym_alarm = v));
    d.appendChild(chk('ALARM przegrzania pieca', () => S.alarm_ogrzewanie, v => S.alarm_ogrzewanie = v));
    d.appendChild(chk('ALARM przegrzania panelu', () => S.alarm_panel, v => S.alarm_panel = v));
    d.appendChild(chk('Czujnik dymu: ostatni pomiar nieświeży', () => !S.dym_swiezy, v => S.dym_swiezy = !v));
    d.appendChild(chk('Symulacja bojlera (65 °C, 45 min)', () => symAktywna('bojler'), v => { if (v) S.sym.bojler = { wartosc: 65, min: 45 }; else delete S.sym.bojler; }));
    d.appendChild(h('h5', '', 'Odczyty'));
    d.appendChild(rng('Piec C.O.', 't_ogrz', 15, 95, 1, '°C'));
    d.appendChild(rng('Bojler', 't_bojler', 10, 80, 1, '°C'));
    d.appendChild(rng('Zewnętrzna', 't_zewn', -20, 35, 1, '°C'));
    d.appendChild(rng('Ciśnienie', 'cisnienie', 970, 1040, 1, 'hPa'));
    d.appendChild(rng('Wilgotność', 'wilgotnosc', 0, 100, 1, '%'));
    d.appendChild(rng('Dym (ADC)', 'dym', 0, 4095, 10, ''));
    d.appendChild(h('h5', '', 'Ekran'));
    d.appendChild(seg([[360, '360'], [412, '412'], [480, '480'], [768, 'tablet']], 412, v => { document.documentElement.style.setProperty('--frame-w', v + 'px'); document.documentElement.style.setProperty('--cols', v >= 720 ? 4 : v >= 520 ? 3 : 2); setTimeout(drawTrend, 50); }));
    $('#demoX', d).onclick = () => d.classList.remove('on');
    $('#demoBtn').onclick = () => { $$('input[type=checkbox]', d).forEach(c => { if (c._get) c.checked = !!c._get(); }); d.classList.toggle('on'); };
  }

  // ───────────────────────── START ─────────────────────────
  function init() {
    document.body.insertAdjacentHTML('afterbegin', ILU.defs);
    $$('[data-ico]').forEach(el => { el.innerHTML = ILU.nav[el.dataset.ico] || ''; });
    $('#heroBoiler').innerHTML = ILU.heroBoiler.svg();
    const cw = $('#chartsIco'), ck = $('#clockIco'); cw.innerHTML = ILU.wykresy.svg(); ck.innerHTML = ILU.czas.svg();
    buildTiles(); buildDemo();
    $('#alarmBanner').onclick = () => openMenu(S.dym_alarm ? 'dym' : S.alarm_ogrzewanie ? 'ogrz' : 'panel');
    $('#qPompa').onclick = () => openMenu('pompa'); $('#qSerwo').onclick = () => openMenu('serwo'); $('#qMiesz').onclick = () => openMenu('mieszadlo');
    $('#cardCharts').onclick = () => navigate(1); $('#cardClock').onclick = () => openMenu('czas');
    $$('[data-menu]').forEach(el => el.onclick = () => openMenu(el.dataset.menu));
    render();
    setInterval(live, 2000);
    setInterval(() => { ILU.czas.update(ck); refreshSheet(); }, 250);
    window.addEventListener('resize', drawTrend);
  }
  document.addEventListener('DOMContentLoaded', init);
})();
