#!/usr/bin/env node
// [ETAP 222] Panel WWW z v3.56.4 na PRAWDZIWYM serwerze przez PRAWDZIWE TCP.
//
// Roznica wobec etapu 218: tam serwer byl modelem (Serwer218.h), a pobranie
// tylko "przekierowaniem" do sprawdzenia pozniej. Tutaj fetch i przekierowanie
// naprawde rozmawiaja z binarka most222, w ktorej siedzi prawdziwy parser,
// prawdziwy dyspozytor i prawdziwe odpowiedzi biblioteki. Plik CSV, ktory
// "pobiera" panel, jest NAPRAWDE zapisywany na dysk.
//
// Co dalej jest modelem: DOM (getElementById, addEventListener, style,
// textContent) - dokladnie ta czesc, ktorej panel uzywa. Kazde uzycie czegos
// spoza modelu rzuca wyjatek i test jest CZERWONY.
//
// Uruchomienie: node panel222.mjs Panel222.html <port> <katalog> <tryb>
//   tryb: token | bez-tokenu
'use strict';
import fs from 'node:fs';
import http from 'node:http';
import path from 'node:path';
import vm from 'node:vm';

const [, , plikPanelu, portTekst, katalog, tryb] = process.argv;
const PORT = Number(portTekst);
if (!plikPanelu || !PORT || !katalog || !tryb) {
  console.error('uzycie: node panel222.mjs Panel222.html <port> <katalog> <token|bez-tokenu>');
  process.exit(2);
}

// Budzik: jesli centrala nie zakonczy odpowiedzi (np. zapowie 81 bajtow,
// a wysle 0), zapytanie wisi. Bez budzika node wyszedlby po cichu z kodem 0
// i test wyszedlby "zielony" tylko dlatego, ze nic nie sprawdzil.
const BUDZIK_MS = 25000;
const budzik = setTimeout(function () {
  console.log('FAIL [panel222] przekroczono czas ' + BUDZIK_MS + ' ms - odpowiedz nie doszla do konca');
  process.exit(1);
}, BUDZIK_MS);

let sprawdzen = 0;
let bledow = 0;
function sprawdz(warunek, co) {
  ++sprawdzen;
  if (!warunek) {
    ++bledow;
    console.log('FAIL [panel222] ' + co);
  }
}

// ─── prawdziwe HTTP do mostka ─────────────────────────────────────────────
function zadanieHttp(sciezka) {
  return new Promise(function (resolve, reject) {
    const req = http.request(
      { host: '127.0.0.1', port: PORT, path: sciezka, method: 'GET', agent: false, headers: { Host: 'sterownik' } },
      function (res) {
        const czesci = [];
        res.on('data', function (d) { czesci.push(d); });
        res.on('end', function () {
          resolve({ status: res.statusCode, naglowki: res.headers, cialo: Buffer.concat(czesci) });
        });
      }
    );
    req.on('error', reject);
    req.end();
  });
}

function nazwaZPliku(naglowki) {
  const cd = naglowki['content-disposition'] || '';
  const m = /filename="([^"]+)"/.exec(cd);
  return m ? m[1] : null;
}

// ─── model DOM (ten sam co w etapie 218) ──────────────────────────────────
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


// ─── prawdziwe fetch i location ───────────────────────────────────────────
const wywolaniaFetch = [];
let biezacyHref = 'http://sterownik.local/';
const pobrania = [];   // { url, status, plik, bajtow, tresc }

function fetchZaslepka(url, opcje) {
  wywolaniaFetch.push({ url: url, opcje: opcje });
  return zadanieHttp(url).then(function (r) {
    return {
      ok: r.status >= 200 && r.status < 300,
      status: r.status,
      text: function () { return Promise.resolve(r.cialo.toString('utf8')); },
    };
  });
}

const location = {};
Object.defineProperty(location, 'href', {
  get: function () { return biezacyHref; },
  set: function (v) {
    biezacyHref = v;
    pobrania.push(
      zadanieHttp(v).then(function (r) {
        const plik = nazwaZPliku(r.naglowki);
        const wpis = { url: v, status: r.status, plik: plik, bajtow: r.cialo.length, tresc: r.cialo };
        if (plik) fs.writeFileSync(path.join(katalog, plik), r.cialo);
        return wpis;
      })
    );
  },
});

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
  vm.runInNewContext(kod, piaskownica, { filename: 'panel222.js' });
  sprawdz(true, 'skrypt panelu wykonal sie bez wyjatku');
} catch (e) {
  sprawdz(false, 'skrypt panelu rzucil wyjatek: ' + e.message);
  console.log('PASS panel222 sprawdzen=' + sprawdzen + ' bledow=' + bledow);
  process.exit(1);
}

const przycisk = dom.elementy.get('histBtn');
const panel = dom.elementy.get('histPanel');
const info = dom.elementy.get('histInfo');
const krotkaBtn = dom.elementy.get('histShort');
const dlugaBtn = dom.elementy.get('histLong');

sprawdz((przycisk._sluchacze['click'] || []).length === 1, 'przycisk HISTORIA CSV ma jeden obsluznik klik');
sprawdz((krotkaBtn._sluchacze['click'] || []).length === 1, 'przycisk "Krotka (24 h)" ma jeden obsluznik klik');
sprawdz((dlugaBtn._sluchacze['click'] || []).length === 1, 'przycisk "Dluga (archiwum)" ma jeden obsluznik klik');

function odczekaj() { return new Promise(function (r) { setImmediate(r); }); }

// Czeka, az panel wykona zapytanie o bilet (fetch) - z limitem, zeby test
// nie wisial, gdy panel nic nie robi.
async function czekajNaFetch(ile) {
  for (let i = 0; i < 500; ++i) {
    await odczekaj();
    if (wywolaniaFetch.length >= ile) return;
  }
}

