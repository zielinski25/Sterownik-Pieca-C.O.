#!/usr/bin/env node
/**
 * Test statyczny: przelacznik zapisu historii na flash (v3.57.5) + parsowanie
 * pola `hist_fs` w panelu centrali i w Piec.html.
 *
 * Po co: w 3.57.4 firmware wysylal `hist_fs` w obu JSON-ach, ale panel i
 * Piec.html w ogole go nie parsowaly (lista flag), wiec kafelek pokazywalby
 * "brak danych". Ten test pilnuje trzech rzeczy, ktore latwo rozjechac:
 *   1. zadanie czyszczenia ma DOKLADNIE ten format, ktorego oczekuje
 *      history_a191::Storage (tresc + nazwy plikow .req/.done),
 *   2. pole `hist_fs` jest w JSON-ach i w listach flag obu paneli,
 *   3. bilans nawiasow w main_centrala.cpp (z pominieciem stringow/komentarzy).
 *
 * Uruchomienie (z katalogu repo):
 *   node narzedzia/testy-firmware/test-hist-fs.js
 */
'use strict';
const fs = require('fs');
const path = require('path');

// Katalog repo liczymy z polozenia testu (nie z cwd), zeby dalo sie go uruchomic
// z dowolnego miejsca. Wersje firmware bierzemy jako NAJNOWSZY folder
// firmware-patch/v3.57.*-hist-fs-opcja - test nie wymaga poprawiania po kazdym
// podbiciu numeru.
const root = path.resolve(__dirname, '..', '..');
const patchDir = path.join(root, 'firmware-patch');
const fwDir = fs.readdirSync(patchDir)
  .filter(n => /^v3\.57\.\d+-hist-fs-opcja$/.test(n))
  .sort((a, b) => parseInt(a.match(/v3\.57\.(\d+)/)[1], 10) - parseInt(b.match(/v3\.57\.(\d+)/)[1], 10))
  .pop();
if (!fwDir) { console.error('Brak folderu firmware-patch/v3.57.*-hist-fs-opcja'); process.exit(2); }
const FW = path.join(patchDir, fwDir, 'src');
const fw = fs.readFileSync(path.join(FW, 'main_centrala.cpp'), 'utf8');
const storage = fs.readFileSync(path.join(FW, 'Storage.h'), 'utf8');
const vfs = fs.readFileSync(path.join(FW, 'EspVfsCalls.h'), 'utf8');
const piec = fs.readFileSync(path.join(root, 'Piec.html'), 'utf8');

let ok = 0, fail = 0;
function check(name, cond, detail) {
  if (cond) { ok++; console.log('  OK   ' + name); }
  else { fail++; console.log('  FAIL ' + name + (detail ? ' — ' + detail : '')); }
}
function section(t) { console.log('\n[' + t + ']'); }

// ───────────────────────────────────────────────────────── A. umowa z Storage.h
section('A. Zadanie czyszczenia = format history_a191::Storage');

// Storage.h trzyma to jako literał C++ ("...\\n"), więc przed porównaniem
// rozwijamy sekwencje ucieczki - inaczej porównywalibyśmy źródło, nie bajty.
function cUnescape(t) {
  return t.replace(/\\n/g, '\n').replace(/\\r/g, '\r').replace(/\\t/g, '\t').replace(/\\\\/g, '\\');
}
const mReq = storage.match(/requestText\s*=\s*"([^"]*)"/);
const reqText = mReq ? cUnescape(mReq[1]) : null;
check('Storage.h ma requestText', !!reqText, 'brak');
check('requestText ma 17 znakow (tyle czyta token())',
  reqText === 'RESET-HISTORY-A1\n' && reqText.length === 17, JSON.stringify(reqText));

const mZad = fw.match(/static const char ZADANIE\[\]\s*=\s*"([^"]*)"\s*;/);
const zadText = mZad ? mZad[1] : null;
const zadBytes = zadText === null ? null : cUnescape(zadText);
check('firmware sklada zadanie o tej samej tresci', zadBytes === reqText,
  JSON.stringify(zadBytes) + ' vs ' + JSON.stringify(reqText));

const mDone = vfs.match(/case Path::Completion:return\s*"([^"]*)"/);
const doneVfs = mDone ? mDone[1] : null;
check('EspVfsCalls: .done = /littlefs/history_reset_a1.done',
  doneVfs === '/littlefs/history_reset_a1.done', String(doneVfs));
check('firmware usuwa .done spod tej samej sciezki (Arduino: /history_reset_a1.done)',
  fw.includes('systemFS196.remove("/history_reset_a1.done")') &&
  fw.includes('systemFS196.open("/history_reset_a1.req.tmp", "w")'));

