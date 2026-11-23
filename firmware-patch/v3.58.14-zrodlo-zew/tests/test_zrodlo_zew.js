// Test ZEWN-ZRODLO (v3.58.14): wycina z podanego pliku (main_centrala.cpp z
// R"PIECHTML( albo Piec.html) funkcje applyIncomingData() oraz blok
// ZEWN_ZRODLA / zewnZrodloSekcjaHtml() i uruchamia je w jsdom z atrapami.
// Sprawdza wspolzaleznosc: wybor zrodla to pole statusu "zewnZrodlo" z centrali -
// zmiana w aplikacji/Telegramie przychodzi statusem i przelacza kafelek na
// stronie; klikniecie na stronie wysyla "ustaw zewnZrodlo N" i NIE przelacza
// nic lokalnie, dopoki centrala nie potwierdzi statusem.
const fs = require('fs');
const { JSDOM } = require('jsdom');
const plik = process.argv[2];
let src = fs.readFileSync(plik, 'utf8');
if (plik.endsWith('.cpp')) src = src.slice(src.indexOf('R"PIECHTML(') + 11, src.indexOf(')PIECHTML";'));
function wytnij(start, stop) {
  const od = src.indexOf(start); if (od < 0) { console.log('nie znaleziono: ' + start.slice(0, 50)); process.exit(2); }
  const doo = src.indexOf(stop, od); if (doo < 0) { console.log('nie znaleziono konca: ' + stop.slice(0, 50)); process.exit(2); }
  return src.slice(od, doo);
}
const fnApply = wytnij('  function applyIncomingData(d) {', '  function updateConnectionStateUI() {');
const fnZrodlo = wytnij('  const ZEWN_ZRODLA = [', '  function sensorModalContent(sen) {');

const dom = new JSDOM('<!DOCTYPE html><div id="sheet"><div class="body"></div></div>');
const { window } = dom; const { document } = window;
const $ = (q, r = document) => r.querySelector(q);
const $$ = (q, r = document) => Array.from(r.querySelectorAll(q));
const h = (tag, cls, html) => { const e = document.createElement(tag); if (cls) e.className = cls; if (html !== undefined) e.innerHTML = html; return e; };
const liveFns = {}; let liveId = 0;
const live$ = (el, fn) => { const id = 'l' + (++liveId); el.dataset.live = id; liveFns[id] = fn; fn(el); return el; };
function refreshSheet() { $$('[data-live]', $('#sheet')).forEach(el => { const f = liveFns[el.dataset.live]; if (f) f(el); }); }
function seg(options, current, onPick) {   // kopia seg() ze strony (wersja z getCurrent)
  const s = h('div', 'seg');
  const getCurrent = typeof current === 'function' ? current : null;
  const syncSelection = (el = s) => { const selected = getCurrent ? getCurrent() : current; $$('.seg button', el).forEach((b, i) => b.classList.toggle('on', options[i][0] === selected)); };
  options.forEach(([v, t]) => {
    const b = h('button', v === (getCurrent ? getCurrent() : current) ? 'on' : '', t);
    b.onclick = () => { const r = onPick(v); const select = () => $$('.seg button', s).forEach(x => x.classList.toggle('on', x === b));
      if (r && typeof r.then === 'function') { $$('.seg button', s).forEach(x => x.disabled = true); r.then(c => { if (c) getCurrent ? syncSelection() : select(); }).finally(() => $$('.seg button', s).forEach(x => x.disabled = false)); } else select(); };
    s.appendChild(b);
  });
  if (getCurrent) live$(s, syncSelection);
  return s;
}
// --- atrapy stanu strony ---
const S = { sym: {}, t_zewn: NaN, t_zewn_ds: NaN, t_zewn_2: NaN, zewnZrodlo: NaN, online: false };
const real = {};
const receivedFields = new Set();
const SYM_POLA = { zewn: 't_zewn', bojler: 't_bojler', ogrz: 't_ogrz', panel: 't_panel', pokoj: 't_pokoj', ogrz_powrot: 't_powrot', ogrz_trociny: 't_trociny', cisnienie: 'cisnienie', wilgotnosc: 'wilgotnosc', dym: 'dym' };
const SYM_BOUNDS = { zewn: [-50, 60], bojler: [0, 120], ogrz: [0, 120], panel: [-30, 150], pokoj: [0, 50], ogrz_powrot: [0, 120], ogrz_trociny: [0, 120], cisnienie: [900, 1100], wilgotnosc: [0, 100], dym: [0, 4095] };
const symAktywna = pole => !!S.sym[pole];
const hasData = key => receivedFields.has(key) || _isDemoMode;
const fmt1 = v => (typeof v === 'number' && !isNaN(v)) ? (Math.round(v * 10) / 10).toFixed(1) : '—';
const hist = []; let _isDemoMode = false; const weatherDataIsReal = false, weatherData = null;
const currentHourSolarRadiation = () => NaN;
const noop = () => {};
const wyslane = [];
async function sendCommand(c) { wyslane.push(c); return true; }   // ACK z centrali - ale status jeszcze stary
const applyIncomingData = new Function('_isDemoMode', 'receivedFields', 'S', 'real', 'SYM_POLA', 'SYM_BOUNDS', 'symAktywna', 'hist', 'updateConnectionStateUI', 'renderDashboard', 'refreshSheet', 'updateChartsLiveFeed', 'weatherDataIsReal', 'weatherData', 'currentHourSolarRadiation',
  fnApply + '\n return applyIncomingData;')(_isDemoMode, receivedFields, S, real, SYM_POLA, SYM_BOUNDS, symAktywna, hist, noop, noop, refreshSheet, noop, weatherDataIsReal, weatherData, currentHourSolarRadiation);
