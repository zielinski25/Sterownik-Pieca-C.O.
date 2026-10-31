#!/usr/bin/env node
// 218: panel WWW na prawdziwym silniku JS (node:vm), bez przegladarki.
//
// Co jest PRAWDZIWE: skrypt panelu wyciagniety z main_centrala.cpp v3.56.4
// (Panel218.html) uruchamiany silnikiem V8, prawdziwe encodeURIComponent,
// prawdziwe Promises.
//
// Co jest MODELEM (i czym to grozi): DOM. Modelowana jest DOKLADNIE ta czesc
// DOM, ktorej panel uzywa: getElementById, addEventListener/click,
// style.display, textContent, window.location.href, fetch. Kazde uzycie
// czegos spoza modelu konczy sie wyjatkiem i testem CZERWONYM - model nie
// moze wiec "po cichu udawac, ze dziala".
//
// Czego ten test NIE sprawdza: wygladu, CSS, tego czy przegladarka w telefonie
// narysuje przycisk. Sprawdza LOGIKE: jakie adresy panel wola i w jakiej
// kolejnosci - i te adresy przekazuje dalej do Test.cpp (urls.json), zeby
// sprawdzic je na prawdziwej tabeli tras.
//
// Uruchomienie: node panel218.mjs Panel218.html urls.json
'use strict';
import fs from 'node:fs';
import vm from 'node:vm';

const [, , plikPanelu, plikWyjscia] = process.argv;
if (!plikPanelu) {
  console.error('uzycie: node panel218.mjs Panel218.html [urls.json]');
  process.exit(2);
}

let sprawdzen = 0;
let bledow = 0;
function sprawdz(warunek, co) {
  ++sprawdzen;
  if (!warunek) {
    ++bledow;
    console.log('FAIL [panel218] ' + co);
  }
}

// ─── model DOM zbudowany z PRAWDZIWEGO fragmentu HTML ─────────────────────
function atrybuty(tag) {
  const wynik = {};
  const wzor = /([a-zA-Z_:][-a-zA-Z0-9_:.]*)\s*=\s*"([^"]*)"/g;
  let m;
  while ((m = wzor.exec(tag)) !== null) wynik[m[1].toLowerCase()] = m[2];
  return wynik;
}

function stylZAtrybutu(tekst) {
  const styl = {};
  (tekst || '').split(';').forEach(function (czesc) {
    const i = czesc.indexOf(':');
    if (i < 0) return;
    styl[czesc.slice(0, i).trim()] = czesc.slice(i + 1).trim();
  });
  return styl;
}

function zbudujDom(html) {
  const elementy = new Map();
  const kolejnosc = [];
  const wzor = /<(\w+)([^>]*)>/g;
  let m;
  while ((m = wzor.exec(html)) !== null) {
    const atrybutyTag = atrybuty(m[2]);
    const id = atrybutyTag.id;
    if (!id) continue;
    const el = {
      tag: m[1].toLowerCase(),
      id: id,
      atrybuty: atrybutyTag,
      style: stylZAtrybutu(atrybutyTag.style),
      textContent: '',
      innerHTML: '',
      className: atrybutyTag.class || '',
      _sluchacze: {},
      _klikniecia: 0,
      addEventListener: function (typ, fn) {
        (this._sluchacze[typ] = this._sluchacze[typ] || []).push(fn);
      },
      removeEventListener: function (typ, fn) {
        const l = this._sluchacze[typ] || [];
        this._sluchacze[typ] = l.filter(function (x) { return x !== fn; });
      },
      click: function () {
        ++this._klikniecia;
        const l = this._sluchacze['click'] || [];
        for (const fn of l) fn.call(this, { type: 'click', target: this });
      },
    };
    if (elementy.has(id)) { bledow++; sprawdzen++; console.log('FAIL [panel218] powtorzony id=' + id); }
    elementy.set(id, el);
    kolejnosc.push(id);
  }
  return { elementy: elementy, kolejnosc: kolejnosc };
}

// ─── odczyt panelu ────────────────────────────────────────────────────────
const html = fs.readFileSync(plikPanelu, 'utf8');

sprawdz(html.indexOf('<script>') > 0, 'panel ma blok <script>');
sprawdz(html.lastIndexOf('</script>') > html.indexOf('<script>'), 'blok <script> jest domkniety');
sprawdz((html.match(/<script>/g) || []).length === 1, 'dokladnie jeden blok <script> w panelu');

