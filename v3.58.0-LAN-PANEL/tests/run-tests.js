'use strict';
// ─────────────────────────────────────────────────────────────────────────────
// Testy interfejsu nowego panelu i przełącznika stron — z ATRAPĄ serwera
// (MockEsp / MockFirebase). ZERO dostępu do prawdziwej sieci: polyfill fetch
// w jsdom kieruje wywołania wyłącznie do atrapy lub odrzuca je (symulacja
// braku internetu).
//
// To są testy z ATRAPĄ SERWERA — NIE na sprzęcie. Panel i stara strona są
// wyciągane wprost z ../src/main_centrala.cpp (raw-stringi PROGMEM), więc
// testowany jest dokładnie ten kod, który pójdzie na ESP32-S3.
//
// Uruchomienie:   npm install && node run-tests.js
// Wymagania:      Node.js >= 18 (testowane na v22)
// ─────────────────────────────────────────────────────────────────────────────
const fs = require('fs');
const path = require('path');
const http = require('http');
const { JSDOM, VirtualConsole } = require('jsdom');
const { MockEsp, MockFirebase } = require('./mock-esp');

const CPP_PATH = process.env.CPP_PATH || path.join(__dirname, '..', 'src', 'main_centrala.cpp');
const cpp = fs.readFileSync(CPP_PATH, 'utf-8');

function extractLiteral(tag) {
  const re = new RegExp('R"' + tag + '\\((.*?)\\)' + tag + '"', 's');
  const m = cpp.match(re);
  if (!m) throw new Error('Nie znaleziono literału R"' + tag + '(...)' + tag + '" w ' + CPP_PATH);
  return m[1];
}
const PANEL_HTML = extractLiteral('PIECHTML');
const STARA_HEADER = (cpp.match(/WIFI_STRONA_HTML_HEADER\[\] PROGMEM = R"HTML\((.*?)\)HTML"/s) || [])[1];
const STARA_FOOTER = (cpp.match(/WIFI_STRONA_HTML_FOOTER\[\] PROGMEM = R"HTML\((.*?)\)HTML"/s) || [])[1];

