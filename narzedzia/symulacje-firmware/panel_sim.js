#!/usr/bin/env node
/**
 * Symulacja panelu: czy przelacznik „Zapis historii na flash" naprawde dziala
 * na zywej stronie — parsowanie statusu, zachowanie checkboxa, wysylka komend.
 *
 * Trzy sekcje:
 *   A. PARSOWANIE — bierzemy PRAWDZIWY blok `flagKeys.forEach(...)` z Piec.html
 *      i z panelu wbudowanego w firmware i puszczamy na 6 roznych statusach.
 *   B. PRZELACZNIK — prawdziwe `toggleRow` + `live$` + logika synchronizacji
 *      z arkusza, na prawdziwym DOM (jsdom): klik, ACK, brak ACK, zmiana statusu.
 *   C. APLIKACJA — model `intField("hist_fs", 0, 1)` z PiecState.kt, zeby
 *      porownac, co widzi telefon przy tych samych statusach.
 *
 * Uruchomienie (z katalogu repo):
 *   node narzedzia/symulacje-firmware/panel_sim.js
 * Wymaga jsdom — run-all.sh instaluje go do /tmp/domjs i ustawia NODE_PATH.
 */
'use strict';

const fs = require('fs');
const path = require('path');

let JSDOM = null;
try { ({ JSDOM } = require('jsdom')); } catch (e) { /* sprawdzamy nizej */ }

const root = path.resolve(__dirname, '..', '..');
const patchDir = path.join(root, 'firmware-patch');
const fwDir = fs.readdirSync(patchDir)
  .filter(n => /^v3\.57\.\d+-hist-fs-opcja$/.test(n))
  .sort((a, b) => parseInt(a.match(/v3\.57\.(\d+)/)[1], 10) - parseInt(b.match(/v3\.57\.(\d+)/)[1], 10))
  .pop();
const fwSrc = fs.readFileSync(path.join(patchDir, fwDir, 'src', 'main_centrala.cpp'), 'utf8');
const piec = fs.readFileSync(path.join(root, 'Piec.html'), 'utf8');

let ok = 0, fail = 0;
function check(name, cond, detail) {
  if (cond) { ok++; console.log('  OK    ' + name); }
  else { fail++; console.log('  FAIL  ' + name + (detail !== undefined ? ' — ' + detail : '')); }
}
function section(t) { console.log('\n[' + t + ']'); }

// ── wyciaganie kawalkow prawdziwego kodu ───────────────────────────────────
// Zaslepia literaly tekstowe ZACHOWUJAC DLUGOSC (offsety musza sie zgadzac,
// inaczej liczenie nawiasow trafia w zle miejsce).
function bezStringow(s) {
  let out = '', i = 0;
  while (i < s.length) {
    const c = s[i];
    if (c === '"' || c === "'" || c === '`') {
      const q = c; const start = i; i++;
      while (i < s.length) { if (s[i] === '\\') { i += 2; continue; } if (s[i] === q) { i++; break; } i++; }
      const dl = i - start;
      out += q + ' '.repeat(Math.max(0, dl - 2)) + (dl >= 2 ? q : '');
      continue;
    }
    out += c; i++;
  }
  return out;
}
function wytnijFunkcje(src, nazwa) {
  const start = src.indexOf('function ' + nazwa + '(');
  if (start < 0) throw new Error('brak funkcji ' + nazwa);
  const czysty = bezStringow(src.slice(start));
  let glebokosc = 0, i = czysty.indexOf('{');
  for (; i < czysty.length; i++) {
    if (czysty[i] === '{') glebokosc++;
    else if (czysty[i] === '}') { glebokosc--; if (glebokosc === 0) { i++; break; } }
  }
  return src.slice(start, start + i);
}
function wytnijFlagi(src) {
  const i = src.indexOf('const flagKeys = [');
  if (i < 0) throw new Error('brak flagKeys');
  const j = src.indexOf('};', src.indexOf('flagKeys.forEach(', i));
  if (j < 0) throw new Error('brak petli flagKeys');
  return src.slice(i, j + 2);        // od `const flagKeys` do `});`
}

// ═══════════════════════════════════════════════════════════════════════════
section('A. Parsowanie statusu — prawdziwy kod obu paneli');
// ═══════════════════════════════════════════════════════════════════════════
const startPanel = fwSrc.indexOf('R"PIECHTML(') + 'R"PIECHTML('.length;
const panel = fwSrc.slice(startPanel, fwSrc.indexOf(')PIECHTML"', startPanel));

const statusy = [
  { nazwa: '3.57.6, zapis WLACZONY', d: { hist_fs: 1 }, znane: true, wartosc: true },
  { nazwa: '3.57.6, zapis WYLACZONY', d: { hist_fs: 0 }, znane: true, wartosc: false },
  { nazwa: '3.57.3 i starsze (brak pola)', d: { ip: '1.2.3.4' }, znane: false, wartosc: false },
  { nazwa: 'pole tekstowe "1" (nie liczba)', d: { hist_fs: '1' }, znane: false, wartosc: false },
  { nazwa: 'pole null', d: { hist_fs: null }, znane: false, wartosc: false },
  { nazwa: 'pole true (bool)', d: { hist_fs: true }, znane: true, wartosc: true },
];

for (const [nazwa, src] of [['Piec.html', piec], ['panel w firmware', panel]]) {
  const blok = wytnijFlagi(src);
  for (const st of statusy) {
    const receivedFields = new Set();
    const S = { hist_fs: false };
    const real = {};
    const d = st.d;
    // eslint-disable-next-line no-new-func
    new Function('d', 'receivedFields', 'S', 'real', blok)(d, receivedFields, S, real);
    const znane = receivedFields.has('hist_fs');
    const wartosc = !!S.hist_fs;
    check(nazwa + ' · ' + st.nazwa,
      znane === st.znane && wartosc === st.wartosc,
      'znane=' + znane + ' wartosc=' + wartosc);
  }
}

