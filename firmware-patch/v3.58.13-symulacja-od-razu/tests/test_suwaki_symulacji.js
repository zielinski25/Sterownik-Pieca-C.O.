// Test suwakow symulacji (v3.58.12 + v3.58.13): wycina symulacjaSekcjaHtml()
// z podanego pliku i odtwarza scenariusz z telefonu: symulacja aktywna, operator
// rusza suwakiem (bez fokusu), refreshSheet() co 1 s NIE moze cofnac suwaka;
// [3.58.13] po potwierdzonym Zastosuj status sterownika przez jakis czas niesie
// jeszcze STARY odczyt (czujnik z harmonogramu co 60-120 s) - suwak nie moze
// wrocic do starej wartosci; gdy odczyt sie zmieni, suwaki znow go sledza.
const fs = require('fs');
const { JSDOM } = require('jsdom');
const plik = process.argv[2];
let src = fs.readFileSync(plik, 'utf8');
if (plik.endsWith('.cpp')) src = src.slice(src.indexOf('R"PIECHTML(') + 11, src.indexOf(')PIECHTML";'));
const od = src.indexOf('  function symulacjaSekcjaHtml(pole, etykieta, jednostka, min, max, krok, statusKey) {');
const doo = src.indexOf('  // ───────────────────────── 9. MENU USTAWIEŃ', od);
if (od < 0 || doo < 0) { console.log('nie znaleziono funkcji'); process.exit(2); }
const fn = src.slice(od, doo);

const dom = new JSDOM('<!DOCTYPE html><div id="sheet"><div class="body"></div></div>');
const { window } = dom; const { document } = window;
const $ = (q, r = document) => r.querySelector(q);
const $$ = (q, r = document) => Array.from(r.querySelectorAll(q));
const h = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };
const liveFns = {}; let liveId = 0;
const live$ = (el, fn) => { const id = 'l' + (++liveId); el.dataset.live = id; liveFns[id] = fn; fn(el); return el; };
function refreshSheet() { $$('[data-live]', $('#sheet')).forEach(el => { const f = liveFns[el.dataset.live]; if (f) f(el); }); }
const S = { sym: { bojler: { active: true, min: 60 } }, t_bojler: 21.0, online: true };
const SYM_POLE_STATUS_KLUCZ = { bojler: 't_bojler' }, SYM_BOUNDS = {};
const fmt1 = v => (+v).toFixed(1);
const val = (pole, key) => S[key];
const symAktywna = pole => !!(S.sym[pole] && S.sym[pole].active);
const hasData = () => false;
let _isDemoMode = false, _isFirebaseConnected = true, _lastFirebaseFetch = Date.now();
const wyslane = [];
// odczytOdRazu=false: jak firmware <= 3.58.12 - ACK ustawia flage symulacji, ale
// t_bojler zmieni sie dopiero przy kolejnym odczycie czujnika (do 60 s).
// odczytOdRazu=true: jak firmware 3.58.13 (symulacjaUstawOdczytOdRazu()).
let odczytOdRazu = false;
async function sendCommand(c) { wyslane.push(c); const m = c.match(/^symuluj bojler (\S+) (\d+)/); if (m) { if (odczytOdRazu) S.t_bojler = +m[1]; S.sym.bojler = { active: true, min: +m[2] }; } if (c.startsWith('symuluj_stop')) S.sym.bojler = { active: false }; return true; }
function renderDashboard() {}
const symulacjaSekcjaHtml = new Function('h', '$', '$$', 'live$', 'refreshSheet', 'S', 'SYM_POLE_STATUS_KLUCZ', 'SYM_BOUNDS', 'fmt1', 'val', 'symAktywna', 'hasData', '_isDemoMode', '_isFirebaseConnected', '_lastFirebaseFetch', 'sendCommand', 'renderDashboard', 'document', 'Date',
  fn + '\n return symulacjaSekcjaHtml;')(h, $, $$, live$, refreshSheet, S, SYM_POLE_STATUS_KLUCZ, SYM_BOUNDS, fmt1, val, symAktywna, hasData, _isDemoMode, _isFirebaseConnected, _lastFirebaseFetch, sendCommand, renderDashboard, document, Date);

let bledy = 0;
const sprawdz = (ok, co) => { console.log((ok ? '  [ OK ] ' : '  [BLAD] ') + co); if (!ok) bledy++; };
const wrap = symulacjaSekcjaHtml('bojler', 'Bojler C.W.U.', '°C', 10, 95, 0.5, 't_bojler');
$('#sheet .body').appendChild(wrap);
const inpVal = $('#sym-bojler-wartosc'), txtVal = $('#sym-bojler-wartoscVal'), inpTime = $('#sym-bojler-czas'), txtTime = $('#sym-bojler-czasVal');
sprawdz(inpVal.value === '21' && txtVal.textContent === '21.0 °C' && inpTime.value === '60', 'start: suwak 21.0 °C / 60 min (symulacja aktywna na sterowniku)');