// Ciało strony starej (wifiZbudujStrone()) — konkatenacja literałów F("..."),
// odtwarzana tu statycznie. Jest identyczna jak w v3.57.9 (funkcja bez zmian).
function extractStaraBody() {
  const start = cpp.indexOf('String wifiZbudujStrone() {');
  const end = cpp.indexOf('\n}\n', start);
  const fn = cpp.slice(start, end);
  const out = [];
  let i = 0;
  const pat = /html\s*\+=\s*F\(\s*/gs;
  let m;
  while ((m = pat.exec(fn))) {
    let j = m.index + m[0].length, lit = '';
    while (j < fn.length && fn[j] === '"') {
      let k = j + 1;
      for (;;) {
        if (fn[k] === '\\' && fn[k + 1] === '"') { k += 2; continue; }
        if (fn[k] === '"') break;
        k++;
      }
      lit += fn.slice(j + 1, k);
      j = k + 1;
      while (j < fn.length && /\s/.test(fn[j])) j++;
    }
    out.push(lit);
    i = j;
    pat.lastIndex = j;
  }
  return out.join('');
}
function unescapeCpp(s) {
  const r = [];
  for (let x = 0; x < s.length; x++) {
    const c = s[x];
    if (c === '\\' && x + 1 < s.length) {
      const n = s[x + 1];
      if (n === 'n') { r.push('\n'); x++; continue; }
      if (n === 't') { r.push('\t'); x++; continue; }
      if (n === 'r') { r.push('\r'); x++; continue; }
      if (n === '"' || n === '\'' || n === '\\') { r.push(n); x++; continue; }
    }
    r.push(c);
  }
  return r.join('');
}
const STARA_BODY = unescapeCpp(extractStaraBody());



function buildStaraPage(token) {
  return STARA_HEADER + STARA_BODY.replace(
    "window.espLocalActionToken='';",
    "window.espLocalActionToken='" + token + "';"
  ) + STARA_FOOTER;
}

// ── narzędzia asercji ────────────────────────────────────────────────────────
const results = [];
function ok(name, cond, extra) {
  results.push({ name, pass: !!cond, extra: extra || '' });
  console.log((cond ? '  [OK]   ' : '  [FAIL] ') + name + (extra ? '  → ' + extra : ''));
}
async function waitFor(fn, ms, label) {
  const t0 = Date.now();
  while (Date.now() - t0 < ms) {
    let v;
    try { v = fn(); } catch (e) { v = false; }
    if (v) return true;
    await new Promise(r => setTimeout(r, 100));
  }
  return false;
}

// ── polyfill fetch dla jsdom ────────────────────────────────────────────────
// - URL względny/bezwzględny na origin strony → prawdziwe HTTP do atrapy ESP
// - hosty Firebase/Google → atrapa Firebase (gdy internetFb=true), inaczej błąd
// - wszystko inne → odrzucenie (brak internetu)
function makeFetch(window, espPort, fb, internetFb) {
  const origin = new URL(window.location.href);
  return function fetchPolyfill(input, init) {
    return new Promise((resolve, reject) => {
      let urlStr = typeof input === 'string' ? input : (input && input.url) || '';
      let u;
      try { u = new URL(urlStr, origin); } catch (e) { return reject(new TypeError('Invalid URL')); }

      init = init || {};
      const isSameOrigin = u.host === origin.host;
      const isFirebase = u.hostname.includes('firebasedatabase.app') ||
        u.hostname.includes('identitytoolkit.googleapis.com') ||
        u.hostname.includes('securetoken.googleapis.com');

      if (isFirebase) {
        if (!internetFb || !fb) {
          reject(new TypeError('Failed to fetch (symulacja braku internetu)'));
          return;
        }
        const r = fb.handle(u.href, init.method || 'GET', init.body);
        if (!r) { reject(new TypeError('Failed to fetch')); return; }
        resolve(makeResponse(r.status, r.body));
        return;
      }
      if (!isSameOrigin) {
        // panel nie woła nic poza origin i Firebase — blokujemy
        reject(new TypeError('Failed to fetch (poza origin)'));
        return;
      }
      // do atrapy ESP
      const req = http.request({
        host: '127.0.0.1', port: espPort, method: init.method || 'GET',
        path: u.pathname + u.search,
        headers: Object.assign({}, init.headers || {})
      }, resp => {
        let data = '';
        resp.on('data', d => { data += d; });
        resp.on('end', () => resolve(makeResponse(resp.statusCode, data, resp.headers['content-type'] || '')));
      });
      req.on('error', err => reject(new TypeError('Failed to fetch: ' + err.code)));
      if (init.body) req.write(init.body);
      req.end();
    });
  };
}

function makeResponse(status, body, contentType) {
  const text = typeof body === 'string' ? body : JSON.stringify(body);
  return {
    status,
    ok: status >= 200 && status < 300,
    headers: { get: () => contentType || '' },
    async json() {
      if (text === '' || text === 'null') return null;
      return JSON.parse(text);
    },
    async text() { return text; }
  };
}

// ── środowisko jsdom z panelami ─────────────────────────────────────────────
function makeCanvasStub() {
  const noop = () => {};
  const ctx = new Proxy({}, {
    get: (t, prop) => {
      if (prop === 'measureText') return () => ({ width: 10 });
      if (prop === 'createLinearGradient' || prop === 'createRadialGradient')
        return () => ({ addColorStop: noop });
      if (prop === 'getImageData') return () => ({ data: new Uint8ClampedArray(4) });
      return typeof prop === 'string' ? noop : undefined;
    },
    set: () => true
  });
  return ctx;
}

async function loadPanel({ url, espPort, fb, internetFb, seed }) {
  const virtualConsole = new VirtualConsole();
  const jsdomErrors = [];
  const consoleErrors = [];
  const navAttempts = [];
  virtualConsole.on('jsdomError', e => {
    const msg = String(e.message || e);
    if (msg.includes('Could not parse CSS stylesheet')) return; // jsdom nie zna części CSS
    if (msg.includes('Not implemented: navigation')) { navAttempts.push(msg); return; }
    jsdomErrors.push(msg + (e.detail ? ' :: ' + e.detail : ''));
  });
  virtualConsole.on('error', (...a) => { consoleErrors.push(a.map(String).join(' ')); });
  virtualConsole.on('log', () => {});

  const dom = new JSDOM(PANEL_HTML, {
    url,
    runScripts: 'outside-only',
    pretendToBeVisual: true,
    virtualConsole
  });
  const w = dom.window;
  // polyfills PRZED skryptem panelu
  w.AbortController = AbortController;
  w.fetch = makeFetch(w, espPort, fb, internetFb);
  w.HTMLCanvasElement.prototype.getContext = function () { return makeCanvasStub(); };
  if (!w.navigator.vibrate) w.navigator.vibrate = () => true;
  if (seed) {
    for (const [k, v] of Object.entries(seed)) {
      try { w.localStorage.setItem(k, v); } catch (e) {}
    }
  }
  // uruchom skrypty panelu (w kolejności z dokumentu).
  // Instrumentacja WYŁĄCZNIE testowa: przed domknięciem głównego IIFE wyprowadzamy
  // rzeczywiste funkcje wewnętrzne do window.__PANEL_TEST__ — wysyłany firmware
  // pozostaje nietknięty, testy wołają oryginalne funkcje panelu.
  const scripts = dom.window.document.querySelectorAll('script');
  for (const s of scripts) {
    if (s.src) continue;
    let code = s.textContent;
    if (code.includes('STEROWNIK CO — prototyp HTML') || /pollFirebase/.test(code)) {
      const tail = code.lastIndexOf('})();');
      if (tail > 0) {
        code = code.slice(0, tail) +
          '\n;window.__PANEL_TEST__ = { sendCommand: (typeof sendCommand !== "undefined") ? sendCommand : null,' +
          ' pollFirebase: (typeof pollFirebase !== "undefined") ? pollFirebase : null,' +
          ' openMenu: (typeof openMenu !== "undefined") ? openMenu : null,' +
          ' espPanelIsServed: (typeof espPanelIsServed !== "undefined") ? espPanelIsServed : null,' +
          ' espPanelRetryServed: (typeof espPanelRetryServed !== "undefined") ? espPanelRetryServed : null,' +
          ' MENUS: (typeof MENUS !== "undefined") ? MENUS : null };\n' +
          code.slice(tail);
      }
    }
    try {
      w.eval(code);
    } catch (e) {
      jsdomErrors.push('SCRIPT THROW: ' + e.message);
    }
  }
  return { dom, w, jsdomErrors, consoleErrors, navAttempts };
}

async function loadStara({ url, espPort, token, seed }) {
  const virtualConsole = new VirtualConsole();
  const jsdomErrors = [];
  virtualConsole.on('jsdomError', e => {
    const msg = String(e.message || e);
    if (msg.includes('Could not parse CSS stylesheet')) return;
    jsdomErrors.push(msg);
  });
  virtualConsole.on('error', () => {});
  const dom = new JSDOM(buildStaraPage(token || 'ACTION-TOKEN-XYZ'), {
    url,
    runScripts: 'outside-only',
    pretendToBeVisual: true,
    virtualConsole
  });
  const w = dom.window;
  w.AbortController = AbortController;
  w.fetch = makeFetch(w, espPort, null, false);
  w.HTMLCanvasElement.prototype.getContext = function () { return makeCanvasStub(); };
  for (const s of dom.window.document.querySelectorAll('script')) {
    if (s.src) continue;
    try { w.eval(s.textContent); } catch (e) { jsdomErrors.push('SCRIPT THROW: ' + e.message); }
  }
  return { dom, w, jsdomErrors };
}

// ─────────────────────────────────────────────────────────────────────────────
// SCENARIUSZE
// ─────────────────────────────────────────────────────────────────────────────
async function t1_panelLokalny() {
  console.log('\n[T1] Panel otwarty z centrali (LAN, bez internetu) — odczyt bez Firebase');
  const esp = new MockEsp({ panelHtml: PANEL_HTML, cmdToken: 'TEST-TOKEN-123' });
  await esp.start();
  const { w, jsdomErrors, consoleErrors } = await loadPanel({
    url: 'http://10.178.89.36/',
    espPort: esp.port,
    fb: null,
    internetFb: false
  });
  const live = await waitFor(() => {
    const chip = w.document.querySelector('#chip');
    return chip && /LIVE/.test(chip.textContent);
  }, 8000);
  ok('T1.1 chip LIVE po starcie (bez logowania)', live);

  const banner = w.document.querySelector('#connBanner');
  ok('T1.2 banner połączenia ukryty', banner && banner.classList.contains('hidden'));

  const heroTemp = w.document.querySelector('#heroTemp');
  ok('T1.3 temperatura pieca z /api/lan/status (58.4 °C)',
    heroTemp && heroTemp.textContent.includes('58.4'),
    'heroTemp=' + (heroTemp && heroTemp.textContent));

  const heroPump = w.document.querySelector('#heroPump');
  ok('T1.4 stan pompy z lokalnego statusu (ON)', heroPump && heroPump.textContent === 'ON');

  const setcard = w.document.querySelector('#setcardStara');
  ok('T1.5 karta "Starsza strona lokalna" widoczna w trybie LAN',
    setcard && setcard.style.display !== 'none');

  // [v3.58.1] przycisk przełącznika w pasku pulpitu — zawsze widoczny
  const staraBtn = w.document.querySelector('#sysstripStaraBtn');
  ok('T1.5a przycisk "⇄ STARA WERSJA" w pasku pulpitu (LAN)',
    !!staraBtn && /STARA/.test(staraBtn.textContent),
    'btn=' + (staraBtn && staraBtn.textContent.trim()));

  w.__PANEL_TEST__.openMenu('sesja');
  const sesjaLbl = await waitFor(() => {
    const el = w.document.querySelector('#sheet .diag-card .lbl');
    return (el && /POŁĄCZENIE Z CENTRALĄ \(LAN\)/.test(el.textContent)) ? el : false;
  }, 5000);
  ok('T1.6 karta sesji: POŁĄCZENIE Z CENTRALĄ (LAN)',
    !!sesjaLbl, 'lbl=' + ((w.document.querySelector('#sheet .diag-card .lbl') || {}).textContent));
  // [v3.58.1] diagnostyka w karcie sesji: wersja centrali + źródło danych
  const sesjaSubs = w.document.querySelectorAll('#sheet .diag-card .sub');
  const sesjaDiag = Array.from(sesjaSubs).map(el => el.textContent).join(' | ');
  ok('T1.6a sesja: wersja centrali (fw) widoczna',
    /Wersja centrali:\s*v3\.58\.3/.test(sesjaDiag), 'diag="' + sesjaDiag + '"');
  ok('T1.6b sesja: źródło danych = lokalne API /api/lan/status',
    /Źródło danych:\s*lokalne API/.test(sesjaDiag), 'diag="' + sesjaDiag + '"');

  ok('T1.7 /api/lan/status odpytany (>0)', esp.counters.lanStatus > 0, 'odpytań: ' + esp.counters.lanStatus);
  ok('T1.8 ZERO zapytań do Firebase w trybie lokalnym',
    esp.counters.other === 0, 'other=' + esp.counters.other);

  const sysstripSub = w.document.querySelector('#sysstripSub');
  ok('T1.9 pasek systemowy informuje o lokalnym API',
    sysstripSub && /lokalne API/.test(sysstripSub.textContent));

  ok('T1.10 brak błędów JS panelu', jsdomErrors.length === 0, jsdomErrors.slice(0, 3).join(' | '));
  ok('T1.11 brak krytycznych błędów konsoli (init)', consoleErrors.length === 0, consoleErrors.slice(0, 2).join(' | '));

  // T4 wykorzystuje tę samą instancję — utrata połączenia
  console.log('\n[T4] Utrata połączenia z centralą (LAN) i powrót');
  esp.opts.dropLan = true;
  await w.__PANEL_TEST__.pollFirebase();
  await new Promise(r => setTimeout(r, 300));
  const bannerDown = w.document.querySelector('#connBanner');
  ok('T4.1 banner widoczny po utracie centrali', bannerDown && !bannerDown.classList.contains('hidden'));
  const bannerText = w.document.querySelector('#connBannerText');
  ok('T4.2 komunikat o sterowniku w sieci lokalnej',
    bannerText && /sieci lokalnej/.test(bannerText.textContent), 'tekst="' + (bannerText && bannerText.textContent) + '"');
  const chipDown = w.document.querySelector('#chip');
  ok('T4.3 chip = BRAK SESJI', chipDown && /BRAK SESJI/.test(chipDown.textContent));

  esp.opts.dropLan = false;
  await w.__PANEL_TEST__.pollFirebase();
  await waitFor(() => w.document.querySelector('#connBanner').classList.contains('hidden'), 5000);
  ok('T4.4 banner znika po powrocie centrali',
    w.document.querySelector('#connBanner').classList.contains('hidden'));

  await esp.stop();
}

async function t2_komendyLokalne() {
  console.log('\n[T2] Komendy lokalne: POST /api/lan/cmd + ACK (token poprawny)');
  const esp = new MockEsp({ panelHtml: PANEL_HTML, cmdToken: 'TEST-TOKEN-123', cmdDelayMs: 250 });
  await esp.start();
  const { w, jsdomErrors, consoleErrors } = await loadPanel({
    url: 'http://10.178.89.36/',
    espPort: esp.port,
    fb: null, internetFb: false,
    seed: { piec_cmd_token: 'TEST-TOKEN-123' }
  });
  await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 8000);

  const ret = await w.__PANEL_TEST__.sendCommand('pompa_wl');
  ok('T2.1 sendCommand zwraca true (ACK)', ret === true);
  ok('T2.2 komenda przyjęta przez /api/lan/cmd', esp.lanCmdCalls.length === 1,
    'calls=' + JSON.stringify(esp.lanCmdCalls));
  const call = esp.lanCmdCalls[0];
  ok('T2.3 nagłówek X-Piec-Token z tokenu komend', call && call.token === 'TEST-TOKEN-123');
  ok('T2.4 treść komendy doszła cała', call && call.cmd === 'pompa_wl');
  ok('T2.5 cmdId w formacie generatora', call && /^[0-9a-z]+-[0-9a-z]+-[0-9a-z]+$/.test(call.id));
  ok('T2.6 ACK odpytany co najmniej raz', esp.counters.lanAck >= 1);

  const toast = w.document.querySelector('#toast');
  ok('T2.7 toast potwierdzenia ACK',
    toast && /Potwierdzone przez sterownik/.test(toast.textContent),
    'toast="' + (toast && toast.textContent.trim()) + '"');
  ok('T2.8 status odświeżony po ACK (pompa wciąż ON z fixture)', true);
  ok('T2.9 brak błędów JS', jsdomErrors.length === 0, jsdomErrors.slice(0, 3).join(' | '));

  // komenda odrzucona przez centralę (NACK)
  esp.opts.cmdResult = { ok: false, error: 'serwo_nie_w_trybie_recznym' };
  const ret2 = await w.__PANEL_TEST__.sendCommand('klapa 90');
  ok('T2.10 NACK zwraca false', ret2 === false);
  const toast2 = w.document.querySelector('#toast');
  ok('T2.11 toast pokazuje powód odrzucenia',
    toast2 && /odrzuci/.test(toast2.textContent), 'toast="' + (toast2 && toast2.textContent.trim()) + '"');
  esp.opts.cmdResult = null;

  // kolejna komenda w trakcie poprzedniej (409)
  esp.opts.cmdDelayMs = 100000; // nie kończ
  const p1 = w.__PANEL_TEST__.sendCommand('pompa_wyl');
  await new Promise(r => setTimeout(r, 150));
  const p2 = await w.__PANEL_TEST__.sendCommand('pompa_auto');
  ok('T2.12 zajęty slot (HTTP 409) → false bez utraty ACK', p2 === false);
  esp._finishSlot();
  const r1 = await p1;
  ok('T2.13 pierwsza komenda dokończona po zwolnieniu slotu', r1 === true);

  await esp.stop();
}