const dom = zbudujDom(html);
for (const wymagany of ['histBtn', 'histPanel', 'histInfo', 'histShort', 'histLong']) {
  sprawdz(dom.elementy.has(wymagany), 'element id=' + wymagany + ' jest w panelu');
}
if (bledow > 0) {
  console.log('FAIL [panel218] panel nie ma wszystkich elementow - skrypt wyszedlby wczesniej ("if (!przycisk || !panel) return")');
  process.exit(1);
}

// Kolejnosc w HTML: przyciski przed skryptem (inaczej getElementById zwroci null).
const pozycjaSkryptu = html.indexOf('<script>');
for (const id of ['histBtn', 'histPanel', 'histShort', 'histLong', 'histInfo']) {
  const pozycja = html.indexOf('id="' + id + '"');
  sprawdz(pozycja >= 0 && pozycja < pozycjaSkryptu, 'element id=' + id + ' jest PRZED skryptem');
}

const kod = html.slice(html.indexOf('<script>') + '<script>'.length, html.lastIndexOf('</script>'));

// ─── zaslepki: fetch i location ───────────────────────────────────────────
let bilet = '0123456789abcdef0123456789abcdef';   // 32 znaki hex, jak z centrali
let trybFetch = 'ok';                              // ok | krotki | http503 | siec
const wywolaniaFetch = [];
const location = { href: 'http://sterownik.local/' };

function fetchZaslepka(url, opcje) {
  wywolaniaFetch.push({ url: url, opcje: opcje });
  if (trybFetch === 'siec') return Promise.reject(new Error('Failed to fetch'));
  if (trybFetch === 'http503') {
    return Promise.resolve({ ok: false, status: 503, text: function () { return Promise.resolve(''); } });
  }
  const tresc = trybFetch === 'krotki' ? bilet.slice(0, 31) : bilet;
  return Promise.resolve({ ok: true, status: 200, text: function () { return Promise.resolve(tresc); } });
}

const piaskownica = {
  document: {
    getElementById: function (id) { return dom.elementy.get(id) || null; },
    querySelector: function () { throw new Error('panel uzywa querySelector - model go nie ma'); },
    addEventListener: function () {},
  },
  window: { location: location },
  fetch: fetchZaslepka,
  encodeURIComponent: encodeURIComponent,
  decodeURIComponent: decodeURIComponent,
  Promise: Promise,
  Error: Error,
  console: console,
  setTimeout: setTimeout,
  clearTimeout: clearTimeout,
};
piaskownica.window.document = piaskownica.document;
piaskownica.globalThis = piaskownica;

// ─── uruchomienie PRAWDZIWEGO skryptu panelu ──────────────────────────────
try {
  vm.runInNewContext(kod, piaskownica, { filename: 'panel218.js' });
  sprawdz(true, 'skrypt panelu wykonal sie bez wyjatku');
} catch (e) {
  sprawdz(false, 'skrypt panelu rzucil wyjatek: ' + e.message);
  console.log('PASS panel218 sprawdzen=' + sprawdzen + ' bledow=' + bledow);
  process.exit(1);
}

const przycisk = dom.elementy.get('histBtn');
const panel = dom.elementy.get('histPanel');
const info = dom.elementy.get('histInfo');
const krotka = dom.elementy.get('histShort');
const dluga = dom.elementy.get('histLong');

sprawdz((przycisk._sluchacze['click'] || []).length === 1, 'przycisk HISTORIA CSV ma jeden obsluznik klik');
sprawdz((krotka._sluchacze['click'] || []).length === 1, 'przycisk "Krotka (24 h)" ma jeden obsluznik klik');
sprawdz((dluga._sluchacze['click'] || []).length === 1, 'przycisk "Dluga (archiwum)" ma jeden obsluznik klik');
sprawdz(panel.style.display === 'none', 'panel startuje schowany (display:none z HTML)');

// Otwieranie i zamykanie panelu.
przycisk.click();
sprawdz(panel.style.display === 'block', 'klik HISTORIA CSV otwiera panel');
przycisk.click();
sprawdz(panel.style.display === 'none', 'drugi klik zamyka panel');
przycisk.click();

function odczekaj() { return new Promise(function (r) { setImmediate(r); }); }