// operator dotyka suwaka na telefonie: touchstart/pointerdown + input, BEZ fokusu
inpVal.dispatchEvent(new window.Event('pointerdown', { bubbles: true }));
inpVal.value = '45'; inpVal.dispatchEvent(new window.Event('input', { bubbles: true }));
sprawdz(document.activeElement !== inpVal, 'suwak nie ma fokusu (jak na telefonie)');
sprawdz(txtVal.textContent === '45.0 °C', 'napis obok suwaka: 45.0 °C');
for (let s = 0; s < 5; s++) refreshSheet();   // 5 s odswiezania arkusza
sprawdz(inpVal.value === '45' && txtVal.textContent === '45.0 °C', 'po 5 odswiezeniach arkusza suwak DALEJ na 45.0 (dotad wracal do 21.0)');
S.sym.bojler.min = 59; refreshSheet();
sprawdz(inpTime.value === '59' && txtTime.textContent === '59 min', 'czas trwania (nieruszany) nadal podaza za sterownikiem: 59 min');
inpTime.value = '30'; inpTime.dispatchEvent(new window.Event('input', { bubbles: true }));
S.sym.bojler.min = 58; refreshSheet();
sprawdz(inpTime.value === '30', 'ruszony suwak czasu tez nie jest cofany (30 min)');

(async () => {
  $('#sym-bojler-btn-apply').click();
  await new Promise(r => setTimeout(r, 10));
  sprawdz(wyslane[0] === 'symuluj bojler 45.0 30', 'Zastosuj wysyla to, co ustawil operator: "symuluj bojler 45.0 30"');
  // [3.58.13] ACK jest, flaga symulacji jest, ale t_bojler w statusie to wciaz 21.0
  for (let s = 0; s < 8; s++) refreshSheet();
  sprawdz(inpVal.value === '45' && txtVal.textContent === '45.0 °C', 'po Zastosuj, gdy status niesie jeszcze stary odczyt 21.0 (8 odswiezen): suwak ZOSTAJE na 45.0 (dotad wracal do 21.0)');
  sprawdz(inpTime.value === '30', 'czas trwania w tym czasie tez zostaje na 30 min');
  S.t_bojler = 45.0; S.sym.bojler.min = 29; refreshSheet();
  sprawdz(inpVal.value === '45' && inpTime.value === '29', 'sterownik zglosil 45.0 -> suwaki znow podazaja za sterownikiem (45.0 °C, 29 min)');
  // sterownik dostal inna wartosc z Telegrama - suwak (nieruszany) ma ja pokazac
  S.t_bojler = 50.0; refreshSheet();
  sprawdz(inpVal.value === '50', 'zmiana z innego zrodla (np. Telegram) jest widoczna: 50.0 °C');
  // operator rusza, potem Wylacz teraz
  inpVal.value = '70'; inpVal.dispatchEvent(new window.Event('input', { bubbles: true }));
  refreshSheet(); sprawdz(inpVal.value === '70', 'edycja 70.0 nie jest cofana');
  $('#sym-bojler-btn-stop').click();
  await new Promise(r => setTimeout(r, 10));
  sprawdz(wyslane[1] === 'symuluj_stop bojler', 'Wylacz teraz: "symuluj_stop bojler"');
  refreshSheet();
  sprawdz(inpVal.value === '70', 'po wylaczeniu suwak zostaje tam, gdzie go zostawil operator (symulacja nieaktywna - nic nie nadpisuje)');
  S.sym.bojler = { active: true, min: 15 }; S.t_bojler = 33.0; refreshSheet();
  sprawdz(inpVal.value === '33' && inpTime.value === '15', 'nowa symulacja z zewnatrz po wylaczeniu: suwaki pokazuja jej parametry (33.0 °C, 15 min)');
  // [3.58.13] firmware z odczytem od razu: po ACK status juz niesie zadana wartosc -
  // blokada po ACK zwalnia sie natychmiast, "Czas trwania" odlicza bez przerwy
  odczytOdRazu = true;
  inpVal.value = '60'; inpVal.dispatchEvent(new window.Event('input', { bubbles: true }));
  inpTime.value = '20'; inpTime.dispatchEvent(new window.Event('input', { bubbles: true }));
  $('#sym-bojler-btn-apply').click();
  await new Promise(r => setTimeout(r, 10));
  sprawdz(wyslane[2] === 'symuluj bojler 60.0 20' && S.t_bojler === 60, 'Zastosuj 60.0/20 na firmware 3.58.13: status od razu 60.0');
  refreshSheet(); S.sym.bojler.min = 19; refreshSheet();
  sprawdz(inpVal.value === '60' && inpTime.value === '19', 'suwak 60.0, a czas trwania odlicza (19 min) - blokada po ACK nie zamraza odliczania');
  // stary odczyt wraca? nie - ale inna wartosc z zewnatrz (Telegram 70) ma byc widoczna
  S.t_bojler = 70.0; refreshSheet();
  sprawdz(inpVal.value === '70', 'zmiana z Telegrama po Zastosuj widoczna: 70.0 °C');
  console.log(bledy ? `NIEZALICZONY: ${bledy} bledow` : 'ZALICZONY');
  process.exit(bledy ? 1 : 0);
})();