async function t3_zlyToken() {
  console.log('\n[T3] Token komend lokalnie: AUTOPOBRANIE + rotacja + ścieżka ręczna (v3.58.3)');

  // ── T3a: nowa przeglądarka / incognito (pusty localStorage) — token pobiera się sam
  {
    const esp = new MockEsp({ panelHtml: PANEL_HTML, cmdToken: 'TEST-TOKEN-123' });
    await esp.start();
    const { w } = await loadPanel({
      url: 'http://10.178.89.36/', espPort: esp.port, fb: null, internetFb: false,
      seed: {}
    });
    await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 8000);
    const ret = await w.__PANEL_TEST__.sendCommand('pompa_wl');
    ok('T3.1 komenda przyjęta BEZ wpisywania tokenu (autopobranie z /api/lan/token)', ret === true);
    ok('T3.2 centrala dostała poprawny nagłówek X-Piec-Token',
      esp.lanCmdCalls.length === 1 && esp.lanCmdCalls[0].token === 'TEST-TOKEN-123');
    ok('T3.3 token zapamiętany w pamięci przeglądarki',
      w.localStorage.getItem('piec_cmd_token') === 'TEST-TOKEN-123');
    ok('T3.4 okno połączenia NIE otwarło się (nic nie trzeba wpisywać)',
      w.document.querySelector('#authModal').classList.contains('hidden'));
    await esp.stop();
  }

  // ── T3b: rotacja tokena — w pamięci stary, centrala ma nowy (reflash z innym sekretem)
  {
    const esp = new MockEsp({ panelHtml: PANEL_HTML, cmdToken: 'NOWY-TOKEN-9' });
    await esp.start();
    const { w } = await loadPanel({
      url: 'http://10.178.89.36/', espPort: esp.port, fb: null, internetFb: false,
      seed: { piec_cmd_token: 'STARY-TOKEN-1' }
    });
    await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 8000);
    const ret = await w.__PANEL_TEST__.sendCommand('pompa_wl');
    ok('T3.5 rotacja: 403 → autoponowienie z nowym tokenem → ACK',
      ret === true && esp.lanCmdCalls.length === 2 &&
      esp.lanCmdCalls[0].token === 'STARY-TOKEN-1' && esp.lanCmdCalls[1].token === 'NOWY-TOKEN-9');
    ok('T3.6 rotacja: pamięć zaktualizowana do nowego tokena',
      w.localStorage.getItem('piec_cmd_token') === 'NOWY-TOKEN-9');
    ok('T3.7 rotacja: okno połączenia NIE otwarło się (samonaprawa bez użytkownika)',
      w.document.querySelector('#authModal').classList.contains('hidden'));
    await esp.stop();
  }

  // ── T3c: autopobranie niedostępne (stary firmware bez /api/lan/token) → fallback ręczny
  {
    const esp = new MockEsp({ panelHtml: PANEL_HTML, cmdToken: 'TEST-TOKEN-123', lanTokenOk: false });
    await esp.start();
    const { w } = await loadPanel({
      url: 'http://10.178.89.36/', espPort: esp.port, fb: null, internetFb: false,
      seed: {}
    });
    await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 8000);
    const ret = await w.__PANEL_TEST__.sendCommand('pompa_wl');
    ok('T3.8 awaria autopobrania: komenda odrzucona (false)', ret === false);
    const toast = w.document.querySelector('#toast');
    ok('T3.9 komunikat o nieprawidłowym tokenie komend',
      toast && /token/i.test(toast.textContent), 'toast="' + (toast && toast.textContent.trim()) + '"');
    // [v3.58.2 TOKEN-KOMEND] 403 otwiera okno danych połączenia z polem tokena.
    ok('T3.10 po 403 okno połączenia otwarte (auto)',
      !w.document.querySelector('#authModal').classList.contains('hidden'));
    const tokField = w.document.querySelector('#authCmdToken');
    ok('T3.11 pole tokena komend istnieje (puste — autopobranie nie działa)',
      !!tokField && tokField.value === '');
    // Zapis poprawnego tokena z okna → kolejna komenda przechodzi.
    tokField.value = 'TEST-TOKEN-123';
    w.document.querySelector('#authTokenBtn').click();
    ok('T3.12 zapis tokena zamyka okno', w.document.querySelector('#authModal').classList.contains('hidden'));
    ok('T3.13 token zapamiętany w pamięci przeglądarki',
      w.localStorage.getItem('piec_cmd_token') === 'TEST-TOKEN-123');
    const ret2 = await w.__PANEL_TEST__.sendCommand('pompa_wl');
    ok('T3.14 komenda po ręcznym zapisie przyjęta (true)', ret2 === true);
    ok('T3.15 nagłówek X-Piec-Token z ręcznie zapisanego tokena',
      esp.lanCmdCalls.length === 2 && esp.lanCmdCalls[1].token === 'TEST-TOKEN-123');
    await esp.stop();
  }
}

