#!/usr/bin/env node
/* 216: panel WWW w prawdziwym silniku JS (node) i na atrapie DOM.
 *
 * Co jest prawdziwe: PRAWdziwy skrypt panelu, dokladnie taki, jaki firmware
 * wysyla do przegladarki (panel/panel.html z build.py), wykonywany przez
 * prawdziwy silnik JavaScript (V8 z node). Sprawdzamy jego zachowanie:
 * ktory element dotyka, jaki adres wola, co pokazuje przy bledzie.
 *
 * Co NIE jest prawdziwe: DOM. To nie jest przegladarka i nie udaje, ze nia
 * jest - atrapa ma tylko te cztery rzeczy, ktorych skrypt uzywa
 * (getElementById, addEventListener, style.display, textContent) oraz
 * window.location.href. Dlatego ten test NIE dowodzi, ze panel ladnie wyglada;
 * dowodzi, ze logika przycisku prowadzi do wlasciwych adresow i ze kazdy
 * element, o ktory skrypt prosi, naprawde jest w HTML.
 *
 * SEDNO: adresy wychodzace z tego testu (urls.txt) ida do Test.cpp, ktory
 * podaje je PRAWDZIWEJ trasie z firmware. Czyli sprawdzamy szew panel<->trasa,
 * a nie kazda strone osobno.
 *
 * Uruchomienie:
 *   node narzedzia/od-przycisku216/panel_test.mjs panel/panel.html wynik/
 */
import { readFileSync, writeFileSync } from 'node:fs';
import { basename, join } from 'node:path';

const [plikHtml, katalogWy] = process.argv.slice(2);
if (!plikHtml || !katalogWy) {
  console.error('uzycie: node panel_test.mjs <panel.html> <katalog-wynikowy>');
  process.exit(2);
}

const html = readFileSync(plikHtml, 'utf8');

// --- 1. co jest w HTML ------------------------------------------------
const idWTagu = [...html.matchAll(/<([a-zA-Z][a-zA-Z0-9]*)\b([^>]*)>/g)];
const idy = [];
const displayPoczatkowy = new Map();
for (const [, tag, atrybuty] of idWTagu) {
  const m = /\bid="([^"]+)"/.exec(atrybuty);
  if (!m) continue;
  idy.push(m[1]);
  const s = /\bstyle="([^"]*)"/.exec(atrybuty);
  if (s) {
    const d = /(?:^|;)\s*display\s*:\s*([a-z-]+)/i.exec(s[1]);
    if (d) displayPoczatkowy.set(m[1], d[1].toLowerCase());
  }
}

const blokiSkryptu = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)].map((m) => m[1]);

// --- 2. atrapa DOM ----------------------------------------------------
const zadaneIdy = [];
const brakujaceIdy = [];
const elementy = new Map();
const nasluchy = [];

function element(id) {
  const el = {
    id,
    textContent: '',
    style: { display: displayPoczatkowy.get(id) ?? '' },
    _nasluchy: new Map(),
    addEventListener(typ, fn) {
      nasluchy.push({ id, typ });
      if (!el._nasluchy.has(typ)) el._nasluchy.set(typ, []);
      el._nasluchy.get(typ).push(fn);
    },
    click() {
      for (const fn of el._nasluchy.get('click') ?? []) fn({ type: 'click', currentTarget: el });
    },
  };
  return el;
}

const document = {
  getElementById(id) {
    zadaneIdy.push(id);
    if (!idy.includes(id)) {
      brakujaceIdy.push(id);
      return null;
    }
    if (!elementy.has(id)) elementy.set(id, element(id));
    return elementy.get(id);
  },
};

const window = { location: { href: 'https://centrala.local/' } };

const odwolaniaFetch = [];
let kolejkaOdpowiedzi = [];
function fetch(url, opcje) {
  odwolaniaFetch.push({ url, opcje });
  const o = kolejkaOdpowiedzi.length > 0 ? kolejkaOdpowiedzi.shift() : { ok: false, status: 599, body: '' };
  return Promise.resolve({
    ok: o.ok,
    status: o.status,
    text: () => Promise.resolve(o.body),
  });
}