const zr = new Function('h', '$', '$$', 'live$', 'seg', 'S', 'hasData', 'fmt1', 'symAktywna', 'sendCommand', '_isDemoMode', 'document',
  fnZrodlo + '\n return { ZEWN_ZRODLA, zewnZrodloNr, zewnZrodloMagistrala, zewnZrodloSekcjaHtml };')(h, $, $$, live$, seg, S, hasData, fmt1, symAktywna, sendCommand, _isDemoMode, document);

let bledy = 0;
const sprawdz = (ok, co) => { console.log((ok ? '  [ OK ] ' : '  [BLAD] ') + co); if (!ok) bledy++; };
const baza = { online: true, t_ogrz: 55.0, t_bojler: 48.0, t_zewn: 3.5, t_zewn_2: 4.2, zewnZrodlo: 1, fw: 'v3.58.14' };

// 1. status z centrali: zrodlo 1 (DS18B20)
applyIncomingData(Object.assign({}, baza));
sprawdz(S.t_zewn === 3.5 && S.t_zewn_ds === 3.5 && S.t_zewn_2 === 4.2 && S.zewnZrodlo === 1, 'status zewnZrodlo=1: kafelek t_zewn = DS18B20 3.5, oba surowe odczyty dostepne (3.5 / 4.2)');
sprawdz(receivedFields.has('t_zewn') && receivedFields.has('t_zewn_ds') && receivedFields.has('t_zewn_2') && receivedFields.has('zewnZrodlo'), 'pola t_zewn, t_zewn_ds, t_zewn_2, zewnZrodlo oznaczone jako odebrane');
sprawdz(zr.zewnZrodloNr() === 1 && zr.zewnZrodloMagistrala('x') === 'DS18B20 · Magistrala 1-Wire', 'naglowek arkusza: magistrala DS18B20');

// 2. arkusz: sekcja wyboru
const wrap = zr.zewnZrodloSekcjaHtml(); $('#sheet .body').appendChild(wrap);
const btn = $$('.seg button', wrap);
sprawdz(btn.length === 2 && btn[0].classList.contains('on') && !btn[1].classList.contains('on'), 'dwa przyciski, podswietlony Zew 1');
sprawdz(btn[0].innerHTML.includes('Zew 1') && btn[0].innerHTML.includes('3.5 °C') && btn[1].innerHTML.includes('Zew 2') && btn[1].innerHTML.includes('4.2 °C'), 'przyciski pokazuja biezace odczyty obu czujnikow (3.5 / 4.2)');
sprawdz(/Aktywne: DS18B20/.test($('.sym-info', wrap).textContent), 'opis: "Aktywne: DS18B20 ..."');