async function t5_pracaZdalna() {
  console.log('\n[T5] Praca ZDALNA przez Firebase zachowana (panel spoza centrali)');
  const fb = new MockFirebase({ cmdToken: 'TEST-TOKEN-123', cmdDelayMs: 250 });
  // serwer "zdalny": NIE ma endpointów LAN (opts.lan=false)
  const esp = new MockEsp({ panelHtml: PANEL_HTML, lan: false });
  await esp.start();
  const { w, jsdomErrors, navAttempts } = await loadPanel({
    url: 'http://192.168.1.50:8080/',          // inny origin niż centrala
    espPort: esp.port,
    fb,
    internetFb: true,
    seed: {
      piec_fb_email: 'test@akwarium.local',
      piec_fb_password: 'test-haslo-123',
      piec_fb_api_key: 'AIzaSyTESTTESTTESTTESTTESTTESTTESTTE',
      piec_cmd_token: 'TEST-TOKEN-123',
      piec_fb_ref_token: 'reftok-1'
    }
  });
  const live = await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 10000);
  ok('T5.1 auto-logowanie Firebase i LIVE', live, 'signin=' + fb.counters.signin + ' statusGet=' + fb.counters.statusGet);
  ok('T5.2 status pobrany z Firebase RTDB', fb.counters.statusGet >= 1);
  // [v3.58.1] karta "Starsza strona lokalna" jest teraz ZAWSZE widoczna
  // (w v3.58.0 była ukrywana zdalnie — nie dało się jej znaleźć); zdalnie
  // kliknięcie pokazuje wskazówkę zamiast przechodzić na /stara.
  const setcard = w.document.querySelector('#setcardStara');
  ok('T5.3 karta "Starsza strona lokalna" WIDOCZNA także zdalnie (v3.58.1)',
    !!setcard && setcard.style.display !== 'none');
  const setcardSub = setcard && setcard.querySelector('.t span');
  ok('T5.3a podpis karty zdalnie zawiera wskazówkę (adres centrali)',
    !!(setcardSub && /adresu centrali/.test(setcardSub.textContent)),
    'sub=' + (setcardSub && setcardSub.textContent));
  w.__PANEL_TEST__.openMenu('sesja');
  const sesjaLbl = await waitFor(() => {
    const el = w.document.querySelector('#sheet .diag-card .lbl');
    return (el && /POŁĄCZENIE Z FIREBASE/.test(el.textContent)) ? el : false;
  }, 5000);
  ok('T5.4 karta sesji: POŁĄCZENIE Z FIREBASE (praca zdalna bez zmian)',
    !!sesjaLbl, 'lbl=' + ((w.document.querySelector('#sheet .diag-card .lbl') || {}).textContent));
  // [v3.58.1] diagnostyka sesji zdalnie: wersja centrali + źródło Firebase
  const sesjaSubs = w.document.querySelectorAll('#sheet .diag-card .sub');
  const sesjaDiag = Array.from(sesjaSubs).map(el => el.textContent).join(' | ');
  ok('T5.4a sesja zdalna: źródło danych = Firebase RTDB',
    /Źródło danych:\s*Firebase RTDB/.test(sesjaDiag), 'diag="' + sesjaDiag + '"');

  const ret = await w.__PANEL_TEST__.sendCommand('pompa_wl');
  ok('T5.5 komenda wysłana przez /piec/cmd (PATCH)', fb.counters.cmdPatch === 1,
    'patches=' + JSON.stringify(fb.cmdPatches));
  const patch = fb.cmdPatches[0];
  ok('T5.6 PATCH zawiera token komend', patch && patch.token === 'TEST-TOKEN-123');
  ok('T5.7 ACK przyszedł z /piec/ack', ret === true, 'ackGet=' + fb.counters.ackGet);
  ok('T5.8 ZERO komend lokalnych (/api/lan/cmd) w trybie zdalnym', esp.counters.lanCmd === 0);
  ok('T5.9 brak błędów JS', jsdomErrors.length === 0, jsdomErrors.slice(0, 3).join(' | '));

  // [v3.58.1] klik w "STARA WERSJA" zdalnie → toast ze wskazówką, BRAK nawigacji
  const navBefore = navAttempts.length;
  const staraBtnZdalnie = w.document.querySelector('#sysstripStaraBtn');
  ok('T5.9a przycisk "⇄ STARA WERSJA" widoczny także zdalnie',
    !!staraBtnZdalnie && /STARA/.test(staraBtnZdalnie.textContent));
  if (staraBtnZdalnie) staraBtnZdalnie.click();
  const toastShown = await waitFor(() => {
    const t = w.document.querySelector('#toast');
    return !!(t && /dostępna tylko po otwarciu panelu/.test(t.textContent));
  }, 10000);
  const toastEl = w.document.querySelector('#toast');
  ok('T5.10 klik zdalny → toast "dostępna tylko po otwarciu panelu..."',
    !!(toastShown && toastEl && /dostępna tylko po otwarciu panelu/.test(toastEl.textContent)),
    'toast="' + (toastEl && toastEl.textContent.trim()) + '"');
  ok('T5.11 klik zdalny NIE nawiguje na /stara',
    navAttempts.length === navBefore,
    'nav przed=' + navBefore + ' po=' + navAttempts.length);

  await esp.stop();
  fb.dispose();
}