// --- 3. uruchom PRAWDZIWY skrypt --------------------------------------
const bledy = [];
const sprawdzenia = [];
function sprawdz(warunek, nazwa, szczegol = '') {
  sprawdzenia.push({ nazwa, ok: !!warunek, szczegol });
  if (!warunek) bledy.push(`${nazwa}${szczegol ? ' :: ' + szczegol : ''}`);
}

sprawdz(blokiSkryptu.length === 1, 'J01 jeden blok <script> w panelu', `jest ${blokiSkryptu.length}`);
sprawdz(new Set(idy).size === idy.length, 'J02 brak duplikatow id w HTML',
  idy.filter((x, i) => idy.indexOf(x) !== i).join(','));

let bladUruchomienia = null;
try {
  // eslint-disable-next-line no-new-func
  new Function('document', 'window', 'fetch', blokiSkryptu[0])(document, window, fetch);
} catch (e) {
  bladUruchomienia = e;
}
sprawdz(bladUruchomienia === null, 'J03 skrypt wykonuje sie bez wyjatku',
  bladUruchomienia ? String(bladUruchomienia.message) : '');
sprawdz(brakujaceIdy.length === 0, 'J04 kazdy element, o ktory prosi skrypt, istnieje w HTML',
  brakujaceIdy.join(','));

const przycisk = elementy.get('histBtn');
const panel = elementy.get('histPanel');
const info = elementy.get('histInfo');
sprawdz(!!przycisk && !!panel, 'J05 przycisk i panel sa podpiete',
  `przycisk=${!!przycisk} panel=${!!panel}`);

// --- 4. klikniecia ----------------------------------------------------
const tik = () => new Promise((r) => setTimeout(r, 0));

function ustalDisplay() {
  if (!panel) return 'brak panelu';
  return panel.style.display;
}

if (przycisk && panel) {
  const przed = ustalDisplay();
  przycisk.click();
  const po1 = ustalDisplay();
  przycisk.click();
  const po2 = ustalDisplay();
  sprawdz(przed === 'none' && po1 === 'block' && po2 === 'none',
    'J06 przycisk rozwija i zwija panel', `${przed} -> ${po1} -> ${po2}`);
} else {
  sprawdz(false, 'J06 przycisk rozwija i zwija panel', 'brak elementow');
}

const BILET = { short: '0123456789abcdef0123456789abcdef', long: 'fedcba9876543210fedcba9876543210' };

async function klikPobierz(ktory, id, strumien, odpowiedz) {
  const el = elementy.get(id);
  sprawdz(!!el, `J07-${ktory} przycisk ${id} istnieje`, id);
  if (!el) return null;
  const przed = odwolaniaFetch.length;
  kolejkaOdpowiedzi = [odpowiedz];
  el.click();
  await tik();
  await tik();
  await tik();
  const nowe = odwolaniaFetch.slice(przed);
  sprawdz(nowe.length === 1, `J08-${ktory} jeden request o bilet`,
    `bylo ${nowe.length}`);
  if (nowe.length !== 1) return null;
  const url = nowe[0].url;
  sprawdz(url.startsWith('/history/link?'), `J09-${ktory} panel wola /history/link`, url);
  sprawdz(new RegExp(`(^|[?&])stream=${strumien}(&|$)`).test(url),
    `J10-${ktory} panel podaje wlasciwy strumien w linku`, url);
  sprawdz(nowe[0].opcje && nowe[0].opcje.cache === 'no-store',
    `J11-${ktory} bilet nie jest cache'owany`, JSON.stringify(nowe[0].opcje));
  return url;
}

let linkShort = null;
let linkLong = null;
let celShort = null;
let celLong = null;

linkShort = await klikPobierz(1, 'histShort', 'short', { ok: true, status: 200, body: BILET.short + '\n' });
sprawdz(window.location.href.includes('/history?k=' + BILET.short),
  'J12-1 krotka konczy sie adresem /history?k=<bilet>', window.location.href);
celShort = window.location.href;