// [3.57.6] Zapis musi byc atomowy: tymczasowy plik + odczyt kontrolny + rename.
check('zapis zadania przez plik tymczasowy + rename',
  fw.includes('systemFS196.rename("/history_reset_a1.req.tmp", "/history_reset_a1.req")'));
check('odczyt kontrolny zadania (17 B, ta sama tresc)',
  /memcmp\(odczyt, ZADANIE, dlugosc\)/.test(fw));
check('naprawa uszkodzonego .req przy starcie (przed historiaBoot.start)',
  fw.includes('historiaFsNaprawUszkodzoneZadanie();') &&
  fw.indexOf('historiaFsNaprawUszkodzoneZadanie();') < fw.indexOf('historiaBoot.start(littlefsReady') &&
  fw.includes('"/history_reset_a1.req.bad"'));

// .done musi zniknac PRZED zlozeniem zadania - inaczej Transition::release()
// tylko konsumuje zadanie i nie kasuje plikow.
const iDone = fw.indexOf('systemFS196.remove("/history_reset_a1.done")');
const iReq = fw.indexOf('systemFS196.open("/history_reset_a1.req.tmp", "w")');
check('kolejnosc: najpierw .done, potem .req', iDone > 0 && iReq > iDone, iDone + ' / ' + iReq);

// ───────────────────────────────────────────────── B. wersja i brak martwego kodu
section('B. Wersja i porzadki');

const wersjaFolderu = fwDir.match(/^v(3\.57\.\d+)-/)[1];
check('FIRMWARE_VERSION = ' + wersjaFolderu + ' (zgodna z folderem)',
  fw.includes('FIRMWARE_VERSION = "v' + wersjaFolderu + '";'), 'folder: ' + fwDir);
check('brak starego znacznika NVS "czysc" (jeden mechanizm, nie dwa)',
  !fw.includes('putBool("czysc"') && !fw.includes('getBool("czysc"'));