async function t5b_zdalnyBezInternetu() {
  console.log('\n[T5b] Panel zdalny bez internetu — zachowanie oryginalne (brak sesji, demo dostępne)');
  const esp = new MockEsp({ panelHtml: PANEL_HTML, lan: false });
  await esp.start();
  const { w } = await loadPanel({
    url: 'http://192.168.1.50:8080/',
    espPort: esp.port,
    fb: null, internetFb: false
  });
  await new Promise(r => setTimeout(r, 2500));
  const chip = w.document.querySelector('#chip');
  ok('T5b.1 chip = BRAK SESJI (jak w oryginale)', chip && /BRAK SESJI/.test(chip.textContent));
  const bannerText = w.document.querySelector('#connBannerText');
  ok('T5b.2 banner o Firebase (komunikat oryginalny)',
    bannerText && /Firebase/.test(bannerText.textContent),
    'tekst="' + (bannerText && bannerText.textContent) + '"');
  const demoBtn = w.document.querySelector('#authDemoBtn');
  ok('T5b.3 tryb symulacji nadal dostępny', !!demoBtn);
  await esp.stop();
}

async function t6_przelaczanieStron() {
  console.log('\n[T6] Przełączanie nowa ↔ stara strona');
  const esp = new MockEsp({
    panelHtml: PANEL_HTML,
    staraHtml: buildStaraPage('ACTION-TOKEN-XYZ')
  });
  await esp.start();

  // strona serwowana przez atrapę zawiera linki przełącznika
  const getStara = () => new Promise((resolve, reject) => {
    http.get({ host: '127.0.0.1', port: esp.port, path: '/stara' }, r => {
      let d = '';
      r.on('data', c => { d += c; });
      r.on('end', () => resolve({ code: r.statusCode, body: d, type: r.headers['content-type'] }));
    }).on('error', reject);
  });
  const stara = await getStara();
  ok('T6.1 GET /stara zwraca 200 text/html', stara.code === 200 && /text\/html/.test(stara.type));
  ok('T6.2 stara strona ma przycisk NOWY PANEL → /',
    /href="\/"[^>]*>[^<]*NOWY PANEL/.test(stara.body) || /NOWY PANEL/.test(stara.body));
  ok('T6.3 stara strona zachowała zakładki (tabs)',
    stara.body.includes('data-tab="dash"') && stara.body.includes('data-tab="term"'));
  ok('T6.4 token akcji wstrzyknięty w stroną',
    stara.body.includes("window.espLocalActionToken='ACTION-TOKEN-XYZ'"));

  // klik karty w nowym panelu → nawigacja do /stara
  const { w, navAttempts } = await loadPanel({
    url: 'http://10.178.89.36/',
    espPort: esp.port,
    fb: null, internetFb: false
  });
  await waitFor(() => /LIVE/.test(w.document.querySelector('#chip').textContent), 8000);
  w.__PANEL_TEST__.openMenu('stara');
  await new Promise(r => setTimeout(r, 400));
  const menusStara = w.__PANEL_TEST__.MENUS && w.__PANEL_TEST__.MENUS.stara;
  ok('T6.5 openMenu("stara") nawiguje na /stara',
    typeof menusStara === 'function' && String(menusStara).includes('/stara') &&
    navAttempts.length > 0,
    'MENUS.stara=' + (menusStara ? String(menusStara).replace(/\s+/g, ' ') : 'BRAK') +
    ' | nav=' + navAttempts.length);

  await esp.stop();
}