async function scenariusz() {
  const sekwencje = [];

  // ---- KROTKA ----
  trybFetch = 'ok';
  bilet = '11111111111111111111111111111111';
  wywolaniaFetch.length = 0;
  location.href = 'http://sterownik.local/';
  krotka.click();
  sprawdz(info.textContent.indexOf('Przygotowuj') === 0, 'po kliknieciu "Krotka" panel mowi, ze przygotowuje link');
  await odczekaj(); await odczekaj();
  sprawdz(wywolaniaFetch.length === 1, 'jedno zapytanie o bilet dla "Krotka"');
  sprawdz(wywolaniaFetch[0].url === '/history/link?stream=short',
    'adres biletu krotkiego: ' + (wywolaniaFetch[0] || {}).url);
  sprawdz(wywolaniaFetch[0].opcje && wywolaniaFetch[0].opcje.cache === 'no-store',
    'zapytanie o bilet nie jest cache\'owane');
  sprawdz(location.href === '/history?k=' + bilet, 'krotka otwiera /history?k=<bilet>, jest: ' + location.href);
  sprawdz(location.href.indexOf('&s=') < 0, 'panel nie dokleja nieuzywanego parametru s=');
  sekwencje.push({
    nazwa: 'krotka',
    bilet_url: wywolaniaFetch[0].url,
    pobranie_url: location.href,
  });

  // ---- DLUGA ----
  trybFetch = 'ok';
  bilet = '22222222222222222222222222222222';
  wywolaniaFetch.length = 0;
  location.href = 'http://sterownik.local/';
  dluga.click();
  await odczekaj(); await odczekaj();
  sprawdz(wywolaniaFetch.length === 1, 'jedno zapytanie o bilet dla "Dluga"');
  sprawdz(wywolaniaFetch[0].url === '/history/link?stream=long',
    'adres biletu dlugiego: ' + (wywolaniaFetch[0] || {}).url);
  sprawdz(location.href === '/history/long?k=' + bilet,
    'dluga otwiera /history/long?k=<bilet>, jest: ' + location.href);
  sekwencje.push({
    nazwa: 'dluga',
    bilet_url: wywolaniaFetch[0].url,
    pobranie_url: location.href,
  });

  // ---- BLAD: centrala nie dała biletu (31 znakow) ----
  trybFetch = 'krotki';
  wywolaniaFetch.length = 0;
  location.href = 'http://sterownik.local/';
  info.textContent = '';
  krotka.click();
  await odczekaj(); await odczekaj();
  sprawdz(location.href === 'http://sterownik.local/', 'przy krotkim bilecie NIE ma przekierowania');
  sprawdz(info.textContent.indexOf('Nie uda') === 0, 'przy krotkim bilecie jest komunikat bledu: ' + info.textContent);

  // ---- BLAD: HTTP 503 ----
  trybFetch = 'http503';
  wywolaniaFetch.length = 0;
  location.href = 'http://sterownik.local/';
  info.textContent = '';
  krotka.click();
  await odczekaj(); await odczekaj();
  sprawdz(location.href === 'http://sterownik.local/', 'przy HTTP 503 NIE ma przekierowania');
  sprawdz(info.textContent === 'Nie udało się: HTTP 503', 'komunikat przy 503: ' + info.textContent);

  // ---- BLAD: brak sieci ----
  trybFetch = 'siec';
  info.textContent = '';
  location.href = 'http://sterownik.local/';
  krotka.click();
  await odczekaj(); await odczekaj();
  sprawdz(location.href === 'http://sterownik.local/', 'bez sieci NIE ma przekierowania');
  sprawdz(info.textContent.indexOf('Nie uda') === 0, 'bez sieci jest komunikat bledu');

  // ---- adresy rozne dla obu strumieni ----
  sprawdz(sekwencje[0].pobranie_url !== sekwencje[1].pobranie_url,
    'krotka i dluga nie moga otwierac tego samego adresu');
  sprawdz(sekwencje[0].bilet_url !== sekwencje[1].bilet_url,
    'krotka i dluga prosza o bilet z innym parametrem stream');

  if (plikWyjscia) {
    fs.writeFileSync(plikWyjscia, JSON.stringify({ sekwencje: sekwencje }, null, 2) + '\n', 'utf8');
  }
  return sekwencje;
}

scenariusz().then(function (sekwencje) {
  console.log('PASS panel218 sprawdzen=' + sprawdzen + ' bledow=' + bledow);
  if (bledow > 0) process.exit(1);
  for (const s of sekwencje) {
    console.log('     ' + s.nazwa + ': ' + s.bilet_url + '  ->  ' + s.pobranie_url);
  }
}).catch(function (e) {
  console.log('FAIL [panel218] wyjatek w scenariuszu: ' + (e && e.stack ? e.stack : e));
  console.log('FAIL panel218 sprawdzen=' + sprawdzen);
  process.exit(1);
});