check('brak kasowania plikow historii wlasnym kodem (LittleFS.remove tylko dla .req)',
  !/^\s*LittleFS\.remove\((HISTORIA_PLIK|HISTORIA_DLUGA_PLIK)/m.test(fw));
const histFsJson = fw.split(String.raw`hist_fs\":`).length - 1;
check('pole hist_fs w /api/status i w JSON dla Firebase', histFsJson === 2, String(histFsJson));
// Panel i aplikacja przyjmuja WYLACZNIE liczbe 0/1 - nie "true"/"false".
const histFsNum = fw.split(String.raw`historiaZapisNaFsWlaczony() ? 1 : 0`).length - 1;
check('hist_fs wysylane jako liczba 0/1 (nie bool)', histFsNum === 2, String(histFsNum));
check('bramka obejmuje oproznianie kolejki krotkiej',
  fw.includes('!littlefsReady || historiaClearPending || !historiaZapisNaFsWlaczony()'));
check('bramka obejmuje oproznianie kolejki dlugiej',
  fw.includes('if (!historiaZapisNaFsWlaczony()) return false;'));

// ───────────────────────────────────────────────── C. panele (JS)
section('C. Parsowanie hist_fs w panelu centrali i Piec.html');

function panelHtml() {
  const start = fw.indexOf('R"PIECHTML(') + 'R"PIECHTML('.length;
  const end = fw.indexOf(')PIECHTML"', start);
  return fw.slice(start, end);
}
const panel = panelHtml();

for (const [nazwa, src] of [['panel centrali', panel], ['Piec.html', piec]]) {
  const mFlags = src.match(/const flagKeys = \[([\s\S]*?)\];/);
  check(nazwa + ': jest lista flagKeys', !!mFlags);
  let flags = [];
  if (mFlags) {
    flags = mFlags[1].split(',').map(x => x.trim()).filter(x => /^'/.test(x)).map(x => x.replace(/^'|'$/g, ''));
  }
  check(nazwa + ': hist_fs jest w flagKeys', flags.includes('hist_fs'), flags.join('|'));
  check(nazwa + ': hist_fs jest w stanie poczatkowym S', /rtc_ok:\s*false,\s*hist_fs:\s*false/.test(src));
  check(nazwa + ': przelacznik dogania status przez live$',
    /live\$\(trgBox, el =>/.test(src));
  check(nazwa + ': nieudana komenda cofa checkbox (wzorem aplikacji)',
    /sendCommand\(v \? 'histfs_wl' : 'histfs_wyl'\)\.then\(ok =>/.test(src));
  check(nazwa + ': kafelek data-menu="histfs" i MENUS.histfs',
    src.includes('data-menu="histfs"') && /histfs:\s*\(\)\s*=>\s*showHistFsSheet\(\)/.test(src));
}

// Symulacja petli flag z applyIncomingData: d.hist_fs = 1 -> S.hist_fs = true,
// a klucz trafia do receivedFields (czyli hasData('hist_fs') === true).
(function symulacja() {
  const receivedFields = new Set();
  const S = {};
  const d = { hist_fs: 1 };
  const flagKeys = ['hist_fs'];
  flagKeys.forEach(key => {
    const raw = d[key];
    const valid = typeof raw === 'boolean' || (typeof raw === 'number' && (raw === 0 || raw === 1));
    if (valid) { const value = raw === true || raw === 1; receivedFields.add(key); S[key] = value; }
  });
  check('symulacja: hist_fs=1 daje S.hist_fs=true i hasData', S.hist_fs === true && receivedFields.has('hist_fs'));
  const S2 = {}; const rf2 = new Set();
  const d2 = {};
  ['hist_fs'].forEach(key => {
    const raw = d2[key];
    const valid = typeof raw === 'boolean' || (typeof raw === 'number' && (raw === 0 || raw === 1));
    if (valid) rf2.add(key);
  });
  check('symulacja: brak pola (3.57.3 i starsze) => brak danych', rf2.has('hist_fs') === false);
})();

// ───────────────────────────────────────────────── D. bilans nawiasow w C++
section('D. Bilans nawiasow main_centrala.cpp (bez stringow i komentarzy)');

(function bilans() {
  const s = fw;
  const n = s.length;
  const counts = { '{': 0, '}': 0, '(': 0, ')': 0, '[': 0, ']': 0 };
  let i = 0;
  while (i < n) {
    const c = s[i];
    if (c === '/' && s[i + 1] === '/') { const j = s.indexOf('\n', i); i = j < 0 ? n : j + 1; continue; }
    if (c === '/' && s[i + 1] === '*') { const j = s.indexOf('*/', i + 2); i = j < 0 ? n : j + 2; continue; }
    if (c === 'R' && s[i + 1] === '"') {
      const k = s.indexOf('(', i + 2);
      const tag = s.slice(i + 2, k);
      const end = s.indexOf(')' + tag + '"', k + 1);
      i = end < 0 ? n : end + tag.length + 2;
      continue;
    }
    if (c === '"' || c === "'") {
      const q = c; i++;
      while (i < n) {
        if (s[i] === '\\') { i += 2; continue; }
        if (s[i] === q) { i++; break; }
        i++;
      }
      continue;
    }
    if (Object.prototype.hasOwnProperty.call(counts, c)) counts[c]++;
    i++;
  }
  check('{} ' + counts['{'] + '/' + counts['}'], counts['{'] === counts['}']);
  check('() ' + counts['('] + '/' + counts[')'], counts['('] === counts[')']);
  check('[] ' + counts['['] + '/' + counts[']'], counts['['] === counts[']']);
})();

// ─────────────────────────────────────────────── E. [3.57.7] Telegram i brak pliku
section('E. Telegram: brak pliku to nie blad wysylki [3.57.7]');

check('helper sprawdza istnienie i rozmiar pliku historii',
  /static bool historiaPlikNaFlashu\(const char \*sciezka\)[\s\S]{0,500}?systemFS196\.exists\(sciezka\)/.test(fw));
check('TG_HIST pyta o plik PRZED wysylka krotkiej historii',
  fw.indexOf('const bool jestS = historiaPlikNaFlashu(HISTORIA_PLIK);') > 0 &&
  fw.indexOf('const bool jestS = historiaPlikNaFlashu(HISTORIA_PLIK);') <
    fw.indexOf('if (jestS) okS = sendTelegramDocumentHistKrotka();'));
check('TG_HIST pyta o plik PRZED wysylka dlugiej historii',
  fw.includes('const bool jestL = historiaPlikNaFlashu(HISTORIA_DLUGA_PLIK);') &&
  fw.includes('if (jestL) okL = sendTelegramDocumentHistDluga();'));
check('komunikat rozroznia pusta historie i wylaczony zapis',
  fw.includes('historia na flashu jest pusta') &&
  fw.includes('zapis historii na flash jest WYLACZONY'));
check('brak pliku nie konczy sie komunikatem o bledzie wysylki',
  fw.includes('else sendTelegramMessage(historiaBrakPlikuKomunikat(\"hist_short.csv\"));') &&
  fw.includes('else sendTelegramMessage(historiaBrakPlikuKomunikat(\"hist_long.csv\"));'));

console.log('\n' + ok + ' OK / ' + fail + ' FAIL');
process.exit(fail ? 1 : 0);