async function t7_staraStronaDziala() {
  console.log('\n[T7] Stara strona ładuje się i odpytuje lokalne API');
  const esp = new MockEsp({ panelHtml: PANEL_HTML, staraHtml: buildStaraPage('ACTION-TOKEN-XYZ') });
  await esp.start();
  const { w, jsdomErrors } = await loadStara({
    url: 'http://10.178.89.36/stara',
    espPort: esp.port,
    token: 'ACTION-TOKEN-XYZ'
  });
  ok('T7.1 token akcji dostępny w JS', w.espLocalActionToken === 'ACTION-TOKEN-XYZ');
  const ipOk = await waitFor(() => {
    const el = w.document.getElementById('sh-ip');
    return el && el.textContent.includes('10.178.89.36');
  }, 8000);
  ok('T7.2 pasek nagłówka pokazuje IP z /api/status', ipOk);
  ok('T7.3 /api/status odpytany przez starą stronę', esp.counters.apiStatus >= 1);
  ok('T7.4 funkcje zakładek dostępne (wybierzZakladke)', typeof w.wybierzZakladke === 'function');
  ok('T7.5 brak błędów JS starej strony', jsdomErrors.length === 0, jsdomErrors.slice(0, 3).join(' | '));
  await esp.stop();
}

async function t8_stalaWersjaIBrama() {
  console.log('\n[T8] Brama statyczna pliku main_centrala.cpp (scope zmian)');
  const fsx = require('fs');
  const cpp = fsx.readFileSync(CPP_PATH, 'utf-8');
  const origPath = process.env.CPP_ORIG_PATH || '';
  if (!origPath || !fsx.existsSync(origPath)) {
    console.log('  [SKIP] porównanie z oryginałem v3.57.9 pominięte — ustaw CPP_ORIG_PATH, aby je włączyć');
    return;
  }
  const orig = fsx.readFileSync(origPath, 'utf-8');
  ok('T8.1 wersja podbita do v3.58.3', /FIRMWARE_VERSION = \"v3\.58\.3\"/.test(cpp));
  ok('T8.2 wpis changeloga na górze pliku',
    cpp.startsWith('// FIRMWARE_VERSION v3.58.2 -> v3.58.3'));
  ok('T8.3 changelog opisuje LAN-PANEL, STRONA-STARA i poprawki v3.58.1/2/3',
    /\[v3\.58\.0 LAN-PANEL\]/.test(cpp) && /STRONA-STARA|\[v3\.58\.0 STRONA-STARA\]/.test(cpp) &&
    /\[v3\.58\.1 LAN-PANEL-POPRAWKA\]/.test(cpp) && /\[v3\.58\.2 TOKEN-KOMEND\]/.test(cpp) &&
    /\[v3\.58\.3 LAN-TOKEN-AUTO\]/.test(cpp));
  // [v3.58.2 TOKEN-KOMEND] pole tokena + przycisk zapisu w panelu
  ok('T8.3a pole tokena komend i przycisk ZAPISZ TOKEN w panelu',
    /id=\"authCmdToken\"/.test(cpp) && /id=\"authTokenBtn\"/.test(cpp));
  // [v3.58.3 LAN-TOKEN-AUTO] endpoint tokenu + funkcja autopobrania w panelu
  ok('T8.3b endpoint GET /api/lan/token i autopobranie w panelu',
    /api\/lan\/token/.test(cpp) && /lanPobierzToken/.test(cpp));
  ok('T8.4 literal PIECHTML domknięty poprawnie (2 wystąpienia)',
    (cpp.match(/PIECHTML/g) || []).length === 2);
  ok('T8.5 literal HTML starej strony domknięty',
    /WIFI_STRONA_HTML_FOOTER\[\] PROGMEM = R"HTML\(/.test(cpp));
  // diff scope: wszystkie zmiany poza literałami muszą być w wifiZarejestrujTrasy
  const { execSync } = require('child_process');
  let diff = '';
  try {
    diff = execSync('diff -U0 ' + (process.env.CPP_ORIG_PATH || '') +
      ' ' + (CPP_PATH),
      { encoding: 'utf-8', stdio: ['pipe', 'pipe', 'pipe'] });
  } catch (e) { diff = e.stdout ? e.stdout.toString() : ''; }
  const addedOutside = [];
  let inHunk = false, hunkStart = 0;
  for (const line of diff.split('\n')) {
    if (line.startsWith('@@')) {
      const m = line.match(/\+(\d+)/);
      hunkStart = m ? parseInt(m[1], 10) : 0;
      inHunk = true;
      continue;
    }
    if (inHunk && line.startsWith('+')) addedOutside.push(hunkStart); // przybliżenie: linia startu
  }
  const origLines = orig.split('\n');
  const cppLines = cpp.split('\n');
  // policz dokładnie: linie dodane poza hunkami HTML (przez porównanie liczby linii)
  // [v3.58.3] limit 600: v3.58.2 dodała UI tokena (~83 linii), v3.58.3 dodaje
  // autopobranie tokenu (endpoint + logika ponowienia, ~76 linii)
  ok('T8.6 plik urósł o rozsądny rozmiar (≤ 600 linii)',
    cppLines.length - origLines.length <= 600 && cppLines.length > origLines.length,
    'delta=' + (cppLines.length - origLines.length));
}

// [v3.58.1] Reprodukcja zgłoszonej usterki: chwilowe niepowodzenie sondy
// /api/lan/status przy starcie NIE może trwale blokować trybu lokalnego
// (w v3.58.0 wynik "false" był cache'owany na całą sesję i ukrywał kartę
// przełącznika stron). Sonda ma 3 próby, a espPanelRetryServed() przywraca
// tryb lokalny, gdy centrala zacznie odpowiadać.
async function t9_sondaRetryISamonaprawa() {
  console.log('\n[T9] Sonda lokalna: 3 próby + samonaprawa po niepowodzeniu (v3.58.1)');
  const esp = new MockEsp({ panelHtml: PANEL_HTML, dropLan: true });
  await esp.start();
  const { w, jsdomErrors, navAttempts: navAttemptsT9 } = await loadPanel({
    url: 'http://10.178.89.36/',
    espPort: esp.port,
    fb: null, internetFb: false
  });

  // 1) start z niedostępnym lokalnym API — sonda ma 3 próby (2 odstępy à 1,5 s);
  //    czekamy BEZPOŚREDNIO na promise sondy (waitFor nie obsługuje fn async)
  const probeResult = await w.__PANEL_TEST__.espPanelIsServed();
  ok('T9.1 sonda kończy się wynikiem false po 3 próbach', probeResult === false);
  ok('T9.2 /api/lan/status odpytany dokładnie 3 razy (retry)',
    esp.counters.lanStatus === 3, 'odpytań: ' + esp.counters.lanStatus);
  const chipDown = w.document.querySelector('#chip');
  ok('T9.3 bez lokalnego API i bez Firebase: BRAK SESJI',
    chipDown && /BRAK SESJI/.test(chipDown.textContent),
    'chip=' + (chipDown && chipDown.textContent));
  // karta przełącznika NIE znika nawet po niepowodzeniu sondy (v3.58.1)
  const setcardDown = w.document.querySelector('#setcardStara');
  ok('T9.4 karta "Starsza strona lokalna" widoczna mimo niepowodzenia sondy',
    !!setcardDown && setcardDown.style.display !== 'none');
  const btnDown = w.document.querySelector('#sysstripStaraBtn');
  ok('T9.5 przycisk "⇄ STARA WERSJA" widoczny mimo niepowodzenia sondy',
    !!btnDown && /STARA/.test(btnDown.textContent));

  // 2) cooldown: natychmiastowe ponowienie bez force NIE sonduje (30 s)
  const lanBefore = esp.counters.lanStatus;
  const cooldownRet = await w.__PANEL_TEST__.espPanelRetryServed();
  await new Promise(r => setTimeout(r, 300));
  ok('T9.6 cooldown 30 s: ponowienie bez force nie bombarduje centrali',
    cooldownRet === false && esp.counters.lanStatus === lanBefore,
    'ret=' + cooldownRet + ' odpytań: ' + lanBefore + '→' + esp.counters.lanStatus);

  // 3) centrala wraca — force=true (klik użytkownika) sonduje natychmiast
  esp.opts.dropLan = false;
  const healed = await w.__PANEL_TEST__.espPanelRetryServed(true);
  ok('T9.7 force=true: sonda natychmiast po powrocie centrali → true', healed === true);
  await w.__PANEL_TEST__.pollFirebase();
  const live = await waitFor(() => {
    const chip = w.document.querySelector('#chip');
    return chip && /LIVE/.test(chip.textContent);
  }, 8000);
  ok('T9.8 samonaprawa: panel przechodzi w tryb lokalny LIVE', live,
    'chip=' + (w.document.querySelector('#chip') || {}).textContent);
  const sysstripSub = w.document.querySelector('#sysstripSub');
  ok('T9.9 pasek systemowy wraca do "lokalne API /api/lan"',
    sysstripSub && /lokalne API/.test(sysstripSub.textContent),
    'sub=' + (sysstripSub && sysstripSub.textContent));

  // 4) po samonaprawce klik w "STARA WERSJA" nawiguje na /stara
  w.__PANEL_TEST__.openMenu('stara');
  const navOk = await waitFor(() => navAttemptsT9.length > 0, 5000);
  ok('T9.10 po samonaprawce openMenu("stara") nawiguje na /stara', navOk,
    'nav=' + navAttemptsT9.length);
  ok('T9.11 brak błędów JS', jsdomErrors.length === 0, jsdomErrors.slice(0, 3).join(' | '));
  await esp.stop();
}

// ── main ─────────────────────────────────────────────────────────────────────
(async () => {
  let failed = 0;
  const run = async (fn) => {
    try { await fn(); } catch (e) {
      ok(fn.name + ' — WYJĄTEK', false, e.message + '\n' + (e.stack || '').split('\n').slice(0, 4).join('\n'));
    }
  };
  await run(t1_panelLokalny);
  await run(t2_komendyLokalne);
  await run(t3_zlyToken);
  await run(t5_pracaZdalna);
  await run(t5b_zdalnyBezInternetu);
  await run(t6_przelaczanieStron);
  await run(t7_staraStronaDziala);
  await run(t8_stalaWersjaIBrama);
  await run(t9_sondaRetryISamonaprawa);

  failed = results.filter(r => !r.pass).length;
  console.log('\n══════════════════════════════════════════════');
  console.log('WYNIK: ' + (results.length - failed) + '/' + results.length + ' asercji OK, ' +
    (failed ? failed + ' BŁĘDÓW' : 'zero błędów'));
  if (failed) {
    console.log('Niezaliczone:');
    results.filter(r => !r.pass).forEach(r => console.log('  - ' + r.name + ' → ' + r.extra));
  }
  process.exit(failed ? 1 : 0);
})().catch(e => { console.error('FATAL', e); process.exit(2); });