// ═══════════════════════════════════════════════════════════════════════════
section('B. Przelacznik w DOM — klik, ACK, brak ACK, zmiana statusu');
// ═══════════════════════════════════════════════════════════════════════════
if (!JSDOM) {
  check('jsdom dostepny', false, 'zainstaluj: npm i jsdom (run-all.sh robi to sam)');
} else {
  const dom = new JSDOM('<!doctype html><html><body><div id="sheet"></div></body></html>');
  global.document = dom.window.document;
  global.window = dom.window;

  // minimalne `h` z panelu (tworzy element, ustawia klase i ew. HTML)
  const h = (tag, cls, html) => {
    const el = document.createElement(tag);
    if (cls) el.className = cls;
    if (html !== undefined) el.innerHTML = html;
    return el;
  };
  const toggleRow = new Function('h', 'return ' + wytnijFunkcje(piec, 'toggleRow'))(h);
  const zywe = {};
  let licznik = 0;
  const live$ = (el, fn) => { const id = 'l' + (++licznik); el.dataset.live = id; zywe[id] = fn; fn(el); return el; };

  // Stan zastepczy: to, co w panelu robi hasData()/S.hist_fs.
  const swiat = { znane: true, wartosc: true, wyslane: [] };
  const hasData = () => swiat.znane;
  const S = {};
  Object.defineProperty(S, 'hist_fs', { get: () => swiat.wartosc });
  const on = () => hasData() && !!S.hist_fs;
  const tick = () => Object.keys(zywe).forEach(k => zywe[k](document.querySelector('[data-live="' + k + '"]')));

  // Kod 1:1 z showHistFsSheet() po poprawkach 3.57.5/3.57.6.
  let trgStan = on();
  const trg = toggleRow('Zapisuj historię na flash centrali', 'opis', on(), v => {
    swiat.wyslane.push(v ? 'histfs_wl' : 'histfs_wyl');
    trgBox.checked = v;
    const ok = swiat.odpowiedz(v);
    if (ok === false) { trgStan = on(); trgBox.checked = on(); }
  });
  const trgBox = trg.querySelector('input');
  document.getElementById('sheet').appendChild(trg);   // jak b.appendChild(trg) w arkuszu
  live$(trgBox, el => { const cur = on(); if (cur !== trgStan) { trgStan = cur; el.checked = cur; } });

  swiat.odpowiedz = () => true;
  check('start: status WL => checkbox zaznaczony', trgBox.checked === true);

  // klik: uzytkownik wylacza
  trgBox.checked = false;
  trgBox.dispatchEvent(new dom.window.Event('change'));
  check('klik WYL => wyslana komenda histfs_wyl', swiat.wyslane[0] === 'histfs_wyl', JSON.stringify(swiat.wyslane));

  // status jeszcze sie nie zmienil (WL) — checkbox NIE moze wrocic sam
  tick(); tick();
  check('status jeszcze stary: checkbox zostaje na zyczenie uzytkownika', trgBox.checked === false);

  // przychodzi potwierdzony status WYL
  swiat.wartosc = false; tick();
  check('status WYL => checkbox nadal odznaczony', trgBox.checked === false);

  // ktos przelaczyl z aplikacji — status WL
  swiat.wartosc = true; tick();
  check('status WL (zmiana zdalna) => checkbox sam sie zaznacza', trgBox.checked === true);

  // nieudana komenda (NACK / brak ACK) => checkbox wraca do prawdy z centrali
  swiat.odpowiedz = () => false;
  trgBox.checked = false;
  trgBox.dispatchEvent(new dom.window.Event('change'));
  check('brak potwierdzenia => checkbox wraca do stanu centrali', trgBox.checked === true);

  // brak pola w statusie => przelacznik zablokowany komunikat, ale nie wywala sie
  swiat.znane = false; swiat.wartosc = false; tick();
  check('brak pola hist_fs => brak wyjatku, checkbox odznaczony', trgBox.checked === false);
}

// ═══════════════════════════════════════════════════════════════════════════
section('C. Aplikacja — model intField("hist_fs", 0, 1) z PiecState.kt');
// ═══════════════════════════════════════════════════════════════════════════
(function aplikacja() {
  // odpowiednik: readNumber() + intField() z core/PiecState.kt
  function readNumber(d, key, min, max, integer) {
    if (!(key in d) || d[key] === null) return null;
    const v = typeof d[key] === 'number' ? d[key] : null;
    if (v === null || !Number.isFinite(v) || v < min || v > max || (integer && v % 1 !== 0)) return null;
    return v;
  }
  const przypadki = [
    ['1 => zapis WL', { hist_fs: 1 }, true, 1],
    ['0 => zapis WYL', { hist_fs: 0 }, true, 0],
    ['brak pola => nieznane', {}, false, 0],
    ['"1" jako tekst => nieznane', { hist_fs: '1' }, false, 0],
    ['null => nieznane', { hist_fs: null }, false, 0],
    ['true (bool) => nieznane', { hist_fs: true }, false, 0],
    ['2 spoza zakresu => nieznane', { hist_fs: 2 }, false, 0],
  ];
  for (const [nazwa, d, znane, wartosc] of przypadki) {
    const v = readNumber(d, 'hist_fs', 0, 1, true);
    const known = v !== null;
    const val = v === null ? 0 : v;
    check('aplikacja · ' + nazwa, known === znane && val === wartosc,
      'known=' + known + ' value=' + val);
  }
})();

console.log('\n' + ok + ' OK / ' + fail + ' FAIL');
process.exit(fail ? 1 : 0);
