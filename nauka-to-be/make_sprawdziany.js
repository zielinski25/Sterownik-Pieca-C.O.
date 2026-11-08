#!/usr/bin/env node
/* Generuje 6 gotowych, rozłącznych sprawdzianów z `to be` jako samodzielne
   pliki HTML do druku A4 (każdy z kluczem odpowiedzi na osobnej stronie).

   Użycie:  node make_sprawdziany.js
   Efekt:   sprawdzian-1.html … sprawdzian-6.html, sprawdziany-wszystkie.html
            oraz kopie w ../docs/
*/
const fs = require("fs");
const path = require("path");

const HERE = __dirname;
require(path.join(HERE, "data.js"));
const D = (typeof window !== "undefined" ? window : globalThis).TOBE_DATA;

function esc(s) {
  return String(s == null ? "" : s).replace(/[&<>"']/g, function (c) {
    return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
  });
}

const CSS = `
@page { size: A4; margin: 15mm 16mm; }
* { box-sizing: border-box; }
body { font-family: "DejaVu Sans", "Segoe UI", Arial, sans-serif; color: #123049; margin: 0; font-size: 12.5pt; line-height: 1.45; }
.page { page-break-after: always; break-after: page; }
.page:last-child { page-break-after: auto; break-after: auto; }
h1 { font-size: 19pt; margin: 0 0 6px; letter-spacing: .3px; }
h1.key { color: #0b8f86; }
.meta { border-top: 3px solid #123049; border-bottom: 1px solid #c9dbe4; padding: 9px 0; margin: 10px 0 16px; font-size: 11.5pt; }
.scope { color: #5c7390; font-size: 10pt; margin: 0 0 14px; }
ol.q { padding-left: 22px; margin: 0; }
ol.q > li { margin-bottom: 11px; page-break-inside: avoid; break-inside: avoid; }
.opts { color: #5c7390; font-size: 10.5pt; margin: 2px 0 1px; padding-left: 4px; }
.line { color: #b9c9d4; letter-spacing: 1.5px; overflow: hidden; white-space: nowrap; font-size: 9pt; }
.foot { margin-top: 18px; padding-top: 8px; border-top: 1px dashed #b9c9d4; color: #5c7390; font-size: 9.5pt; }
ol.key { padding-left: 22px; }
ol.key > li { margin-bottom: 7px; }
.why { color: #5c7390; font-size: 10.5pt; }
.badge { display: inline-block; background: #e2f7f4; color: #0b8f86; border-radius: 6px; padding: 2px 9px; font-size: 10pt; font-weight: 700; }
hr.sep { border: 0; border-top: 2px solid #123049; margin: 0 0 14px; }
`;

function questionsHtml(qs) {
  return '<ol class="q">' + qs.map(function (q) {
    return '<li><div>' + esc(q.p) + '</div>' +
      (q.o ? '<div class="opts">A) ' + esc(q.o[0]) + ' &nbsp;&nbsp; B) ' + esc(q.o[1]) +
             ' &nbsp;&nbsp; C) ' + esc(q.o[2]) + '</div>' : "") +
      '<div class="line">......................................................................................................................................................</div></li>';
  }).join("") + '</ol>';
}

function keyHtml(qs) {
  return '<ol class="key">' + qs.map(function (q) {
    return '<li><b>' + esc(Array.isArray(q.a) ? q.a[0] : q.a) + '</b>' +
      (q.w ? ' <span class="why">— ' + esc(q.w) + '</span>' : "") + '</li>';
  }).join("") + '</ol>';
}

function sheetPage(sh, i) {
  return '<div class="page"><h1>SPRAWDZIAN ' + (i + 1) + ' &nbsp;·&nbsp; czasownik „to be"</h1>' +
    '<div class="meta">Imię i nazwisko: ____________________________________ &nbsp; Data: ____________ &nbsp; Wynik: ______ / 15 &nbsp; Ocena: ______</div>' +
    '<p class="scope">Zakres: odmiana przez osoby, twierdzenia, pytania, przeczenia i krótkie odpowiedzi · Together 2026, klasa 4 · termin kartkówki 10.11.2026</p>' +
    questionsHtml(sh.questions) +
    '<div class="foot">Wypełnij luki lub zaznacz A / B / C. Za każde zadanie 1 punkt. Powodzenia! 💪</div></div>';
}

function keyPage(sh, i) {
  return '<div class="page"><h1 class="key">🔑 Klucz odpowiedzi &nbsp;·&nbsp; sprawdzian ' + (i + 1) + '</h1>' +
    '<p class="scope">Dla rodzica / nauczyciela — nie drukuj razem z arkuszem dla dziecka.</p>' +
    '<hr class="sep">' + keyHtml(sh.questions) +
    '<div class="foot">Skala (zgodna z aplikacją): 15 → 6 · 14 → 5 · 11–13 → 4 · 9–10 → 3 · 6–8 → 2 · 0–5 → 1.</div></div>';
}

function page(title, body) {
  return '<!DOCTYPE html>\n<html lang="pl">\n<head>\n<meta charset="utf-8">\n' +
    '<meta name="viewport" content="width=device-width, initial-scale=1">\n' +
    '<title>' + esc(title) + '</title>\n<style>' + CSS + '</style>\n</head>\n<body>\n' +
    body + '\n</body>\n</html>\n';
}

if (!D.printTests || D.printTests.length !== 6) {
  console.error("Brak 6 sprawdzianów w data.js — uruchom generator danych.");
  process.exit(1);
}
/* Sprawdzenie rozłączności zestawów */
const all = [];
D.printTests.forEach(function (sh) { sh.questions.forEach(function (q) { all.push(q.id || q.p); }); });
if (new Set(all).size !== all.length) {
  console.error("BŁĄD: sprawdziany zawierają powtórzone pytania!");
  process.exit(1);
}

const dests = [HERE, path.join(HERE, "..", "docs")];
dests.forEach(function (dir) { fs.mkdirSync(dir, { recursive: true }); });

D.printTests.forEach(function (sh, i) {
  const html = page("Sprawdzian " + (i + 1) + " · TO BE", sheetPage(sh, i) + keyPage(sh, i));
  dests.forEach(function (dir) {
    const p = path.join(dir, "sprawdzian-" + (i + 1) + ".html");
    fs.writeFileSync(p, html, "utf8");
    console.log("OK →", p);
  });
});

const bundle = page("Sprawdziany 1–6 · TO BE",
  '<div class="page"><h1>📄 Komplet 6 sprawdzianów z „to be"</h1>' +
  '<p class="scope">6 rozłącznych arkuszy po 15 zadań, potem 6 kluczy odpowiedzi. Wydrukuj całość lub wybrane strony.</p>' +
  '<p><span class="badge">A4</span> &nbsp; <span class="badge">15 zadań / arkusz</span> &nbsp; <span class="badge">klucz na osobnej stronie</span></p>' +
  '<p>1–6: arkusze dla dziecka · 7–12: klucze dla rodzica.</p></div>' +
  D.printTests.map(function (sh, i) { return sheetPage(sh, i); }).join("") +
  D.printTests.map(function (sh, i) { return keyPage(sh, i); }).join(""));
dests.forEach(function (dir) {
  const p = path.join(dir, "sprawdziany-wszystkie.html");
  fs.writeFileSync(p, bundle, "utf8");
  console.log("OK →", p);
});
console.log("Sprawdziany: 6 × 15 =", all.length, "różnych pytań ✓");
