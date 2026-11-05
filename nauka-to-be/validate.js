#!/usr/bin/env node
/* Sprawdza spójność bazy zadań i kompletność implementacji.
   Uruchom: node validate.js   (nie wymaga żadnych zależności) */
"use strict";
const fs = require("fs");
const path = require("path");
const here = __dirname;
require(path.join(here, "data.js"));
const D = global.TOBE_DATA;
const problems = [];
const bad = (m) => problems.push(m);

/* Czy odpowiedź da się złożyć z dostępnych klocków?
   Porównujemy „grupy liter" — ignorujemy interpunkcję, ale pilnujemy liczności. */
function reconstructible(answer, words) {
  const groups = (s) => s.toLowerCase().replace(/[^a-z0-9' ]/g, " ").replace(/\s+/g, " ").trim().split(" ").filter(Boolean);
  const need = groups(answer), pool = groups(words.join(" "));
  return need.every((g) => { const i = pool.indexOf(g); if (i === -1) return false; pool.splice(i, 1); return true; });
}

if (!D || !D.be || D.be.length !== 8) bad("Tabela be powinna mieć 8 osób");
D.choice.forEach((x, i) => {
  if (x.o.indexOf(x.a) === -1) bad(`choice[${i}]: poprawnej odpowiedzi nie ma w opcjach`);
  if (x.s.indexOf("___") === -1) bad(`choice[${i}]: brak luki ___`);
});
D.gap.forEach((x, i) => {
  if (!Array.isArray(x.a) || !x.a.length) bad(`gap[${i}]: brak akceptowanych odpowiedzi`);
  if (x.s.indexOf("___") === -1) bad(`gap[${i}]: brak luki ___`);
});
D.pairSets.concat(D.matchSets).forEach((set) => set.pairs.forEach((p, i) => {
  if (p.length !== 2 || !p[0] || !p[1]) bad(`${set.name}[${i}]: niekompletna para`);
}));
["order", "translate"].forEach((bank) => D[bank].forEach((x, i) => {
  if (!reconstructible(x.answer, x.words)) bad(`${bank}[${i}]: nie da się złożyć „${x.answer}" z klocków`);
  if (new Set(x.words).size !== x.words.length) bad(`${bank}[${i}]: powtórzony klocek (indeksy nie będą unikalne)`);
  if (x.extra && x.words.indexOf(x.extra) === -1) bad(`${bank}[${i}]: brak zadeklarowanego distractora`);
}));
D.transform.forEach((x, i) => { if (!x.a.length || !x.from || !x.show || !x.task) bad(`transform[${i}]: niekompletne dane`); });
D.shorts.forEach((x, i) => { if (x.o.indexOf(x.a) === -1) bad(`shorts[${i}]: brak poprawnej odpowiedzi`); if (!x.cue) bad(`shorts[${i}]: brak wskazówki Tak/Nie`); });
D.detective.forEach((x, i) => {
  if (x.o.indexOf(x.a) === -1) bad(`detective[${i}]: brak poprawnego zdania w opcjach`);
  if (x.bad < 0 || x.bad >= x.words.length) bad(`detective[${i}]: indeks błędu poza zakresem`);
  if (x.o.length !== 3) bad(`detective[${i}]: powinny być 3 opcje`);
});
D.sort.forEach((x, i) => { if (["affirmative", "question", "negative"].indexOf(x.t) === -1) bad(`sort[${i}]: nieznany typ „${x.t}"`); });
D.reading.forEach((r) => r.items.forEach((x, i) => { if (typeof x.t !== "boolean") bad(`reading „${r.title}"[${i}]: brak T/F`); }));
D.listening.forEach((x, i) => { if (x.o.indexOf(x.a) === -1) bad(`listening[${i}]: brak poprawnej odpowiedzi`); });
D.dictation.forEach((x, i) => { if (!x.a.length || !x.spoken) bad(`dictation[${i}]: brak danych`); });
D.spelling.forEach((x, i) => { if (!x.word || !x.pl) bad(`spelling[${i}]: brak danych`); });
D.flashcards.forEach((x, i) => { if (!x.front || !x.back) bad(`flashcards[${i}]: pusta karta`); });
D.examPool.forEach((q, i) => {
  if (q.type === "choice" && q.o.indexOf(q.a) === -1) bad(`examPool[${i}]: brak poprawnej opcji`);
  if (q.type === "input" && (!Array.isArray(q.a) || !q.a.length)) bad(`examPool[${i}]: wpisywane pytanie musi mieć listę odpowiedzi`);
  if (!q.area || !q.w) bad(`examPool[${i}]: brak obszaru lub wyjaśnienia`);
});
if (D.plan.length !== 5) bad("Plan nauki powinien mieć 5 dni");

/* każde ćwiczenie z listy musi mieć implementację i odwrotnie */
const app = fs.readFileSync(path.join(here, "app.js"), "utf8");
const block = app.match(/const IMPL = \{([\s\S]*?)\n  \};/);
if (!block) bad("Nie znaleziono mapy IMPL w app.js");
else {
  const keys = Array.from(block[1].matchAll(/\b(\w+):\s*act\w+/g)).map((m) => m[1]);
  D.activities.forEach((a) => { if (keys.indexOf(a.id) === -1) bad(`Brak implementacji ćwiczenia: ${a.id}`); });
  keys.forEach((k) => { if (!D.activities.some((a) => a.id === k)) bad(`Implementacja bez opisu w danych: ${k}`); });
}

/* wersja jednoplikowa powinna być aktualna */
const out = path.join(here, "to-be-kartkowka.html");
if (!fs.existsSync(out)) bad("Brak to-be-kartkowka.html — uruchom python3 build.py");
else {
  const h = fs.readFileSync(out, "utf8");
  const newest = Math.max(...["index.html", "styles.css", "data.js", "app.js"].map((f) => fs.statSync(path.join(here, f)).mtimeMs));
  if (fs.statSync(out).mtimeMs < newest) bad("to-be-kartkowka.html jest starszy niż źródła — uruchom python3 build.py");
  if (h.indexOf("<style>") === -1 || h.indexOf("TOBE_DATA") === -1) bad("Wersja jednoplikowa nie zawiera wbudowanego CSS/JS");
}

console.log(`Baza: ${D.activities.length} ćwiczeń · ${D.choice.length} pytań wyboru · ${D.gap.length} luk · ` +
            `${D.order.length + D.translate.length} rozsypanek · ${D.examPool.length} pytań kartkówki · ${D.flashcards.length} fiszek`);
if (problems.length) { console.error("BŁĘDY:\n - " + problems.join("\n - ")); process.exit(1); }
console.log("✅ Walidacja danych i implementacji: OK");