// Czeka na zakonczenie wszystkich pobran (przekierowan na plik).
async function czekajNaPobrania() {
  for (let i = 0; i < 500; ++i) {
    if (pobrania.length === 0) {
      await odczekaj();
      continue;
    }
    const gotowe = await Promise.all(pobrania);
    if (gotowe.every(function (x) { return x && x.status; })) return gotowe;
    await odczekaj();
  }
  throw new Error('pobranie nie zakonczylo sie w limicie czasu');
}

async function scenariusz() {
  const plikKrotki = path.join(path.dirname(katalog), 'dysk', 'littlefs', 'hist_short.csv');
  const plikDlugi = path.join(path.dirname(katalog), 'dysk', 'littlefs', 'hist_long.csv');

  przycisk.click();
  sprawdz(panel.style.display === 'block', 'klik HISTORIA CSV otwiera panel');

  if (tryb === 'token') {
    // ---- KROTKA ----
    wywolaniaFetch.length = 0;
    info.textContent = '';
    krotkaBtn.click();
    sprawdz(info.textContent.indexOf('Przygotowuj') === 0, 'po kliknieciu "Krotka" panel mowi, ze przygotowuje link');
    await czekajNaFetch(1);
    const p1 = await czekajNaPobrania().catch(function (e) { sprawdz(false, 'blad pobrania krotkiego: ' + e.message); return []; });
    sprawdz(wywolaniaFetch.length === 1, 'jedno zapytanie o bilet dla "Krotka"');
    sprawdz((wywolaniaFetch[0] || {}).url === '/history/link?stream=short', 'adres biletu krotkiego: ' + (wywolaniaFetch[0] || {}).url);
    sprawdz(/^\/history\?k=[0-9a-f]{32}$/.test(biezacyHref), 'panel otwiera /history?k=<32 hex>, jest: ' + biezacyHref);
    const w1 = p1[p1.length - 1] || {};
    sprawdz(w1.status === 200, 'pobranie krotkie konczy sie 200, jest: ' + w1.status);
    sprawdz(w1.plik === 'historia-krotka.csv', 'nazwa pliku krotkiego: ' + w1.plik);
    const naDysku = fs.readFileSync(plikKrotki);
    sprawdz(w1.tresc && w1.tresc.equals(naDysku), 'zapisany plik krotki jest bajt w bajt plikiem z centrali (' + naDysku.length + ' B)');
    sprawdz(fs.existsSync(path.join(katalog, 'historia-krotka.csv')), 'plik historia-krotka.csv lezy na dysku');

    // ---- DLUGA ----
    wywolaniaFetch.length = 0;
    dlugaBtn.click();
    await czekajNaFetch(2);
    const p2 = await czekajNaPobrania().catch(function (e) { sprawdz(false, 'blad pobrania dlugiego: ' + e.message); return []; });
    sprawdz((wywolaniaFetch[0] || {}).url === '/history/link?stream=long', 'adres biletu dlugiego: ' + (wywolaniaFetch[0] || {}).url);
    sprawdz(/^\/history\/long\?k=[0-9a-f]{32}$/.test(biezacyHref), 'panel otwiera /history/long?k=<32 hex>, jest: ' + biezacyHref);
    const w2 = p2[p2.length - 1] || {};
    sprawdz(w2.status === 200, 'pobranie dlugie konczy sie 200, jest: ' + w2.status);
    sprawdz(w2.plik === 'historia-dluga.csv', 'nazwa pliku dlugiego: ' + w2.plik);
    const naDysku2 = fs.readFileSync(plikDlugi);
    sprawdz(w2.tresc && w2.tresc.equals(naDysku2), 'zapisany plik dlugi jest bajt w bajt plikiem z centrali (' + naDysku2.length + ' B)');

    // ---- ten sam link drugi raz ----
    const powtorka = await zadanieHttp(biezacyHref);
    sprawdz(powtorka.status === 401, 'ten sam link drugi raz daje 401, jest: ' + powtorka.status);

    // ---- stary bilet krotki nie otwiera dlugiej trasy ----
    const biletKrotki = /^\/history\?k=([0-9a-f]{32})$/.exec(biezacyHref);
    sprawdz(biletKrotki === null, 'po pobraniu dlugim adres jest dlugi (kontrola spójności)');
  } else {
    // ---- profil bez tokenu: centrala odmawia ----
    wywolaniaFetch.length = 0;
    info.textContent = '';
    krotkaBtn.click();
    await czekajNaFetch(1);
    await odczekaj(); await odczekaj(); await odczekaj();
    sprawdz((wywolaniaFetch[0] || {}).url === '/history/link?stream=short', 'panel pyta o bilet tak samo jak z tokenem');
    sprawdz(biezacyHref === 'http://sterownik.local/', 'bez tokenu NIE ma przekierowania na pobranie, jest: ' + biezacyHref);
    sprawdz(info.textContent.indexOf('Nie uda') === 0, 'panel pokazuje komunikat bledu: ' + info.textContent);
    sprawdz(info.textContent.indexOf('503') > 0, 'komunikat zawiera kod HTTP 503: ' + info.textContent);
  }
}

scenariusz()
  .then(function () {
    clearTimeout(budzik);
    console.log('PASS panel222 sprawdzen=' + sprawdzen + ' bledow=' + bledow + ' tryb=' + tryb);
    process.exit(bledow === 0 ? 0 : 1);
  })
  .catch(function (e) {
    clearTimeout(budzik);
    console.log('FAIL [panel222] wyjatek scenariusza: ' + e.stack);
    process.exit(1);
  });