(async () => {
  // 3. klikniecie Zew 2 na stronie: komenda do centrali, lokalnie NIC sie nie przelacza
  btn[1].click();
  await new Promise(r => setTimeout(r, 10));
  sprawdz(wyslane[0] === 'ustaw zewnZrodlo 2', 'klikniecie Zew 2 wysyla "ustaw zewnZrodlo 2"');
  sprawdz(S.t_zewn === 3.5 && btn[0].classList.contains('on') && !btn[1].classList.contains('on'), 'po ACK, ale przed nowym statusem: kafelek i podswietlenie bez zmian (zadnego stanu optymistycznego)');
  // 4. centrala potwierdza statusem (ta sama droga, ktora widzi aplikacja i LCD)
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 2 })); refreshSheet();
  sprawdz(S.t_zewn === 4.2 && real.t_zewn === 4.2 && S.t_zewn_ds === 3.5, 'status zewnZrodlo=2: kafelek t_zewn = DHT22 4.2, DS18B20 nadal w t_zewn_ds');
  sprawdz(!btn[0].classList.contains('on') && btn[1].classList.contains('on'), 'podswietlenie przeszlo na Zew 2 ze statusu');
  sprawdz(zr.zewnZrodloMagistrala('x') === 'DHT22 · czujnik wilgotności' && /Aktywne: DHT22/.test($('.sym-info', wrap).textContent), 'magistrala i opis: DHT22');
  // 5. zmiana w APLIKACJI (albo Telegramie): przychodzi tylko statusem -> strona podaza
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 1 })); refreshSheet();
  sprawdz(S.t_zewn === 3.5 && btn[0].classList.contains('on') && wyslane.length === 1, 'zmiana z aplikacji (status zewnZrodlo=1): kafelek 3.5 i przycisk Zew 1 - bez zadnej komendy ze strony');
  // 6. Zew 2 wybrane, DHT22 bez odczytu (null) -> BRAK DANYCH, bez cichego DS18B20
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 2, t_zewn_2: null })); refreshSheet();
  sprawdz(Number.isNaN(S.t_zewn) && !receivedFields.has('t_zewn') && S.t_zewn_ds === 3.5, 'zewnZrodlo=2 + t_zewn_2=null: kafelek BRAK DANYCH (t_zewn NaN), DS18B20 widoczny tylko jako Zew 1');
  sprawdz(/brak świeżego odczytu DHT22/.test($('.sym-info', wrap).textContent) && btn[1].innerHTML.includes('brak odczytu'), 'opis i przycisk Zew 2: brak odczytu');
  // 7. symulacja "zewn" aktywna na sterowniku ma pierwszenstwo nad zrodlem
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 2, symulacja_zewn: true, symulacja_zewn_min: 30, t_zewn: -12.0 })); refreshSheet();
  sprawdz(S.t_zewn === -12.0 && /symulacja/.test($('.sym-info', wrap).textContent), 'symulacja zewn aktywna: t_zewn = wartosc wymuszona -12.0 mimo zewnZrodlo=2');
  // 8. starsze firmware (bez pola) -> zrodlo 1 jak dotad, wybor niedostepny
  applyIncomingData({ online: true, t_zewn: 3.5, t_zewn_2: 4.2 }); refreshSheet();
  sprawdz(S.t_zewn === 3.5 && zr.zewnZrodloNr() === null && /starsze niż 3.58.14/.test($('.sym-info', wrap).textContent), 'firmware bez pola zewnZrodlo: t_zewn = DS18B20, opis "wybor niedostepny"');
  // 9. smieci w polu -> odrzucone (zakres 1..2, calkowite)
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 3 }));
  const a = Number.isNaN(S.zewnZrodlo) && !receivedFields.has('zewnZrodlo') && S.t_zewn === 3.5;
  applyIncomingData(Object.assign({}, baza, { zewnZrodlo: 1.5 }));
  sprawdz(a && Number.isNaN(S.zewnZrodlo) && S.t_zewn === 3.5, 'zewnZrodlo=3 / 1.5 odrzucone (t_zewn zostaje DS18B20)');
  applyIncomingData(Object.assign({}, baza, { t_zewn_2: 95 }));
  sprawdz(!receivedFields.has('t_zewn_2') && Number.isNaN(S.t_zewn_2) && btn[1].innerHTML.includes('Zew 2'), 't_zewn_2=95 poza zakresem DHT22 (-40..80) odrzucone');
  console.log(bledy ? 'NIEZALICZONY: ' + bledy + ' BLAD' : 'ZALICZONY: wszystkie sprawdzenia OK (' + 18 + ')');
  process.exit(bledy ? 1 : 0);
})();
