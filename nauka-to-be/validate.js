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

/* ---------- mega-baza: rozmiar, stałe id, brak powtórzeń ---------- */
if (!D.meta || !D.meta.bankTotal || D.meta.bankTotal < 4000)
  bad(`Mega-baza za mała: ${D.meta && D.meta.bankTotal} (oczekujemy ≥ 4000 zadań)`);
const BANKS = ["choice", "gap", "order", "transform", "shorts", "detective", "sort",
               "listening", "dictation", "translate", "spelling", "flashcards", "examPool"];
const allIds = new Set();
BANKS.forEach((k) => D[k].forEach((x, i) => {
  if (!x.id) bad(`${k}[${i}]: brak stałego identyfikatora (system bez powtórek)`);
  if (x.id && allIds.has(x.id)) bad(`${k}[${i}]: identyfikator ${x.id} się powtarza`);
  if (x.id) allIds.add(x.id);
  if (!x.area) bad(`${k}[${i}]: brak obszaru umiejętności (oceny cząstkowe)`);
}));
const DUPKEYS = {
  choice: (x) => x.s + "|" + x.a, gap: (x) => x.s + "|" + x.a[0], order: (x) => x.answer,
  translate: (x) => x.pl + "|" + x.answer, transform: (x) => x.from + "|" + x.task,
  shorts: (x) => x.q + "|" + x.cue, detective: (x) => x.words.join(" ") + "|" + x.a,
  sort: (x) => x.s, listening: (x) => x.spoken, dictation: (x) => x.spoken,
  spelling: (x) => x.word, flashcards: (x) => x.front,
  examPool: (x) => x.p + "|" + JSON.stringify(x.o || x.a), reading: (x) => x.text
};
Object.keys(DUPKEYS).forEach((k) => {
  const seen = new Set();
  D[k].forEach((x, i) => {
    const key = DUPKEYS[k](x);
    if (seen.has(key)) bad(`${k}[${i}]: powtórzone zadanie „${String(key).slice(0, 60)}”`);
    seen.add(key);
  });
});
["pairSets", "matchSets"].forEach((k) => D[k].forEach((s) => {
  if (!s.id || !s.pairs.length) bad(`${k}: zestaw bez id lub par`);
}));

/* każde ćwiczenie z listy musi mieć implementację i odwrotnie */
const app = fs.readFileSync(path.join(here, "app.js"), "utf8");
["function deal(", "function recordGrade(", "function renderReview(", "function logAnswer(",
 "function gradeOf(", "exam-count", "print-exam", "btn-dark", "btn-mute",
 "function actMix(", "function renderLab(", "function renderLights(", "function machineBuild(",
 "function renderFind(", "exitTicketHtml", "chip-level"].forEach((needle) => {
  if (app.indexOf(needle) === -1) bad(`W app.js brakuje nowej funkcji/elementu: ${needle}`);
});
const html = fs.readFileSync(path.join(here, "index.html"), "utf8");
["review-stats", "review-host", "review-list", "gradebook", "skill-grid", "hub-bank", "run-set",
 "machine-host", "machine-sentences", "find-host", "lights-grid", "lights-summary", "chip-level",
 "print-mine"].forEach((id) => {
  if (html.indexOf('id="' + id + '"') === -1) bad(`W index.html brakuje sekcji: ${id}`);
});
if (html.indexOf('id="lab"') === -1) bad("W index.html brakuje zakładki Laboratorium");
if (D.activities.length !== 16) bad(`Oczekiwano 16 ćwiczeń, jest ${D.activities.length}`);
if (!D.machine || !D.machine.subjects || !D.machine.endings || !D.machine.people || !D.machine.questions)
  bad("Brak danych Laboratorium (D.machine)");
else {
  if (D.machine.subjects.length < 8) bad("Za mało podmiotów w maszynie zdań");
  if (D.machine.endings.length < 10) bad("Za mało końcówek zdań w maszynie");
  D.machine.subjects.forEach((s, i) => {
    if (!D.be[s.bi]) bad(`machine.subjects[${i}]: zły indeks formy be`);
  });
  D.machine.people.forEach((p, i) => {
    if (!p.name || !p.age || !p.from || !p.job) bad(`machine.people[${i}]: niepełny profil`);
  });
  D.machine.questions.forEach((q, i) => {
    if (!q.q || !q.test) bad(`machine.questions[${i}]: brak treści/testu`);
  });
}
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

console.log(`Baza: ${D.activities.length} ćwiczeń · ${D.meta.bankTotal} unikalnych zadań · ${D.choice.length} pytań wyboru · ` +
            `${D.gap.length} luk · ${D.order.length + D.translate.length} rozsypanek · ${D.examPool.length} pytań kartkówki · ${D.flashcards.length} fiszek`);
console.log("Nowe funkcje: brak powtórek, oceny 1–6, powtórki, dzienniczek, ciemny tryb, wydruk kartkówki ✓");
console.log("Z researchu: Miks/interleaving, maszyna zdań, własne zdania, Find Someone Who, światła samooceny, bilety wyjścia, poziomy ✓");
if (problems.length) { console.error("BŁĘDY:\n - " + problems.join("\n - ")); process.exit(1); }
console.log("✅ Walidacja danych i implementacji: OK");