window.location.href = 'https://centrala.local/';
linkLong = await klikPobierz(2, 'histLong', 'long', { ok: true, status: 200, body: BILET.long });
sprawdz(window.location.href.includes('/history/long?k=' + BILET.long),
  'J12-2 dluga konczy sie adresem /history/long?k=<bilet>', window.location.href);
celLong = window.location.href;

// --- 5. adres pobrania musi pasowac do strumienia biletu --------------
for (const [nazwa, link, cel, strumien] of [
  ['krotka', linkShort, celShort, 'short'],
  ['dluga', linkLong, celLong, 'long'],
]) {
  if (!link || !cel) {
    sprawdz(false, `J13-${nazwa} adres pobrania zgadza sie ze strumieniem biletu`, 'brak adresu');
    continue;
  }
  const sciezka = cel.replace(/^https?:\/\/[^/]+/, '').split('?')[0];
  const oczekiwana = strumien === 'long' ? '/history/long' : '/history';
  sprawdz(sciezka === oczekiwana, `J13-${nazwa} adres pobrania zgadza sie ze strumieniem biletu`,
    `${sciezka} != ${oczekiwana}`);
}

// --- 6. sciezki bledow ------------------------------------------------
{
  const przed = window.location.href;
  const el = elementy.get('histShort');
  if (el) {
    kolejkaOdpowiedzi = [{ ok: false, status: 503, body: '' }];
    el.click();
    await tik();
    await tik();
    await tik();
    sprawdz(window.location.href === przed, 'J14 przy 503 nie ma nawigacji', window.location.href);
    sprawdz(info && /503/.test(info.textContent), 'J15 przy 503 komunikat mowi o HTTP 503',
      info ? info.textContent : 'brak elementu');
  } else {
    sprawdz(false, 'J14 przy 503 nie ma nawigacji', 'brak przycisku');
  }
}
{
  const przed = window.location.href;
  const el = elementy.get('histShort');
  if (el) {
    kolejkaOdpowiedzi = [{ ok: true, status: 200, body: 'za-krotki' }];
    el.click();
    await tik();
    await tik();
    await tik();
    sprawdz(window.location.href === przed,
      'J16 zla dlugosc biletu nie powoduje nawigacji', window.location.href);
    sprawdz(info && info.textContent.includes('biletu'),
      'J17 zla dlugosc biletu ma komunikat', info ? info.textContent : 'brak elementu');
  } else {
    sprawdz(false, 'J16 zla dlugosc biletu nie powoduje nawigacji', 'brak przycisku');
  }
}

// --- 7. wynik dla Test.cpp -------------------------------------------
function szablon(cel, bilet) {
  const bezHostu = cel.replace(/^https?:\/\/[^/]+/, '');
  const i = bezHostu.indexOf(bilet);
  if (i < 0) return null;
  return bezHostu.slice(0, i) + '{BILET}' + bezHostu.slice(i + bilet.length);
}

const pary = [
  { strumien: 'short', link: linkShort, pobranie: celShort ? szablon(celShort, BILET.short) : null },
  { strumien: 'long', link: linkLong, pobranie: celLong ? szablon(celLong, BILET.long) : null },
];
for (const p of pary) {
  sprawdz(!!p.pobranie && p.pobranie.includes('{BILET}'),
    `J18-${p.strumien} adres pobrania ma miejsce na bilet`, String(p.pobranie));
}

writeFileSync(join(katalogWy, 'urls.txt'),
  pary.map((p) => `${p.strumien}\t${p.link}\t${p.pobranie}`).join('\n') + '\n');
writeFileSync(join(katalogWy, 'urls.json'), JSON.stringify({
  panel: basename(plikHtml),
  idy,
  pary,
  zadaneIdy: [...new Set(zadaneIdy)],
  sprawdzenia,
}, null, 2) + '\n');

const nieudane = sprawdzenia.filter((s) => !s.ok);
for (const s of sprawdzenia) {
  if (!s.ok) console.log(`FAIL panel216 [${s.nazwa}] ${s.szczegol}`);
}
console.log(`${nieudane.length === 0 ? 'PASS' : 'FAIL'} panel216 sprawdzen=${sprawdzenia.length} bledow=${nieudane.length}`);
process.exit(nieudane.length === 0 ? 0 : 1);
