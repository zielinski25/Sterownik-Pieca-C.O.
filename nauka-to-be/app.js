/* =========================================================================
   TO BE — logika aplikacji (bez zewnętrznych bibliotek, działa z file://)
   ========================================================================= */
(function () {
  "use strict";
  const D = window.TOBE_DATA;
  const $ = function (s, r) { return (r || document).querySelector(s); };
  const $$ = function (s, r) { return Array.prototype.slice.call((r || document).querySelectorAll(s)); };

  function esc(s) {
    return String(s == null ? "" : s).replace(/[&<>"']/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c];
    });
  }
  function shuffle(a) {
    const x = a.slice();
    for (let i = x.length - 1; i > 0; i--) { const j = Math.floor(Math.random() * (i + 1)); const t = x[i]; x[i] = x[j]; x[j] = t; }
    return x;
  }
  function norm(s) {
    return String(s == null ? "" : s).toLowerCase()
      .replace(/[’`´]/g, "'").replace(/\s+/g, " ").replace(/[.!?]+$/, "").trim();
  }
  function pct(a, b) { return b ? Math.round(100 * a / b) : 0; }
  function today() { return new Date().toISOString().slice(0, 10); }

  /* ---------------- stan (localStorage) ---------------- */
  const KEY = "tobe-kartkowka-v1";
  function fresh() {
    return { xp: 0, streak: 0, bestStreak: 0, correct: 0, wrong: 0, bestExam: 0,
             acts: {}, badges: [], plan: [], cards: {}, weak: {}, days: [], speedBest: 0,
             seen: {}, cycles: {}, grades: [], mistakes: {}, skills: {}, mute: false, dark: false,
             lights: {}, mySentences: [], exitTickets: [],
             /* JEDEN globalny rejestr użytych treści dla całej strony.
                Klucz = kanoniczna sygnatura zadania, wartość = numer użycia. */
             used: {}, useSeq: 0, usedMigrated: false, cardQueue: [], daily: null, sheets: [] };
  }
  let S = (function () {
    try { const raw = localStorage.getItem(KEY); if (raw) return Object.assign(fresh(), JSON.parse(raw)); } catch (e) {}
    return fresh();
  })();
  /* migracja starszych zapisów — nowe pols muszą zawsze istnieć */
  if (!S.seen || typeof S.seen !== "object") S.seen = {};
  if (!S.cycles || typeof S.cycles !== "object") S.cycles = {};
  if (!S.mistakes || typeof S.mistakes !== "object") S.mistakes = {};
  if (!S.skills || typeof S.skills !== "object") S.skills = {};
  if (!Array.isArray(S.grades)) S.grades = [];
  if (!Array.isArray(S.days)) S.days = [];
  if (!Array.isArray(S.badges)) S.badges = [];
  if (!Array.isArray(S.plan)) S.plan = [];
  if (!S.acts || typeof S.acts !== "object") S.acts = {};
  if (!S.cards || typeof S.cards !== "object") S.cards = {};
  if (!S.weak || typeof S.weak !== "object") S.weak = {};
  if (!S.lights || typeof S.lights !== "object") S.lights = {};
  if (!Array.isArray(S.mySentences)) S.mySentences = [];
  if (!Array.isArray(S.exitTickets)) S.exitTickets = [];
  if (!S.used || typeof S.used !== "object") S.used = {};
  if (!Number.isInteger(S.useSeq)) S.useSeq = Object.keys(S.used).length;
  if (!Array.isArray(S.cardQueue)) S.cardQueue = [];
  if (!Array.isArray(S.sheets)) S.sheets = [];
  if (typeof S.usedMigrated !== "boolean") S.usedMigrated = false;
  function save() { try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (e) {} }
  if (S.days.indexOf(today()) === -1) { S.days.push(today()); save(); }

  /* ---------------- oceny ---------------- */
  const GRADE = [
    { min: 95, g: 6, name: "celujący", ico: "🏆" },
    { min: 88, g: 5, name: "bardzo dobry", ico: "🌟" },
    { min: 72, g: 4, name: "dobry", ico: "👍" },
    { min: 56, g: 3, name: "dostateczny", ico: "🙂" },
    { min: 36, g: 2, name: "dopuszczający", ico: "🌱" },
    { min: 0,  g: 1, name: "jeszcze ćwiczę", ico: "💪" }
  ];
  function gradeOf(p) { for (let i = 0; i < GRADE.length; i++) if (p >= GRADE[i].min) return GRADE[i]; return GRADE[GRADE.length - 1]; }
  function gradeBadge(p) { const g = gradeOf(p); return g.ico + " " + g.g + " · " + g.name; }
  function recordGrade(kind, act, ok, total, seconds) {
    if (!total) return;
    const p = pct(ok, total);
    S.grades.unshift({ d: today(), t: Date.now(), kind: kind, act: act, ok: ok, total: total, p: p, g: gradeOf(p).g, sec: seconds || 0 });
    if (S.grades.length > 80) S.grades.length = 80;
    save();
  }

  /* ---------------- JEDEN globalny rejestr — zero powtórek ----------------
     wcześniej: każde ćwiczenie miało WŁASNY licznik, a kartkówka, miks,
     szybka runda i wydruk kopiowały te same zdania z `choice`/`gap`.
     Efekt: dziecko robiło „Wybierz formę", potem dostawało to samo zdanie
     na próbnej kartkówce. Teraz rejestr jest współdzielony przez całą stronę. */
  function contentKey(it) {
    if (!it) return "";
    if (it.pairs) return "set|" + norm(it.name || it.id || "");
    if (it.text) return "text|" + norm(String(it.text).slice(0, 200));
    const p = it.s || it.p || it.q || it.from || it.spoken || it.front || it.word || it.pl ||
              (it.words ? it.words.join(" ") : "") || "";
    const a = Array.isArray(it.a) ? it.a[0] : (it.a || it.show || it.answer || it.t || "");
    const opts = Array.isArray(it.o) ? " {" + it.o.slice().sort().map(norm).join(",") + "}" : "";
    const task = it.task ? norm(it.task) + "|" : "";
    let s = norm(String(p) + " | " + String(a)) + opts;
    s = s.replace(/^uzupełnij\s*:\s*/, "").replace(/^uzupełnij\s+/, "").replace(/^po angielsku\s*:\s*/, "");
    /* area = ćwiczona umiejętność (musi być identycznie jak sig() w data.js) */
    return (it.area ? norm(it.area) + "|" : "") + task + s;
  }
  function useContent(it) { const k = contentKey(it); if (k) S.used[k] = ++S.useSeq; }
  function seenCount() { return Object.keys(S.used).length; }
  function globalTotal() { return D.meta.uniqueContent || D.meta.bankTotal; }

  function deal(key, items, count) {
    const n = items.length;
    if (!n) return [];
    const order = shuffle(items.map(function (_, i) { return i; }));
    const out = [];
    /* 1) wyłącznie treści, których dziecko NIGDZIE jeszcze nie widziało */
    order.forEach(function (i) {
      if (out.length >= count) return;
      const ck = contentKey(items[i]);
      if (ck && S.used[ck] === undefined) { out.push(i); S.used[ck] = ++S.useSeq; }
    });
    /* 2) gdy pula się wyczerpie: nowy cykl od NAJDAWNIEJ widzianych,
       nigdy losowo i nigdy z powrotem z tego samego zestawu */
    if (out.length < count) {
      const inSet = {}; out.forEach(function (i) { inSet[i] = true; });
      const rest = items.map(function (_, i) { return i; })
        .filter(function (i) { return !inSet[i]; })
        .sort(function (a, b) {
          return (S.used[contentKey(items[a])] || 0) - (S.used[contentKey(items[b])] || 0);
        });
      rest.forEach(function (i) {
        if (out.length >= count) return;
        out.push(i); S.used[contentKey(items[i])] = ++S.useSeq;
      });
      S.cycles[key] = (S.cycles[key] || 0) + 1;
    } else if (!S.cycles[key]) S.cycles[key] = 1;
    /* S.seen zostaje dla etykiet „Zestaw N" i wstecznej zgodności */
    const old = Array.isArray(S.seen[key]) ? S.seen[key] : [];
    S.seen[key] = old.concat(out);
    save();
    return shuffle(out).map(function (i) { return items[i]; });
  }
  /* Migracja starszych zapisów: indeksy z S.seen → sygnatury w S.used,
     żeby po aktualizacji strony dziecko nie dostało starych zadań od nowa. */
  function migrateSeenToUsed() {
    if (S.usedMigrated) return;
    Object.keys(S.seen || {}).forEach(function (k) {
      const arr = BANK_OF[k];
      if (!arr) return;
      (S.seen[k] || []).forEach(function (i) {
        if (!arr[i]) return;
        const ck = contentKey(arr[i]);
        if (ck && S.used[ck] === undefined) S.used[ck] = ++S.useSeq;
      });
    });
    S.usedMigrated = true;
    save();
  }
  /* Miks = trening przeplatany (interleaving): jeden zestaw z 5 różnych typów zadań.
     Kolejność jest stała, więc indeksy w S.seen.mix też są stałe → zero powtórek. */
  const MIX_SOURCES = [["choice", D.choice], ["gap", D.gap], ["transform", D.transform],
                       ["shorts", D.shorts], ["detective", D.detective]];
  const MIX_POOL = [];
  MIX_SOURCES.forEach(function (p) {
    p[1].forEach(function (it) { const c = Object.assign({}, it); c._act = p[0]; MIX_POOL.push(c); });
  });
  const BANK_OF = {
    choice: D.choice, gap: D.gap, order: D.order, transform: D.transform, shorts: D.shorts,
    detective: D.detective, sort: D.sort, listen: D.listening, listening: D.listening, dictation: D.dictation,
    translate: D.translate, spelling: D.spelling, memory: D.pairSets, match: D.matchSets,
    reading: D.reading, speed: D.choice, mix: MIX_POOL, exam: D.examPool, printExam: D.examPool,
    sheets: D.sheetBank
  };
  function bankSize(act) { const b = BANK_OF[act]; return b ? b.length : 0; }
  function seenTotal() { return seenCount(); }

  /* ---------------- powtórka rozłożona w czasie (to, czego dziecko nie umie) ---------------- */
  const BOX_DAYS = [0, 1, 2, 4, 7, 14, 30];
  function addDays(n) { const d = new Date(); d.setDate(d.getDate() + n); return d.toISOString().slice(0, 10); }
  function skillOf(act) { const a = D.activities.find(function (x) { return x.id === act; }); return a ? a.skill : "powtórka"; }
  function reviewData(it, act, correctText) {
    if (!it) return { prompt: "", correct: String(correctText || ""), options: null };
    if (Array.isArray(it.o)) return { prompt: it.s || it.p || it.q || (it.words ? it.words.join(" ") : ""), correct: Array.isArray(it.a) ? it.a[0] : it.a, options: it.o.slice() };
    if (act === "reading") return { prompt: it.s, correct: it.t ? "Prawda" : "Fałsz", options: ["Prawda", "Fałsz"] };
    if (it.from && it.show) return { prompt: "Zrób " + it.task + ": " + it.from, correct: it.show, options: null };
    if (it.pl && it.answer) return { prompt: "Po angielsku: " + it.pl, correct: it.answer, options: null };
    if (it.words && it.answer) return { prompt: "Ułóż zdanie z wyrazów: " + shuffle(it.words).join(" "), correct: it.answer, options: null };
    if (Array.isArray(it.a)) return { prompt: it.s || it.p || it.spoken || it.spoken || "", correct: it.a[0], options: null };
    return { prompt: it.s || it.p || it.q || it.front || "", correct: String(correctText || it.a || ""), options: null };
  }
  function logAnswer(it, act, ok, correctText, why) {
    const area = (it && it.area) || skillOf(act);
    const st = S.skills[area] || (S.skills[area] = { tried: 0, ok: 0 });
    st.tried++; if (ok) st.ok++;
    if (!ok) S.weak[area] = (S.weak[area] || 0) + 1;
    const id = it && it.id ? it.id : null;
    if (id) {
      if (!ok) {
        const rd = reviewData(it, act, correctText);
        const rec = S.mistakes[id] || (S.mistakes[id] = {
          id: id, act: act, area: area, prompt: rd.prompt, correct: rd.correct,
          options: rd.options, why: why || (it && it.w) || "",
          box: 0, wrong: 0, right: 0, due: today(), mastered: false
        });
        rec.wrong++; rec.box = 0; rec.due = today(); rec.mastered = false;
      } else if (S.mistakes[id] && !S.mistakes[id].mastered) {
        const rec = S.mistakes[id];
        rec.right++; rec.box = Math.min(6, rec.box + 1);
        rec.due = addDays(BOX_DAYS[rec.box] || 30);
        if (rec.box >= 6) rec.mastered = true;
      }
    }
    save();
  }
  function dueReviews() {
    const t = today();
    return Object.keys(S.mistakes).map(function (k) { return S.mistakes[k]; })
      .filter(function (r) { return !r.mastered && r.due <= t; })
      .sort(function (a, b) { return (b.wrong - b.right) - (a.wrong - a.right); });
  }
  function allMistakes() { return Object.keys(S.mistakes).map(function (k) { return S.mistakes[k]; }); }
  function weakestSkill() {
    /* Samoocena „🔴" ma pierwszeństwo: dziecko samo wie, czego nie umie. */
    const red = Object.keys(S.lights).filter(function (k) { return S.lights[k] === "red"; });
    if (red.length) {
      const k = red[0], st = S.skills[k] || { ok: 0, tried: 0 };
      return { k: k, p: pct(st.ok, st.tried), self: true };
    }
    const arr = Object.keys(S.skills).filter(function (k) { return S.skills[k].tried >= 2; })
      .map(function (k) { return { k: k, p: pct(S.skills[k].ok, S.skills[k].tried) }; })
      .sort(function (a, b) { return a.p - b.p; });
    return arr.length ? arr[0] : null;
  }
  const LIGHT_SKILLS = [
    { k: "odmiana", ican: "Umiem odmienić am / is / are" },
    { k: "twierdzenia", ican: "Umiem zbudować twierdzenie" },
    { k: "przeczenia", ican: "Umiem zrobić przeczenie (isn't, aren't, I'm not)" },
    { k: "pytania", ican: "Umiem zadać pytanie z be na początku" },
    { k: "odpowiedzi", ican: "Umiem odpowiedzieć Yes, I am. / No, I'm not." },
    { k: "pułapki", ican: "Umiem znaleźć i poprawić błąd" },
    { k: "czytanie", ican: "Umiem przeczytać krótki tekst o postaci" },
    { k: "słuchanie", ican: "Umiem rozpoznać formę ze słuchu" },
    { k: "pisownia", ican: "Umiem napisać I'm / isn't / aren't" },
    { k: "tłumaczenie", ican: "Umiem przetłumaczyć zdanie z be" }
  ];
  const LIGHTS = [
    { id: "green", ico: "🟢", label: "Umiem", color: "#2f9e6f" },
    { id: "amber", ico: "🟡", label: "Prawie", color: "#d99b1c" },
    { id: "red", ico: "🔴", label: "Jeszcze ćwiczę", color: "#d1564b" }
  ];
  const SKILL_ACT = { odmiana: "choice", twierdzenia: "gap", pytania: "transform", przeczenia: "detective",
    odpowiedzi: "shorts", pułapki: "detective", rozpoznawanie: "sort", czytanie: "reading",
    słuchanie: "listen", pisownia: "spelling", tłumaczenie: "translate", powtórka: "mix" };

  const BADGES = [
    { id: "first", ico: "🌱", name: "Pierwszy krok", need: "1 poprawna odpowiedź", t: function (s) { return s.correct >= 1; } },
    { id: "xp100", ico: "⭐", name: "100 punktów", need: "Zdobądź 100 pkt", t: function (s) { return s.xp >= 100; } },
    { id: "xp500", ico: "🌟", name: "Gwiazda grupy", need: "Zdobądź 500 pkt", t: function (s) { return s.xp >= 500; } },
    { id: "streak", ico: "🔥", name: "Seria 10", need: "10 odpowiedzi z rzędu", t: function (s) { return s.bestStreak >= 10; } },
    { id: "explorer", ico: "🧭", name: "Odkrywca", need: "Zacznij 8 różnych ćwiczeń", t: function (s) { return Object.keys(s.acts).length >= 8; } },
    { id: "all", ico: "🏆", name: "Wszystkie gry", need: "Ukończ wszystkie ćwiczenia", t: function (s) { return D.activities.every(function (a) { return s.acts[a.id] && s.acts[a.id].done; }); } },
    { id: "detective", ico: "🕵️", name: "Detektyw", need: "8 poprawionych błędów", t: function (s) { return (s.acts.detective && s.acts.detective.ok >= 8); } },
    { id: "ears", ico: "🎧", name: "Słuchacz", need: "10 odpowiedzi ze słuchu", t: function (s) { return ((s.acts.listen && s.acts.listen.ok) || 0) + ((s.acts.dictation && s.acts.dictation.ok) || 0) >= 10; } },
    { id: "exam80", ico: "🥈", name: "80% na próbnej", need: "Wynik min. 80%", t: function (s) { return s.bestExam >= 80; } },
    { id: "exam100", ico: "🥇", name: "Bez błędu!", need: "100% na próbnej kartkówce", t: function (s) { return s.bestExam >= 100; } },
    { id: "cards", ico: "🗂️", name: "Fiszkowicz", need: "20 kart „Umiem”", t: function (s) { return Object.keys(s.cards).filter(function (k) { return s.cards[k] && s.cards[k].ok > 0; }).length >= 20; } },
    { id: "week", ico: "📅", name: "Systematyczność", need: "Nauka w 3 różne dni", t: function (s) { return s.days.length >= 3; } },
    { id: "grinder", ico: "🧠", name: "100 nowych zadań", need: "Zobacz 100 różnych zadań", t: function () { return seenTotal() >= 100; } },
    { id: "repairer", ico: "🛠️", name: "Naprawiacz", need: "Popraw 8 trudnych zadań", t: function (s) { return Object.keys(s.mistakes).filter(function (k) { return s.mistakes[k] && s.mistakes[k].right >= 2; }).length >= 8; } },
    { id: "six", ico: "🎓", name: "Szóstka!", need: "Dostań ocenę 6", t: function (s) { return s.grades.some(function (g) { return g.g === 6; }); } },
    { id: "mixer", ico: "🧩", name: "Mistrz miksu", need: "Ukończ Miks 5 minut", t: function (s) { return !!(s.acts.mix && s.acts.mix.done); } },
    { id: "writer", ico: "✍️", name: "Autor zdań", need: "Zapisz 5 własnych zdań", t: function (s) { return s.mySentences.length >= 5; } },
    { id: "honest", ico: "🚦", name: "Szczery uczeń", need: "Oceń 5 umiejętności światełkami", t: function (s) { return Object.keys(s.lights).length >= 5; } }
  ];

  function updateChips() {
    $("#chip-xp").textContent = "⭐ " + S.xp + " pkt";
    $("#chip-streak").textContent = "🔥 " + S.streak + " z rzędu";
    const lv = $("#chip-level");
    if (lv) {
      const level = Math.floor(S.xp / 100) + 1;
      const inLevel = S.xp % 100;
      lv.textContent = "🎚️ Poziom " + level + " · " + inLevel + "/100";
      lv.title = "Poziom rośnie co 100 punktów. Następny za " + (100 - inLevel) + " pkt.";
    }
    checkBadges();
  }
  function checkBadges() {
    let added = false;
    BADGES.forEach(function (b) {
      if (S.badges.indexOf(b.id) === -1 && b.t(S)) { S.badges.push(b.id); added = true; toast("🏅 Nowa odznaka: " + b.name + "!"); confetti(16); }
    });
    if (added) { save(); renderBadges(); }
  }
  function mark(act, ok) {
    const a = S.acts[act] || { tried: 0, ok: 0, done: false };
    a.tried++; if (ok) a.ok++;
    S.acts[act] = a;
    if (ok) { S.correct++; S.streak++; S.xp += 10 + (S.streak >= 5 ? 5 : 0); S.bestStreak = Math.max(S.bestStreak, S.streak); }
    else { S.wrong++; S.streak = 0; }
    save(); updateChips();
  }
  function award(act, ok, total) {
    const a = S.acts[act] || { tried: 0, ok: 0, done: false };
    a.tried += total; a.ok += ok; a.done = true; S.acts[act] = a;
    S.correct += ok; S.wrong += (total - ok); S.xp += ok * 10;
    if (ok === total) { S.streak += ok; S.bestStreak = Math.max(S.bestStreak, S.streak); } else S.streak = 0;
    save(); updateChips(); updateHub();
  }
  function noteWeak(area) { if (!area) return; S.weak[area] = (S.weak[area] || 0) + 1; save(); }

  /* ---------------- udostępnianie (Telegram, WhatsApp, e-mail…) ---------------- */
  const SHARE_URL = "https://is.gd/gAbph1";
  const SHARE_TEXT = "TO BE — trening przed kartkówką (angielski, 10.11) 🇬🇧 5 698 zadań bez powtórek, 16 typów ćwiczeń, 6 sprawdzianów do druku z kluczami, oceny 1–6, powtórki z błędów i Laboratorium zdań. Bez logowania.";
  $("#btn-share").onclick = async function () {
    const data = { title: "TO BE · trening przed kartkówką", text: SHARE_TEXT, url: SHARE_URL };
    if (navigator.share) {
      try { await navigator.share(data); toast("Gotowe — wybierz Telegram 💬"); return; }
      catch (e) { /* użytkownik anulował — przechodzimy do kopiowania */ }
    }
    const full = SHARE_TEXT + "\n" + SHARE_URL;
    try {
      await navigator.clipboard.writeText(full);
      toast("📋 Link skopiowany — wklej do Telegrama");
    } catch (e) {
      window.prompt("Skopiuj i wklej do Telegrama:", full);
    }
  };

  /* ---------------- wygląd i dźwięk ---------------- */
  function applyDark() {
    document.body.classList.toggle("dark", !!S.dark);
    const b = $("#btn-dark"); if (b) b.textContent = S.dark ? "☀️ Jasny" : "🌙 Ciemny";
  }
  function applyMute() { const b = $("#btn-mute"); if (b) b.textContent = S.mute ? "🔇 Głos wyłączony" : "🔊 Głos"; }
  $("#btn-dark").onclick = function () { S.dark = !S.dark; save(); applyDark(); toast(S.dark ? "🌙 Tryb ciemny włączony" : "☀️ Tryb jasny"); };
  $("#btn-mute").onclick = function () { S.mute = !S.mute; save(); applyMute(); toast(S.mute ? "🔇 Robbie zamilkł" : "🔊 Robbie znowu mówi"); };

  /* ---------------- efekty ---------------- */
  let toastT = null;
  function toast(msg) {
    const t = $("#toast"); t.textContent = msg; t.classList.add("show");
    clearTimeout(toastT); toastT = setTimeout(function () { t.classList.remove("show"); }, 2600);
  }
  function confetti(n) {
    const colors = ["#12b3a6", "#ff7a59", "#ffc93c", "#2fb673", "#123049", "#ff8fb1"];
    for (let i = 0; i < (n || 20); i++) {
      (function (i) {
        setTimeout(function () {
          const c = document.createElement("i"); c.className = "confetti";
          c.style.left = (5 + Math.random() * 90) + "vw";
          c.style.background = colors[i % colors.length];
          c.style.transform = "rotate(" + Math.random() * 90 + "deg)";
          c.style.animationDelay = (Math.random() * .5) + "s";
          document.body.appendChild(c);
          setTimeout(function () { c.remove(); }, 3200);
        }, i * 22);
      })(i);
    }
  }
  function speak(text, slow) {
    if (!text || S.mute) return;
    if (!("speechSynthesis" in window)) { toast("Ta przeglądarka nie ma czytania głosem — poproś kogoś o przeczytanie 🙂"); return; }
    try {
      window.speechSynthesis.cancel();
      const u = new SpeechSynthesisUtterance(String(text));
      u.lang = "en-GB"; u.rate = slow ? 0.68 : 0.9; u.pitch = 1.05;
      const vs = window.speechSynthesis.getVoices() || [];
      const v = vs.find(function (x) { return /en[-_]GB/i.test(x.lang); }) || vs.find(function (x) { return /^en/i.test(x.lang); });
      if (v) u.voice = v;
      window.speechSynthesis.speak(u);
    } catch (e) { toast("Nie udało się odtworzyć głosu."); }
  }
  if ("speechSynthesis" in window) { try { window.speechSynthesis.getVoices(); } catch (e) {} }

  /* ---------------- nawigacja ---------------- */
  function go(id) {
    if (id !== "practice") { clearSpeed(); }
    if (id !== "exam") { clearExamTimer(); }
    $$(".section").forEach(function (s) { s.classList.toggle("active", s.id === id); });
    $$(".tab").forEach(function (t) { t.classList.toggle("active", t.dataset.go === id); });
    if (id === "progress") renderProgress();
    if (id === "flashcards") renderCard();
    if (id === "review") renderReview();
    if (id === "lab") renderLab();
    if (id === "sheets") renderSheets();
    window.scrollTo({ top: 0, behavior: "smooth" });
  }
  document.addEventListener("click", function (e) {
    const act = e.target.closest("[data-act]");
    if (act) { openActivity(act.dataset.act); return; }
    const g = e.target.closest("[data-go]");
    if (g) { e.preventDefault(); go(g.dataset.go); }
  });

  /* ---------------- START ---------------- */
  function daysLeft() {
    const target = new Date(D.meta.date + "T00:00:00");
    const now = new Date(); const now0 = new Date(now.getFullYear(), now.getMonth(), now.getDate());
    return Math.round((target - now0) / 86400000);
  }
  function renderStart() {
    const d = daysLeft();
    $("#countdown").textContent = d > 0 ? d : (d === 0 ? "DZIŚ!" : "✓");
    $("#countdown-label").textContent = d > 0 ? "dni do 10 listopada 2026" : (d === 0 ? "To dziś! Dasz radę 💪" : "kartkówka już za nami — powtórka zawsze się przyda");
    $("#chip-days").textContent = d > 0 ? "📅 do kartkówki: " + d + " dni" : (d === 0 ? "📅 kartkówka dziś!" : "📅 po kartkówce");
    $("#scope-text").textContent = "Zakres: " + D.meta.scope;
    $("#book-text").textContent = "Materiał: " + D.meta.book;

    $("#skills-grid").innerHTML = [
      ["🧮", "Odmiana przez osoby", "I am · you are · he/she/it is · we/you/they are", "choice"],
      ["➕", "Twierdzenia", "Pełne i krótkie formy: I'm, he's, they're", "gap"],
      ["❓", "Pytania", "be na początku + pytania o imię, wiek, kraj", "transform"],
      ["➖", "Przeczenia", "isn't · aren't · I'm not (nigdy amn't!)", "detective"]
    ].map(function (x) {
      return '<div class="card flat" style="margin:0;text-align:center"><div style="font-size:2rem">' + x[0] + "</div><b>" + x[1] + '</b><div class="muted">' + x[2] + '</div><button class="btn small ghost" style="margin-top:8px" data-act="' + x[3] + '">Ćwicz →</button></div>';
    }).join("");

    // ☀️ Pytanie dnia — jedno na dzień, ale ZAWSZE z niewidzianej jeszcze puli
    // (dawniej był to stały indeks, więc wracało to samo zadanie co w ćwiczeniach)
    const ds = today();
    if (!S.daily || S.daily.d !== ds) {
      const candidates = shuffle(D.choice.map(function (_, i) { return i; }))
        .filter(function (i) { return S.used[contentKey(D.choice[i])] === undefined; });
      const pick = candidates.length ? candidates[0] : Math.floor(Math.random() * D.choice.length);
      S.daily = { d: ds, i: pick };
      if (candidates.length) useContent(D.choice[pick]);
      save();
    }
    const q = D.choice[S.daily.i] || D.choice[0];
    const dc = $("#daily-card");
    if (dc) dc.innerHTML =
      '<div class="between"><h2 style="margin:0">☀️ Pytanie dnia</h2><span class="pill teal">zmienia się codziennie</span></div>' +
      '<p class="muted">Jedno szybkie zadanie na rozgrzewkę. Odpowiedź jest ukryta — najpierw spróbuj sam/a!</p>' +
      '<div class="qbox"><div class="q">' + esc(q.s).replace("___", "<em>___</em>") + '</div></div>' +
      '<details><summary>✅ Sprawdź odpowiedź i wyjaśnienie</summary><div style="padding:10px 4px"><b>' + esc(q.a) +
      '</b> — ' + esc(q.w) + ' <button class="icon-btn" id="daily-say">🔊</button></div></details>';
    const dsay = $("#daily-say"); if (dsay) dsay.onclick = function () { speak(q.a); };

    renderPlan(); renderBadges();
  }
  function renderPlan() {
    $("#plan-list").innerHTML = D.plan.map(function (p, i) {
      const on = S.plan.indexOf(i) !== -1;
      return '<label class="plan-item' + (on ? " done" : "") + '"><input type="checkbox" data-plan="' + i + '"' + (on ? " checked" : "") + '><span><b class="pt">' + p.day + " · " + p.title + '</b><small class="pt">' + p.todo + "</small></span></label>";
    }).join("");
    $$("[data-plan]").forEach(function (c) {
      c.onchange = function () {
        const i = +c.dataset.plan;
        const k = S.plan.indexOf(i);
        if (c.checked && k === -1) S.plan.push(i);
        if (!c.checked && k !== -1) S.plan.splice(k, 1);
        save(); renderPlan();
        if (c.checked) { toast("✅ " + D.plan[i].day + " zaliczone!"); confetti(10); }
      };
    });
  }
  function renderBadges() {
    const html = BADGES.map(function (b) {
      const got = S.badges.indexOf(b.id) !== -1;
      return '<div class="badge' + (got ? " got" : "") + '" title="' + esc(b.need) + '"><div class="bico">' + b.ico + "</div><b>" + b.name + '</b><small class="muted">' + (got ? "zdobyta ✓" : b.need) + "</small></div>";
    }).join("");
    $("#badges-mini").innerHTML = BADGES.slice(0, 6).map(function (b) {
      const got = S.badges.indexOf(b.id) !== -1;
      return '<div class="badge' + (got ? " got" : "") + '"><div class="bico">' + b.ico + "</div><b>" + b.name + "</b></div>";
    }).join("");
    const full = $("#badges-full"); if (full) full.innerHTML = html;
  }

  /* ---------------- NAUKA ---------------- */
  const RULE_SAY = [
    "I am, you are, he is, she is, it is, we are, you are, they are.",
    "I am from Poland. She is eleven. They are cool.",
    "He is not ready. He isn't ready. I am not ready. I'm not ready.",
    "Is she Polish? Are you ten? Where are you from?",
    "Are you ten? Yes, I am. No, I'm not. Remember: not Yes, I'm.",
    "What's your name? Where are you from? How old are you? What's your nationality?",
    "I'm eleven. I'm eleven years old."
  ];
  function renderLearn() {
    $("#rules-list").innerHTML = D.rules.map(function (r, i) {
      return '<div class="rule" data-say="' + i + '" style="cursor:pointer"><div class="ico">' + r.icon + "</div><div><b>" + r.title + "</b><div>" + r.text + '</div><small class="muted">🔊 Kliknij, żeby usłyszeć przykład</small></div></div>';
    }).join("");
    $$("#rules-list .rule").forEach(function (r) { r.onclick = function () { speak(RULE_SAY[+r.dataset.say]); }; });

    const head = "<thead><tr><th>Zaimek</th><th>Po polsku</th><th>Twierdzenie</th><th>Krótko</th><th>Przeczenie (długie)</th><th>Przeczenie (krótko)</th><th>Pytanie</th><th>Krótka odpowiedź</th></tr></thead>";
    const rows = D.be.map(function (b, i) {
      return '<tr data-be="' + i + '" style="cursor:pointer" title="Kliknij, żeby usłyszeć"><td class="en">' + b.en + "</td><td>" + b.pl + "</td><td><b>" + b.en + " " + b.form + "</b></td><td>" + b.short + "</td><td>" + b.negLong + "</td><td><b>" + b.negShort + "</b></td><td>" + b.question + " ...?</td><td>" + b.yes + " / " + b.no + "</td></tr>";
    }).join("");
    $("#be-table").innerHTML = head + "<tbody>" + rows + "</tbody>";
    $$("#be-table tr[data-be]").forEach(function (tr) {
      tr.onclick = function () {
        const b = D.be[+tr.dataset.be];
        speak(b.en + " " + b.form + ". " + b.short + ". " + b.negShort + ". " + b.question + "? " + b.yes);
      };
    });
    $("#btn-sing-table").onclick = function () { speak(RULE_SAY[0] + " " + D.be.map(function (b) { return b.short + " " + b.negShort + "."; }).join(" ")); };

    $("#book-questions").innerHTML = D.bookQuestions.map(function (q) {
      return "<details><summary>" + esc(q.q) + " <span class=\"muted\">— " + esc(q.pl) + '</span></summary><div style="padding:8px 4px"><b>Odpowiedź:</b> ' + esc(q.a) + ' <button class="icon-btn" data-sayq="' + esc(q.q + " " + q.a) + '">🔊</button></div></details>';
    }).join("");
    $$("[data-sayq]").forEach(function (b) { b.onclick = function () { speak(b.dataset.sayq); }; });

    $("#mistakes-list").innerHTML = D.mistakes.map(function (m) {
      return '<div class="mistake"><span class="bad" style="text-decoration:none">' + esc(m.bad) + '</span><span>➜</span><span class="good">' + esc(m.good) + '</span><span class="muted" style="flex-basis:100%">💡 ' + esc(m.why) + '</span></div>';
    }).join("");

    $("#words-list").innerHTML =
      "<details><summary>🌍 Kraje i narodowości (" + D.words.countries.length + ")</summary><div class='grid3' style='padding:10px 0'>" +
      D.words.countries.map(function (w) { return "<div class='muted'><b>" + w[0] + "</b> — " + w[1] + " → <b>" + w[2] + "</b></div>"; }).join("") + "</div></details>" +
      "<details><summary>✨ Przymiotniki (" + D.words.adjectives.length + ")</summary><div class='grid3' style='padding:10px 0'>" +
      D.words.adjectives.map(function (w) { return "<div class='muted'><b>" + w[0] + "</b> — " + w[1] + "</div>"; }).join("") + "</div></details>" +
      "<details><summary>📍 Miejsca (" + D.words.places.length + ")</summary><div class='grid3' style='padding:10px 0'>" +
      D.words.places.map(function (w) { return "<div class='muted'><b>" + w[0] + "</b> — " + w[1] + "</div>"; }).join("") + "</div></details>";

    // ściąga do druku
    $("#print-rules").innerHTML = "<div class='card'><b>3 złote zasady</b><p>1. I → am, he/she/it → is, you/we/they → are.<br>2. Przeczenie: dodaj not po be (isn't, aren't, I'm not; NIGDY amn't).<br>3. Pytanie: be na początek (Is she Polish?), bez do/does.<br>4. Krótka odpowiedź: Yes, I am. / No, I'm not. (nie: Yes, I'm.)<br>5. Wiek: I'm eleven, nie I have 11 years.</p></div>";
    $("#print-table").innerHTML = head + "<tbody>" + rows + "</tbody>";
    $("#print-mistakes").innerHTML = "<div class='card'><b>Najczęstsze błędy → poprawnie</b>" + D.mistakes.map(function (m) { return "<div>" + esc(m.bad) + " → <b>" + esc(m.good) + "</b></div>"; }).join("") + "</div>";
  }
  function buildPrintExam() {
    const qs = deal("printExam", D.examPool, 15);
    return '<div class="card"><h2>Kartkówka · ' + today() + ' · zestaw nr ' + (S.cycles.printExam || 1) + '</h2>' +
      '<p>Imię i nazwisko: __________________________________ &nbsp; Wynik: ________ / 15 &nbsp; Ocena: ________</p></div>' +
      '<div class="card"><ol class="print-questions">' + qs.map(function (q) {
        return '<li><div>' + esc(q.p) + '</div>' +
          (q.o ? '<div class="muted">' + q.o.map(function (o) { return esc(o); }).join(" &nbsp;·&nbsp; ") + '</div>' : "") +
          '<div class="answer-line">............................................................................................</div></li>';
      }).join("") + '</ol></div>' +
      '<div class="answer-key"><div class="card"><h3>🔑 Klucz odpowiedzi (dla rodzica)</h3><ol>' +
      qs.map(function (q) { return '<li>' + esc(Array.isArray(q.a) ? q.a[0] : q.a) + ' <span class="muted">— ' + esc(q.w) + '</span></li>'; }).join("") +
      '</ol></div></div>';
  }
  /* ---------------- SPRAWDZIANY DO DRUKU ----------------
     6 przygotowanych, rozłącznych arkuszy po 15 zadań + klucz dla rodzica.
     „Nowy komplet" losuje 90 świeżych pytań z GLOBALNEGO rejestru, więc
     papierowe sprawdziany też nie powtarzają tego, co było na ekranie. */
  let SH = null;
  function currentSheets() {
    return (Array.isArray(S.sheets) && S.sheets.length === 6 && S.sheets.every(function (s) { return s.questions && s.questions.length === 15; }))
      ? S.sheets : D.printTests;
  }
  function sheetQuestionsHtml(qs) {
    return '<ol class="print-questions">' + qs.map(function (q) {
      return '<li><div>' + esc(q.p) + '</div>' +
        (q.o ? '<div class="muted">A) ' + esc(q.o[0]) + ' &nbsp; B) ' + esc(q.o[1]) + ' &nbsp; C) ' + esc(q.o[2]) + '</div>' : "") +
        '<div class="answer-line">........................................................................................................</div></li>';
    }).join("") + '</ol>';
  }
  function sheetKeyHtml(qs) {
    return '<ol>' + qs.map(function (q) {
      return '<li><b>' + esc(Array.isArray(q.a) ? q.a[0] : q.a) + '</b>' +
        (q.w ? ' <span class="muted">— ' + esc(q.w) + '</span>' : "") + '</li>';
    }).join("") + '</ol>';
  }
  function sheetPage(sh, i, withKey) {
    let h = '<div class="sheet-page"><div class="sheet-head">' +
      '<h2>SPRAWDZIAN ' + (i + 1) + ' · czasownik „to be"</h2>' +
      '<p>Imię i nazwisko: ____________________________________ &nbsp; Data: ____________ &nbsp; Wynik: ______ / 15 &nbsp; Ocena: ______</p>' +
      '<p class="muted">Zakres: odmiana, twierdzenia, pytania, przeczenia, krótkie odpowiedzi · Together 2026, klasa 4</p>' +
      '</div>' + sheetQuestionsHtml(sh.questions) + '</div>';
    if (withKey) h += '<div class="sheet-page sheet-key"><h2>🔑 Klucz odpowiedzi · sprawdzian ' + (i + 1) +
      ' (dla rodzica / nauczyciela)</h2>' + sheetKeyHtml(sh.questions) + '</div>';
    return h;
  }
  function renderSheets() {
    const host = $("#sheets-grid"); if (!host) return;
    const list = currentSheets(), custom = list !== D.printTests;
    host.innerHTML = '<div class="grid3">' + list.map(function (sh, i) {
      const areas = [...new Set(sh.questions.map(function (q) { return q.area || "odmiana"; }))].join(" · ");
      return '<div class="card flat sheet-card"><span class="pill ' + (custom ? "sun" : "teal") + '">' +
        (custom ? "NOWY KOMPLET" : "GOTOWY") + ' · ' + (i + 1) + '/6</span>' +
        '<h3>📄 Sprawdzian ' + (i + 1) + '</h3>' +
        '<p class="muted">15 zadań · ' + esc(areas) + '</p>' +
        '<div class="row"><button class="btn small" data-sheet="' + i + '">🖨️ Drukuj</button>' +
        '<button class="btn small ghost" data-key="' + i + '">🔑 Klucz</button>' +
        '<button class="btn small ghost" data-preview="' + i + '">👀 Podgląd</button></div></div>';
    }).join("") + "</div>";
    $$("#sheets-grid [data-sheet]").forEach(function (b) { b.onclick = function () { doPrint("sheet:" + b.dataset.sheet); }; });
    $$("#sheets-grid [data-key]").forEach(function (b) { b.onclick = function () { doPrint("keys:" + b.dataset.key); }; });
    $$("#sheets-grid [data-preview]").forEach(function (b) {
      b.onclick = function () { SH = +b.dataset.preview; drawSheetsPreview(); };
    });
    if (SH === null) SH = 0;
    drawSheetsPreview();
  }
  function drawSheetsPreview() {
    const host = $("#sheets-preview"); if (!host) return;
    const list = currentSheets(), sh = list[SH] || list[0];
    host.innerHTML = '<div class="between"><h3 style="margin:0">Podgląd: sprawdzian ' + (SH + 1) +
      '</h3><span class="muted">Odpowiedzi są tylko w kluczu do druku 🔑</span></div>' +
      sheetQuestionsHtml(sh.questions);
  }
  function generateSheets() {
    const qs = deal("sheets", D.sheetBank, 90);
    S.sheets = [];
    for (let i = 0; i < 6; i++) S.sheets.push({ id: "N" + (i + 1), title: "Nowy sprawdzian " + (i + 1),
                                                 questions: qs.slice(i * 15, (i + 1) * 15) });
    SH = 0; save(); renderSheets(); updateHub();
    toast("🆕 Wygenerowano 6 nowych, niepowtarzalnych sprawdzianów");
  }
  const bns = $("#btn-new-sheets"); if (bns) bns.onclick = generateSheets;
  const bos = $("#btn-original-sheets");
  if (bos) bos.onclick = function () { S.sheets = []; SH = 0; save(); renderSheets(); toast("📄 Wrócono do 6 przygotowanych sprawdzianów"); };
  const bpa = $("#btn-print-all-sheets"); if (bpa) bpa.onclick = function () { doPrint("sheets:all"); };

  function doPrint(kind) {
    const sheet = $("#print-sheet");
    const isSheets = kind.indexOf("sheet") === 0 || kind.indexOf("keys") === 0;
    sheet.className = isSheets ? "exam-mode sheets-mode" : (kind === "exam" ? "exam-mode" : "");
    $("#print-title").textContent = isSheets ? "TO BE — sprawdziany do druku" :
      (kind === "exam" ? "TO BE — kartkówka do druku" : "TO BE — ściąga przed kartkówką");
    $("#print-report").innerHTML = kind === "report" ? "<div class='card'><h2>Raport postępów · " + today() + "</h2>" + $("#parent-report").innerHTML + "</div>" : "";
    $("#print-mine").innerHTML = (kind === "exam" || isSheets) ? "" :
      '<div class="card"><h2>Moje zdania i samoocena 🚦</h2>' +
      (S.mySentences.length ? "<p><b>Własne zdania:</b> " + S.mySentences.map(function (s) { return esc(s); }).join(" · ") + "</p>" : "<p>Nie zapisano jeszcze własnych zdań (Laboratorium).</p>") +
      (Object.keys(S.lights).length ? "<p><b>Samoocena:</b> " + LIGHT_SKILLS.filter(function (x) { return S.lights[x.k]; }).map(function (x) {
        const l = LIGHTS.filter(function (z) { return z.id === S.lights[x.k]; })[0];
        return (l ? l.ico : "") + " " + esc(x.ican);
      }).join(" · ") + "</p>" : "<p>Brak samooceny światełkami.</p>") + "</div>";
    let examHtml = "";
    if (kind === "exam") examHtml = buildPrintExam();
    else if (kind === "sheets:all") {
      examHtml = currentSheets().map(function (sh, i) { return sheetPage(sh, i, true); }).join("");
    } else if (kind.indexOf("sheet:") === 0) {
      const i = +kind.split(":")[1], list = currentSheets();
      if (list[i]) examHtml = sheetPage(list[i], i, true);
    } else if (kind.indexOf("keys:") === 0) {
      const i = +kind.split(":")[1], list = currentSheets();
      if (list[i]) examHtml = '<div class="sheet-page sheet-key"><h2>🔑 Klucz odpowiedzi · sprawdzian ' +
        (i + 1) + ' (dla rodzica / nauczyciela)</h2>' + sheetKeyHtml(list[i].questions) + '</div>';
    }
    $("#print-exam").innerHTML = examHtml;
    try { window.print(); } catch (e) { toast("Drukowanie niedostępne w tej przeglądarce."); }
  }
  ["btn-print-start", "btn-print-learn"].forEach(function (id) { $("#" + id).onclick = function () { doPrint("cheat"); }; });
  const bpe = $("#btn-print-exam"); if (bpe) bpe.onclick = function () { doPrint("exam"); };

  /* ---------------- hub ćwiczeń ---------------- */
  function renderHub() {
    const hb = $("#hub-bank");
    if (hb) hb.innerHTML = "🧠 <b>" + globalTotal().toLocaleString("pl-PL") + "</b> różnych treści · " +
      "widziano <b>" + seenTotal().toLocaleString("pl-PL") + "</b> · pozostało <b>" +
      Math.max(0, globalTotal() - seenTotal()).toLocaleString("pl-PL") + "</b> nowych";
    $("#activity-grid").innerHTML = D.activities.map(function (a) {
      const st = S.acts[a.id];
      const done = st && st.done ? '<span class="done">✓ ukończone · ' + st.ok + "/" + st.tried + " poprawnych</span>" : (st && st.tried ? '<span class="done">rozpoczęte · ' + st.tried + " zadań</span>" : "");
      return '<button class="tile" data-act="' + a.id + '"><span class="ico">' + a.icon + "</span><b>" + a.title + "</b><small>" + a.desc + "</small>" + done + "</button>";
    }).join("");
    updateHub();
  }
  function updateHub() {
    const done = D.activities.filter(function (a) { return S.acts[a.id] && S.acts[a.id].done; }).length;
    const h = $("#hub-progress"); if (h) h.textContent = "Ukończone: " + done + " z " + D.activities.length;
    const hb = $("#hub-bank");
    if (hb) hb.innerHTML = "🧠 <b>" + globalTotal().toLocaleString("pl-PL") + "</b> różnych treści · " +
      "widziano <b>" + seenTotal().toLocaleString("pl-PL") + "</b> · pozostało <b>" +
      Math.max(0, globalTotal() - seenTotal()).toLocaleString("pl-PL") + "</b> nowych";
  }
  $("#btn-random").onclick = function () { openActivity(shuffle(D.activities)[0].id); };
  $("#btn-back-hub").onclick = function () { clearSpeed(); $("#practice-run").style.display = "none"; $("#practice-hub").style.display = "block"; updateHub(); };

  /* ---------------- wspólny silnik sesji ---------------- */
  function Session(act, items, count, renderer) {
    this.act = act; this.items = deal(act, items, count || 8);
    this.i = 0; this.ok = 0; this.streak = 0; this.renderer = renderer;
    this.host = $("#run-host"); this.spoken = "";
  }
  Session.prototype.show = function () {
    if (this.i >= this.items.length) return finishSession(this);
    this.spoken = "";
    updateRun(this);
    this.renderer(this.items[this.i], this);
  };
  Session.prototype.next = function () { this.i++; this.show(); };
  function updateRun(s) {
    $("#run-progress").style.width = pct(s.i, s.items.length) + "%";
    $("#run-score").textContent = "Wynik: " + s.ok + " / " + s.i;
    $("#run-streak").textContent = "🔥 " + s.streak + " z rzędu";
  }
  function step(s, cfg) {
    s.host.innerHTML =
      '<div class="qbox"><div class="q">' + (cfg.q || "") + "</div>" + (cfg.hint ? '<div class="hint">' + cfg.hint + "</div>" : "") + "</div>" +
      (cfg.pre || "") +
      '<div id="step-body">' + (cfg.body || "") + "</div>" +
      '<div id="hintbox"></div><div class="feedback" id="fb"></div>' +
      '<div class="ex-actions" id="step-actions">' + (cfg.actions || "") + "</div>";
  }
  function showHint(s, text) {
    $("#hintbox").innerHTML = '<div class="feedback tip show">💡 ' + esc(text) + "</div>";
  }
  function check(s, ok, correctText, why) {
    if (ok) { s.ok++; s.streak++; } else s.streak = 0;
    logAnswer(s.items[s.i], s.act, ok, correctText, why);
    mark(s.act, ok);
    updateRun(s);
    const fb = $("#fb");
    fb.className = "feedback show " + (ok ? "ok" : "no");
    fb.innerHTML = (ok ? "✅ <b>Brawo!</b> " : "❌ <b>Poprawnie:</b> <u>" + esc(correctText || "") + "</u>. ") +
      (why ? '<span style="font-weight:600">' + esc(why) + "</span>" : "");
    if (ok && s.streak > 0 && s.streak % 5 === 0) { confetti(12); toast("🔥 " + s.streak + " odpowiedzi z rzędu!"); }
    const last = s.i + 1 >= s.items.length;
    $("#step-actions").innerHTML =
      '<button class="btn" id="btn-next-step">' + (last ? "Zobacz wynik 🏁" : "Dalej →") + "</button>" +
      (s.spoken ? '<button class="btn small ghost" id="btn-say-sol">🔊 Posłuchaj odpowiedzi</button>' : "");
    $("#btn-next-step").onclick = function () { s.next(); };
    const bs = $("#btn-say-sol"); if (bs) bs.onclick = function () { speak(s.spoken); };
  }
  function finishSession(s) {
    const total = s.items.length, p = pct(s.ok, total);
    S.acts[s.act].done = true; save(); updateHub();
    $("#run-progress").style.width = "100%";
    $("#run-score").textContent = "Wynik: " + s.ok + " / " + total;
    summaryHost(s.host, s.act, s.ok, total, p >= 80 ? "Świetnie! Robbie jest z Ciebie dumny 🤖" : "Dobra robota! Wróć tu jeszcze raz — powtórka robi mistrza 💪");
  }
  function summaryHost(host, act, ok, total, msg) {
    const p = pct(ok, total);
    recordGrade("ćwiczenie", act, ok, total);
    const due = dueReviews().length;
    host.innerHTML = '<div class="center" style="padding:10px 0"><div style="font-size:3.4rem">' + (p >= 80 ? "🏆" : p >= 50 ? "👍" : "🌱") + '</div>' +
      '<h2>' + ok + " / " + total + " poprawnych · " + p + '%</h2>' +
      '<div style="font-size:1.25rem;font-weight:900;color:var(--teal-d)">Ocena: ' + gradeBadge(p) + '</div>' +
      '<p class="muted">' + esc(msg) + "</p>" +
      '<p class="muted">🎲 Następne wejście = <b>całkiem nowy zestaw</b> z ' + bankSize(act).toLocaleString("pl-PL") + " zadań.</p>" +
      (due ? '<p class="muted">🔁 Czeka ' + due + ' zadań do powtórki w zakładce <b>Powtórki</b>.</p>' : "") +
      '<div class="row" style="justify-content:center;margin-top:16px">' +
      '<button class="btn" id="btn-again">🔁 Nowy zestaw</button>' +
      '<button class="btn ghost" id="btn-other">🎮 Inne ćwiczenia</button>' +
      '<button class="btn orange" id="btn-to-exam">📝 Próbna kartkówka</button>' +
      (due ? '<button class="btn small yellow" id="btn-to-review">🔁 Powtórz błędy</button>' : "") + '</div>' +
      exitTicketHtml(act) + '</div>';
    $$("#run-host [data-exit]").forEach(function (b) {
      b.onclick = function () {
        const sk = skillOf(act);
        S.lights[sk] = b.dataset.exit;
        S.exitTickets.unshift({ d: today(), act: act, skill: sk, light: b.dataset.exit });
        if (S.exitTickets.length > 40) S.exitTickets.length = 40;
        save();
        $$("#run-host [data-exit]").forEach(function (x) { x.classList.toggle("on", x === b); });
        renderLights(); renderReviewStats();
        toast("🎫 Zapisano samoocenę — dziękuję!");
      };
    });
    $("#btn-again").onclick = function () { openActivity(act); };
    $("#btn-other").onclick = function () { $("#practice-run").style.display = "none"; $("#practice-hub").style.display = "block"; updateHub(); go("practice"); };
    $("#btn-to-exam").onclick = function () { go("exam"); };
    const br = $("#btn-to-review"); if (br) br.onclick = function () { go("review"); };
    if (p >= 80) confetti(30);
    checkBadges();
  }

  /* ---------------- otwieranie ćwiczenia ---------------- */
  let activeSpeed = null, activeExamTimer = null;
  function clearSpeed() { if (activeSpeed) { clearInterval(activeSpeed); activeSpeed = null; } }
  function clearExamTimer() { if (activeExamTimer) { clearInterval(activeExamTimer); activeExamTimer = null; } }
  function openActivity(id) {
    clearSpeed();
    const a = D.activities.find(function (x) { return x.id === id; });
    if (!a) return;
    go("practice");
    $("#practice-hub").style.display = "none";
    $("#practice-run").style.display = "block";
    $("#run-icon").textContent = a.icon;
    $("#run-title").textContent = a.title;
    $("#run-desc").textContent = a.desc;
    $("#run-host").innerHTML = "";
    $("#run-progress").style.width = "0%";
    $("#run-score").textContent = "Wynik: 0 / 0";
    $("#run-streak").textContent = "🔥 0 z rzędu";
    IMPL[id]();
    /* etykieta po utworzeniu sesji — deal() może właśnie rozpocząć nowy cykl */
    const rs = $("#run-set");
    if (rs) rs.textContent = "🎲 Zestaw " + (S.cycles[id] || 1) + " · baza " + bankSize(id).toLocaleString("pl-PL") + " zadań · bez powtórek";
    window.scrollTo({ top: 0, behavior: "smooth" });
  }

  /* ============ 1. Wybierz formę ============ */
  function actChoice() {
    new Session("choice", D.choice, 10, function (it, s) {
      s.spoken = it.s.replace("___", it.a);
      step(s, {
        q: it.s.replace("___", "<em>___</em>"),
        hint: "Która forma pasuje? Pomyśl o zaimku (I / he / they…).",
        body: '<div class="options">' + shuffle(it.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>"
      });
      $$("#step-body .opt").forEach(function (b) {
        b.onclick = function () {
          const ok = b.dataset.v === it.a;
          $$("#step-body .opt").forEach(function (x) {
            x.disabled = true;
            if (x.dataset.v === it.a) x.classList.add("correct");
            else if (x === b) x.classList.add("wrong");
          });
          check(s, ok, it.a, it.w);
        };
      });
    }).show();
  }

  /* ============ 2. Wpisz formę ============ */
  function actGap() {
    new Session("gap", D.gap, 10, function (it, s) {
      s.spoken = it.w;
      step(s, {
        q: it.s.replace("___", "<em>___</em>"),
        hint: "Wpisz brakującą formę. Możesz użyć pełnej lub krótkiej formy.",
        body: '<input class="answer-input" id="ans" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="wpisz po angielsku…">',
        actions: '<button class="btn" id="btn-check">Sprawdź ✓</button><button class="btn small ghost" id="btn-hint">💡 Podpowiedź</button><button class="btn small ghost" id="btn-say">🔊 Przeczytaj zdanie</button>'
      });
      const inp = $("#ans");
      function submit() {
        const v = norm(inp.value);
        if (!v) { toast("Najpierw coś wpisz 🙂"); inp.focus(); return; }
        const ok = it.a.some(function (x) { return norm(x) === v; });
        inp.disabled = true;
        check(s, ok, it.w, it.w);
      }
      $("#btn-check").onclick = submit;
      inp.onkeydown = function (e) { if (e.key === "Enter") submit(); };
      $("#btn-hint").onclick = function () { showHint(s, it.h); };
      $("#btn-say").onclick = function () { speak(it.w); };
      inp.focus();
    }).show();
  }

  /* ============ 3. Memory ============ */
  function actMemory() {
    const first = deal("memory", D.pairSets, 1)[0];
    let setIndex = Math.max(0, D.pairSets.indexOf(first));
    buildMemory(setIndex);
    function buildMemory(si) {
      const set = D.pairSets[si];
      let lock = false, first = null, matched = 0, moves = 0;
      const cards = shuffle(set.pairs.reduce(function (acc, p, i) {
        acc.push({ t: p[0], id: i }, { t: p[1], id: i }); return acc;
      }, []));
      $("#run-host").innerHTML =
        '<div class="card flat" style="background:var(--teal-l);border-color:#c6ece7"><div class="between"><b>🃏 ' + esc(set.name) + '</b><span class="muted" id="mem-info">Ruchy: 0 · Pary: 0/' + set.pairs.length + "</span></div>" +
        '<div class="row" style="margin-top:10px">' + D.pairSets.map(function (p, i) { return '<button class="btn small ' + (i === si ? "" : "ghost") + '" data-set="' + i + '">' + esc(p.name) + "</button>"; }).join("") +
        '<button class="btn small ghost" id="mem-new">🔀 Nowa rozgrywka</button></div></div>' +
        '<p class="muted">Odkryj dwie karty. Jeśli tworzą parę — zostają odkryte. Kliknij kartę, żeby usłyszeć słowo 🔊 (po odkryciu).</p>' +
        '<div class="mem-grid" id="mem-grid">' + cards.map(function (c, i) { return '<button class="mem-card" data-i="' + i + '" aria-label="Zakryta karta"><span>' + esc(c.t) + "</span>?</button>"; }).join("") + "</div>";
      $$("[data-set]").forEach(function (b) { b.onclick = function () { buildMemory(+b.dataset.set); }; });
      $("#mem-new").onclick = function () {
        const nx = deal("memory", D.pairSets, 1)[0];
        buildMemory(Math.max(0, D.pairSets.indexOf(nx)));
        toast("🔀 Nowy zestaw kart");
      };
      const grid = $("#mem-grid");
      grid.onclick = function (e) {
        const b = e.target.closest(".mem-card"); if (!b || lock) return;
        const i = +b.dataset.i, c = cards[i];
        if (b.classList.contains("open") || b.classList.contains("matched")) return;
        b.classList.add("open"); b.innerHTML = "<span>" + esc(c.t) + "</span>";
        if (c.t.length < 14) speak(c.t);
        if (first === null) { first = { i: i, id: c.id, btn: b }; return; }
        moves++;
        if (first.id === c.id) {
          b.classList.add("matched"); first.btn.classList.add("matched");
          matched++; first = null;
          $("#mem-info").textContent = "Ruchy: " + moves + " · Pary: " + matched + "/" + set.pairs.length;
          if (matched === set.pairs.length) {
            setTimeout(function () {
              award("memory", set.pairs.length, set.pairs.length);
              summaryHost($("#run-host"), "memory", set.pairs.length, set.pairs.length,
                "Wszystkie pary odnalezione w " + moves + " ruchach! 🧠");
            }, 450);
          }
        } else {
          lock = true; const f = first; first = null;
          setTimeout(function () {
            [f.btn, b].forEach(function (x) { x.classList.remove("open"); x.innerHTML = '<span>' + esc(cards[+x.dataset.i].t) + "</span>?"; });
            lock = false;
            $("#mem-info").textContent = "Ruchy: " + moves + " · Pary: " + matched + "/" + set.pairs.length;
          }, 800);
        }
      };
    }
  }

  /* ============ 4. Połącz w pary ============ */
  function actMatch() {
    const first = deal("match", D.matchSets, 1)[0];
    let si = Math.max(0, D.matchSets.indexOf(first));
    build(si);
    function build(k) {
      const set = D.matchSets[k];
      const L = shuffle(set.pairs.map(function (p, i) { return { t: p[0], id: i, side: "L" }; }));
      const R = shuffle(set.pairs.map(function (p, i) { return { t: p[1], id: i, side: "R" }; }));
      let sel = null, done = 0, tries = 0, okCount = 0;
      $("#run-host").innerHTML =
        '<div class="card flat" style="background:var(--sun-l);border-color:#ffe49b"><b>🔗 ' + esc(set.name) + '</b><div class="muted">Kliknij najpierw coś z lewej kolumny, potem pasującą rzecz z prawej.</div>' +
        '<div class="row" style="margin-top:8px">' + D.matchSets.map(function (p, i) { return '<button class="btn small ' + (i === k ? "" : "ghost") + '" data-ms="' + i + '">' + esc(p.name) + "</button>"; }).join("") + "</div></div>" +
        '<div id="match-wrap"><div class="match-cols"><div class="match-col">' + L.map(function (x) { return '<button class="match-item" data-side="L" data-id="' + x.id + '">' + esc(x.t) + "</button>"; }).join("") + '</div><div class="match-col">' + R.map(function (x) { return '<button class="match-item" data-side="R" data-id="' + x.id + '">' + esc(x.t) + "</button>"; }).join("") + "</div></div>" +
        '<p class="muted" id="match-info">Pozostało par: ' + set.pairs.length + "</p></div>";
      $$("[data-ms]").forEach(function (b) { b.onclick = function () { build(+b.dataset.ms); }; });
      const wrap = $("#match-wrap");
      wrap.onclick = function (e) {
        const b = e.target.closest(".match-item"); if (!b || b.classList.contains("done")) return;
        if (!sel) { sel = b; b.classList.add("sel"); speak(b.textContent); return; }
        if (sel === b) { b.classList.remove("sel"); sel = null; return; }
        if (sel.dataset.side === b.dataset.side) { $$(".match-item.sel").forEach(function (x) { x.classList.remove("sel"); }); sel = b; b.classList.add("sel"); return; }
        tries++;
        if (sel.dataset.id === b.dataset.id) {
          sel.classList.remove("sel"); sel.classList.add("done"); b.classList.add("done");
          sel.disabled = true; b.disabled = true; done++; okCount++;
          toast("✅ Para połączona!");
        } else {
          b.classList.add("wiggle"); sel.classList.add("wiggle");
          const s0 = sel; sel = null;
          setTimeout(function () { [b, s0].forEach(function (x) { x.classList.remove("wiggle", "sel"); }); }, 500);
        }
        $("#match-info").textContent = "Pozostało par: " + (set.pairs.length - done);
        if (done === set.pairs.length) {
          setTimeout(function () {
            award("match", okCount, tries);
            summaryHost($("#run-host"), "match", okCount, tries, "Wszystkie pary połączone! 💬");
          }, 400);
        }
      };
    }
  }

  /* ============ wspólny klockowy budowniczy zdań ============ */
  function buildSentence(tokens) {
    let out = "";
    tokens.forEach(function (t) {
      if (/^[.!?]$/.test(t)) out = out.replace(/\s+$/, "") + t + " ";
      else out += t + " ";
    });
    return out.trim();
  }
  function wordBuilder(it, s, opts) {
    s.picked = [];
    s.spoken = it.answer;
    const bankOrder = shuffle(it.words.map(function (_, i) { return i; }));
    const extra = it.extra ? '<div class="feedback tip show">⚠️ Uwaga! Jeden wyraz jest niepotrzebny — zostaw go.</div>' : "";
    step(s, {
      q: opts.q + (opts.pl ? ' <span class="pill coral">' + esc(opts.pl) + "</span>" : ""),
      hint: opts.hint,
      pre: extra,
      body: '<div class="answer-slot" id="slot"></div><div class="tiles" id="bank"></div>' +
            '<div class="row"><button class="btn small ghost" id="btn-undo">↩ Cofnij</button><button class="btn small ghost" id="btn-clear">🧹 Wyczyść</button>' +
            '<button class="btn small ghost" id="btn-say-build">🔊 Przeczytaj moje zdanie</button></div>',
      actions: '<button class="btn" id="btn-check">Sprawdź ✓</button><button class="btn small ghost" id="btn-hint">💡 Podpowiedź</button>'
    });
    function tokensHTML(arr, from) {
      return arr.map(function (idx) {
        const w = it.words[idx];
        return '<button class="token ' + (from === "slot" ? "in-answer" : (s.picked.indexOf(idx) !== -1 ? "used" : "")) + '" data-from="' + from + '" data-idx="' + idx + '">' + esc(w) + "</button>";
      }).join("");
    }
    function draw() {
      $("#slot").innerHTML = tokensHTML(s.picked, "slot");
      $("#bank").innerHTML = tokensHTML(bankOrder, "bank");
    }
    draw();
    $("#step-body").onclick = function (e) {
      const b = e.target.closest("[data-idx]"); if (!b) return;
      const idx = +b.dataset.idx;
      if (b.dataset.from === "bank") { if (s.picked.indexOf(idx) === -1) s.picked.push(idx); }
      else s.picked = s.picked.filter(function (x) { return x !== idx; });
      draw();
    };
    $("#btn-undo").onclick = function () { s.picked.pop(); draw(); };
    $("#btn-clear").onclick = function () { s.picked = []; draw(); };
    $("#btn-say-build").onclick = function () { speak(buildSentence(s.picked.map(function (i) { return it.words[i]; })) || "Click the words"); };
    $("#btn-hint").onclick = function () { showHint(s, opts.hintText || "Zacznij od osoby (I / he / they…), potem daj formę be."); };
    $("#btn-check").onclick = function () {
      const made = buildSentence(s.picked.map(function (i) { return it.words[i]; }));
      if (!made) { toast("Ułóż zdanie z klocków 🙂"); return; }
      const ok = norm(made) === norm(it.answer);
      check(s, ok, it.answer, it.w || "Pamiętaj o kolejności: osoba → be → reszta.");
    };
  }

  /* ============ 5. Rozsypanka ============ */
  function actOrder() {
    new Session("order", D.order, 8, function (it, s) {
      wordBuilder(it, s, {
        q: "Ułóż zdanie z wyrazów:",
        hint: "Klikaj klocki w dobrej kolejności. Kliknięty klocek w zdaniu wraca na dół.",
        hintText: "Pytanie zaczyna się od formy be (Are / Is / Where are…), twierdzenie od osoby.",
        pl: ""
      });
    }).show();
  }

  /* ============ 6. Przekształcenia ============ */
  function actTransform() {
    new Session("transform", D.transform, 8, function (it, s) {
      s.spoken = it.show;
      const isNeg = it.task === "przeczenie";
      step(s, {
        q: "Zrób <span class='pill " + (isNeg ? "coral" : "teal") + "'>" + it.task.toUpperCase() + "</span> z tego zdania:",
        hint: it.from,
        body: '<div class="qbox" style="background:#fff;margin-bottom:12px"><div class="q">“' + esc(it.from) + '”</div></div>' +
              '<input class="answer-input" id="ans" autocomplete="off" spellcheck="false" placeholder="napisz całe zdanie…">',
        actions: '<button class="btn" id="btn-check">Sprawdź ✓</button><button class="btn small ghost" id="btn-hint">💡 Podpowiedź</button><button class="btn small ghost" id="btn-say">🔊 Przeczytaj zdanie</button>'
      });
      const inp = $("#ans");
      function submit() {
        const v = norm(inp.value);
        if (!v) { toast("Napisz zdanie 🙂"); inp.focus(); return; }
        const ok = it.a.some(function (x) { return norm(x) === v; });
        inp.disabled = true;
        check(s, ok, it.show, it.w);
      }
      $("#btn-check").onclick = submit;
      inp.onkeydown = function (e) { if (e.key === "Enter") submit(); };
      $("#btn-hint").onclick = function () { showHint(s, isNeg ? "Dodaj not po be: is not → isn't, are not → aren't, I am not → I'm not." : "Przenieś be na początek i dodaj znak zapytania."); };
      $("#btn-say").onclick = function () { speak(it.from); };
      inp.focus();
    }).show();
  }

  /* ============ 7. Krótkie odpowiedzi ============ */
  function actShorts() {
    new Session("shorts", D.shorts, 8, function (it, s) {
      s.spoken = it.a;
      step(s, {
        q: '<span style="font-size:.7em">💬 PYTANIE</span><br>“' + esc(it.q) + '”',
        hint: "Odpowiedz: <b>" + esc(it.cue) + "</b> — wybierz właściwą krótką odpowiedź.",
        body: '<div class="options">' + shuffle(it.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>"
      });
      $$("#step-body .opt").forEach(function (b) {
        b.onclick = function () {
          const ok = b.dataset.v === it.a;
          $$("#step-body .opt").forEach(function (x) { x.disabled = true; if (x.dataset.v === it.a) x.classList.add("correct"); else if (x === b) x.classList.add("wrong"); });
          check(s, ok, it.a, it.w);
        };
      });
    }).show();
  }

  /* ============ 8. Detektyw błędów ============ */
  function actDetective() {
    new Session("detective", D.detective, 8, function (it, s) {
      s.spoken = it.a;
      step(s, {
        q: "🕵️ Znajdź błędne słowo, a potem wybierz poprawne zdanie",
        hint: "Najpierw kliknij słowo, które jest nie tak. Potem wybierz poprawną wersję całego zdania.",
        body: '<div class="qbox" style="background:#fff"><div class="sentence" id="sent">' + it.words.map(function (w, i) { return '<button class="word" data-w="' + i + '">' + esc(w) + "</button>"; }).join(" ") + "</div></div>" +
              '<div id="fixes"></div>'
      });
      $("#sent").onclick = function (e) {
        const b = e.target.closest(".word"); if (!b || $("#fixes").innerHTML) return;
        const i = +b.dataset.w;
        if (i !== it.bad) { b.classList.add("wiggle"); setTimeout(function () { b.classList.remove("wiggle"); }, 500); toast("Nie to słowo… szukaj formy be 🙂"); return; }
        $$("#sent .word").forEach(function (x) { x.classList.remove("picked"); x.disabled = true; });
        b.classList.add("picked");
        $("#fixes").innerHTML = '<p><b>Teraz wybierz poprawne zdanie:</b></p><div class="options">' +
          shuffle(it.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>";
        $$("#fixes .opt").forEach(function (o) {
          o.onclick = function () {
            const ok = o.dataset.v === it.a;
            $$("#fixes .opt").forEach(function (x) { x.disabled = true; if (x.dataset.v === it.a) x.classList.add("correct"); else if (x === o) x.classList.add("wrong"); });
            $("#sent").innerHTML = '<span class="fixed">' + esc(it.a) + "</span>";
            check(s, ok, it.a, it.w);
          };
        });
      };
    }).show();
  }

  /* ============ 9. Sortownik zdań ============ */
  const BUCKETS = [
    { id: "affirmative", name: "➕ Twierdzenie" },
    { id: "question", name: "❓ Pytanie" },
    { id: "negative", name: "➖ Przeczenie" }
  ];
  function actSort() {
    const items = deal("sort", D.sort, 12).map(function (x) { return Object.assign({}, x, { done: false }); });
    let sel = null, done = 0, tries = 0, okCount = 0;
    const placed = { affirmative: [], question: [], negative: [] };
    function draw() {
      $("#run-host").innerHTML =
        '<p class="muted">Kliknij zdanie, potem kliknij właściwy koszyk. Zdania z <b>not / isn’t / aren’t</b> to przeczenia, z <b>?</b> i <i>be</i> na początku to pytania.</p>' +
        '<div id="sort-wrap"><div class="tiles" style="flex-direction:column;align-items:stretch" id="sort-list">' +
        items.map(function (x, i) { return x.done ? "" : '<button class="token" data-s="' + i + '" style="text-align:left">' + esc(x.s) + "</button>"; }).join("") +
        '</div><div class="buckets">' + BUCKETS.map(function (b) {
          return '<button class="bucket ' + b.id + '" data-b="' + b.id + '">' + b.name + "<small>" + (placed[b.id].length ? esc(placed[b.id].join(" · ")) : "wrzuć tutaj") + "</small></button>";
        }).join("") + "</div><p class='muted' id='sort-info'>Pozostało zdań: " + (items.length - done) + "</p></div>";
      const wrap = $("#sort-wrap");
      wrap.onclick = function (e) {
        const sbtn = e.target.closest("[data-s]");
        const bbtn = e.target.closest("[data-b]");
        if (sbtn) {
          $$("#sort-list .token").forEach(function (x) { x.classList.remove("in-answer"); });
          sbtn.classList.add("in-answer"); sel = +sbtn.dataset.s; speak(items[sel].s); return;
        }
        if (bbtn) {
          if (sel === null) { toast("Najpierw wybierz zdanie 🙂"); return; }
          const it = items[sel]; tries++;
          if (bbtn.dataset.b === it.t) {
            logAnswer(it, "sort", true, it.s, "To zdanie to " + it.t + ".");
            it.done = true; placed[it.t].push(it.s); done++; okCount++; toast("✅ Dobrze!"); sel = null; draw();
            if (done === items.length) {
              award("sort", okCount, tries);
              summaryHost($("#run-host"), "sort", okCount, tries, "Wszystkie zdania posegregowane! 🗂️");
              return;
            }
          } else {
            logAnswer(it, "sort", false, it.s, "Poprawny koszyk: " + it.t + ".");
            bbtn.classList.add("wiggle"); const bb = bbtn;
            setTimeout(function () { bb.classList.remove("wiggle"); }, 500);
            toast("❌ To nie ten koszyk — spróbuj jeszcze raz.");
            $("#sort-info").textContent = "Pozostało zdań: " + (items.length - done);
          }
        }
      };
    }
    draw();
  }

  /* ============ 10. Prawda / Fałsz ============ */
  function actReading() {
    const first = deal("reading", D.reading, 1)[0];
    let ti = Math.max(0, D.reading.indexOf(first));
    build(ti);
    function build(k) {
      const r = D.reading[k];
      let idx = 0, ok = 0, tries = 0;
      function draw() {
        if (idx >= r.items.length) {
          award("reading", ok, tries);
          summaryHost($("#run-host"), "reading", ok, tries, "Tekst przeczytany! 📖 Spróbuj też innego tekstu.");
          const b = document.createElement("button"); b.className = "btn ghost"; b.textContent = "📄 Inny tekst";
          b.onclick = function () {
            const nx = deal("reading", D.reading, 1)[0];
            build(Math.max(0, D.reading.indexOf(nx)));
          };
          $("#run-host .row").appendChild(b);
          return;
        }
        const it = r.items[idx];
        $("#run-host").innerHTML =
          '<div class="card flat" style="background:var(--sun-l);border-color:#ffe49b"><b>📖 ' + esc(r.title) + '</b> <button class="btn small ghost" id="read-say">🔊 Posłuchaj tekstu</button></div>' +
          '<div class="reading-text">' + esc(r.text) + "</div>" +
          '<div id="read-wrap"><div class="qbox"><div class="q">' + esc(it.s) + '</div><div class="hint">Pytanie ' + (idx + 1) + " z " + r.items.length + " — prawda czy fałsz?</div></div>" +
          '<div class="tf-buttons"><button class="btn" data-tf="true">✅ Prawda (T)</button><button class="btn orange" data-tf="false">❌ Fałsz (F)</button></div>' +
          '<div class="feedback" id="fb"></div><div class="ex-actions" id="read-actions"></div></div>';
        $("#read-say").onclick = function () { speak(r.text); };
        $("#read-wrap").onclick = function (e) {
          const b = e.target.closest("[data-tf]"); if (!b || b.disabled) return;
          const good = (b.dataset.tf === "true") === it.t;
          logAnswer(it, "reading", good, it.t ? "Prawda" : "Fałsz", it.w);
          tries++; if (good) ok++;
          const fb = $("#fb"); fb.className = "feedback show " + (good ? "ok" : "no");
          fb.innerHTML = (good ? "✅ Zgadza się! " : "❌ Poprawnie: " + (it.t ? "Prawda" : "Fałsz") + ". ") + esc(it.w);
          $$("[data-tf]").forEach(function (x) { x.disabled = true; });
          $("#read-actions").innerHTML = '<button class="btn" id="read-next">' + (idx + 1 >= r.items.length ? "Zobacz wynik 🏁" : "Dalej →") + "</button>";
          $("#read-next").onclick = function () { idx++; draw(); };
        };
      }
      draw();
    }
  }

  /* ============ 11. Słuchaj i wybierz ============ */
  function actListen() {
    new Session("listen", D.listening, 8, function (it, s) {
      s.spoken = it.a;
      step(s, {
        q: "🎧 Co słyszysz?",
        hint: "Robbie przeczyta jedno zdanie. Kliknij ▶ (możesz kilka razy), potem wybierz właściwą odpowiedź.",
        body: '<div class="row"><button class="btn" id="play">▶ Odtwórz</button><button class="btn small ghost" id="play-slow">🐢 Wolniej</button><button class="btn small ghost" id="show-text">👀 Nie mam dźwięku</button></div>' +
              '<div id="audio-text" class="qbox" style="margin-top:12px;display:none"></div>' +
              '<div class="options" style="margin-top:14px">' + shuffle(it.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>" +
              '<p class="muted" style="margin-top:12px">🎤 Powiedz to zdanie na głos 3 razy, zanim klikniesz — to świetny trening przed kartkówką!</p>',
        actions: ""
      });
      $("#play").onclick = function () { speak(it.spoken); };
      $("#play-slow").onclick = function () { speak(it.spoken, true); };
      $("#show-text").onclick = function () { const t = $("#audio-text"); t.style.display = "block"; t.innerHTML = "<b>Tekst:</b> " + esc(it.spoken); };
      try { speak(it.spoken); } catch (e) {}
      $$("#step-body .opt").forEach(function (b) {
        b.onclick = function () {
          const ok = b.dataset.v === it.a;
          $$("#step-body .opt").forEach(function (x) { x.disabled = true; if (x.dataset.v === it.a) x.classList.add("correct"); else if (x === b) x.classList.add("wrong"); });
          check(s, ok, it.a, "Posłuchaj jeszcze raz i porównaj.");
        };
      });
    }).show();
  }

  /* ============ 12. Dyktando ============ */
  function actDictation() {
    new Session("dictation", D.dictation, 8, function (it, s) {
      s.spoken = it.spoken;
      step(s, {
        q: "🔤 Napisz to, co słyszysz",
        hint: "Zwróć uwagę na apostrof: isn't, aren't, I'm, we're, they're.",
        body: '<div class="row"><button class="btn" id="play">▶ Odtwórz</button><button class="btn small ghost" id="play-slow">🐢 Wolniej</button></div>' +
              '<input class="answer-input" id="ans" style="margin-top:12px" autocomplete="off" spellcheck="false" placeholder="wpisz usłyszaną formę…">',
        actions: '<button class="btn" id="btn-check">Sprawdź ✓</button><button class="btn small ghost" id="btn-hint">💡 Podpowiedź</button>'
      });
      const inp = $("#ans");
      $("#play").onclick = function () { speak(it.spoken); };
      $("#play-slow").onclick = function () { speak(it.spoken, true); };
      $("#btn-hint").onclick = function () { showHint(s, it.hint); };
      function submit() {
        const v = norm(inp.value);
        if (!v) { toast("Najpierw coś wpisz 🙂"); inp.focus(); return; }
        const ok = it.a.some(function (x) { return norm(x) === v; });
        inp.disabled = true;
        check(s, ok, it.spoken, "Zapisz apostrof w miejscu, gdzie „zniknęła” litera.");
      }
      $("#btn-check").onclick = submit;
      inp.onkeydown = function (e) { if (e.key === "Enter") submit(); };
      try { speak(it.spoken); } catch (e) {}
      inp.focus();
    }).show();
  }

  /* ============ 13. Tłumacz z polskiego ============ */
  function actTranslate() {
    new Session("translate", D.translate, 8, function (it, s) {
      wordBuilder(it, s, {
        q: "🇬🇧 Ułóż po angielsku:",
        pl: it.pl,
        hint: "Wybierz właściwe klocki — w koszyku są też pułapki!",
        hintText: "Zacznij od osoby, potem forma be (lub be na początku w pytaniu), na końcu reszta."
      });
    }).show();
  }

  /* ============ 14. Literowanie form (wisielec) ============ */
  function actSpelling() {
    const words = deal("spelling", D.spelling, 6);
    let wi = 0, ok = 0, tries = 0;
    function draw() {
      if (wi >= words.length) {
        award("spelling", ok, tries);
        summaryHost($("#run-host"), "spelling", ok, tries, "Formy odgadnięte — apostrofy na miejscu! 🔤");
        return;
      }
      const it = words[wi], w = it.word.toLowerCase();
      const guessed = new Set(); let lives = 6, solved = false;
      function mask() {
        return w.split("").map(function (c) {
          return /[a-z0-9]/.test(c) ? (guessed.has(c) || solved ? c.toUpperCase() : "_") : c;
        }).join(" ");
      }
      function render() {
        const letters = "abcdefghijklmnopqrstuvwxyz".split("");
        $("#run-host").innerHTML =
          '<div class="card flat" style="background:var(--teal-l);border-color:#c6ece7"><div class="between"><b>🎲 Słowo ' + (wi + 1) + " z " + words.length + '</b><span class="muted">Bateria Robbiego: ' + "❤️".repeat(Math.max(0, lives)) + " · " + lives + "/6</span></div>" +
          '<div class="muted">Znaczenie: ' + esc(it.pl) + '</div><div class="row" style="margin-top:8px"><button class="btn small ghost" id="sp-say">🔊 Posłuchaj</button><button class="btn small ghost" id="sp-hint">💡 Odkryj literę</button><button class="btn small ghost" id="sp-skip">⏭ Pomiń</button></div></div>' +
          '<div class="qbox center"><div class="hang-word" id="sp-word">' + mask() + '</div><div class="muted" id="sp-msg">Kliknij litery na klawiaturze. Litery spoza alfabetu (apostrof, przecinek, spacja) są pokazane od razu.</div></div>' +
          '<div id="sp-wrap"><div class="keyboard">' + letters.map(function (c) { return '<button class="key" data-c="' + c + '"' + (guessed.has(c) ? " disabled" : "") + ">" + c.toUpperCase() + "</button>"; }).join("") + "</div>" +
          '<div class="ex-actions" id="sp-actions"></div></div>';
        $("#sp-say").onclick = function () { speak(it.word); };
        $("#sp-hint").onclick = function () {
          const rest = w.split("").filter(function (c) { return /[a-z]/.test(c) && !guessed.has(c); });
          if (rest.length) { guessed.add(rest[0]); render(); }
        };
        $("#sp-skip").onclick = function () { solved = true; $("#sp-word").textContent = it.word.toUpperCase(); finishWord(false); };
        $("#sp-wrap").onclick = function (e) {
          const b = e.target.closest("[data-c]"); if (!b || solved) return;
          const c = b.dataset.c; guessed.add(c);
          if (w.indexOf(c) === -1) { lives--; $("#sp-msg").textContent = "Nie ma tej litery! Zostało " + Math.max(0, lives) + " prób."; }
          else $("#sp-msg").textContent = "Jest! ✅";
          const remains = w.split("").filter(function (x) { return /[a-z]/.test(x) && !guessed.has(x); });
          if (!remains.length) { solved = true; $("#sp-word").textContent = it.word.toUpperCase(); finishWord(true); return; }
          if (lives <= 0) { solved = true; $("#sp-word").textContent = it.word.toUpperCase(); finishWord(false); return; }
          render();
        };
      }
      function finishWord(good) {
        logAnswer(it, "spelling", good, it.word, "Apostrof zastępuje znikniętą literę.");
        tries++; if (good) { ok++; toast("✅ " + it.word); confetti(10); }
        else toast("Poprawnie: " + it.word);
        speak(it.word);
        $$("#sp-wrap .key").forEach(function (k) { k.disabled = true; });
        $("#sp-actions").innerHTML = '<button class="btn" id="sp-next">' + (wi + 1 >= words.length ? "Zobacz wynik 🏁" : "Następne słowo →") + "</button>";
        $("#sp-next").onclick = function () { wi++; draw(); };
      }
      render();
      try { speak(it.word); } catch (e) {}
    }
    draw();
  }

  /* ============ 15. Szybka runda ============ */
  function actSpeed() {
    const pool = deal("speed", D.choice, Math.min(60, D.choice.length));
    let time = 60, score = 0, asked = 0, cur = null;
    function draw() {
      cur = pool[asked % pool.length];
      $("#run-host").innerHTML =
        '<div class="card flat" style="background:var(--coral-l);border-color:#ffd3c4"><div class="between"><b>⚡ Szybka runda</b><span class="chip coral timer" id="sp-timer">⏱ ' + time + " s</span></div>" +
        '<div class="muted">Ile poprawnych form zdążysz w 60 sekund? Błąd nie kończy gry — lecimy dalej!</div></div>' +
        '<div id="speed-wrap"><div class="qbox"><div class="q">' + cur.s.replace("___", "<em>___</em>") + '</div></div>' +
        '<div class="options">' + shuffle(cur.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>" +
        '<div class="feedback" id="speed-fb"></div><p class="score-line" id="speed-score">Wynik: ' + score + " / " + asked + "</p></div>";
      $("#speed-wrap").onclick = function (e) {
        const b = e.target.closest("[data-v]"); if (!b || time <= 0) return;
        asked++;
        const ok = b.dataset.v === cur.a;
        if (ok) score++;
        logAnswer(cur, "speed", ok, cur.a, "");
        mark("speed", ok);
        const fb = $("#speed-fb"); fb.className = "feedback show " + (ok ? "ok" : "no");
        fb.innerHTML = ok ? "✅ Tak!" : "❌ Poprawnie: " + esc(cur.a);
        $("#speed-score").textContent = "Wynik: " + score + " / " + asked;
        setTimeout(draw, ok ? 220 : 650);
      };
    }
    function end() {
      clearSpeed();
      S.speedBest = Math.max(S.speedBest || 0, score);
      const a = S.acts.speed || { tried: 0, ok: 0, done: false }; a.done = true; S.acts.speed = a;
      recordGrade("szybka runda", "speed", score, asked, 60);
      save(); checkBadges();
      $("#run-host").innerHTML = '<div class="center"><div style="font-size:3rem">⚡</div><h2>' + score + " poprawnych w 60 sekund!</h2>" +
        '<div style="font-size:1.15rem;font-weight:900;color:var(--teal-d)">Ocena: ' + gradeBadge(pct(score, asked)) + '</div>' +
        '<p class="muted">Najlepszy wynik: ' + S.speedBest + '. Zobacz, jak rośnie szybkość po kilku dniach powtórek.</p>' +
        '<div class="row" style="justify-content:center"><button class="btn" id="sp-again">🔁 Jeszcze raz</button><button class="btn ghost" id="sp-other">🎮 Inne ćwiczenia</button></div></div>';
      $("#sp-again").onclick = function () { actSpeed(); };
      $("#sp-other").onclick = function () { $("#practice-run").style.display = "none"; $("#practice-hub").style.display = "block"; updateHub(); };
      confetti(20);
    }
    draw();
    clearSpeed();
    activeSpeed = setInterval(function () {
      time--;
      const t = $("#sp-timer"); if (t) t.textContent = "⏱ " + Math.max(0, time) + " s";
      if (time <= 0) end();
    }, 1000);
  }

  /* ============ 16. Miks 5 minut — trening przeplatany (interleaving) ============ */
  function actMix() {
    new Session("mix", MIX_POOL, 12, function (it, s) {
      const rd = reviewData(it, it._act);
      const src = D.activities.find(function (a) { return a.id === it._act; });
      const accepted = Array.isArray(it.a) ? it.a : [rd.correct];
      s.spoken = rd.correct;
      step(s, {
        q: esc(rd.prompt).replace("___", "<em>___</em>"),
        hint: "🧩 " + esc(src ? src.title : it._act) + " · " + esc(it.area || "powtórka") + " — co chwilę inny typ zadania",
        body: rd.options
          ? '<div class="options">' + shuffle(rd.options).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>"
          : '<input class="answer-input" id="ans" autocomplete="off" autocapitalize="off" spellcheck="false" placeholder="wpisz po angielsku…">',
        actions: rd.options ? "" : '<button class="btn" id="btn-check">Sprawdź ✓</button><button class="btn small ghost" id="btn-say">🔊 Przeczytaj</button>'
      });
      if (rd.options) {
        $$("#step-body .opt").forEach(function (b) {
          b.onclick = function () {
            const ok = norm(b.dataset.v) === norm(rd.correct);
            $$("#step-body .opt").forEach(function (x) {
              x.disabled = true;
              if (norm(x.dataset.v) === norm(rd.correct)) x.classList.add("correct");
              else if (x === b) x.classList.add("wrong");
            });
            check(s, ok, rd.correct, it.w || rd.correct);
          };
        });
      } else {
        const inp = $("#ans");
        function submit() {
          const v = norm(inp.value);
          if (!v) { toast("Najpierw coś wpisz 🙂"); inp.focus(); return; }
          const ok = accepted.some(function (x) { return norm(x) === v; });
          inp.disabled = true;
          check(s, ok, rd.correct, it.w || rd.correct);
        }
        $("#btn-check").onclick = submit;
        inp.onkeydown = function (e) { if (e.key === "Enter") submit(); };
        const bs = $("#btn-say"); if (bs) bs.onclick = function () { speak(String(rd.prompt).replace("___", rd.correct)); };
        inp.focus();
      }
    }).show();
  }

  const IMPL = {
    choice: actChoice, gap: actGap, memory: actMemory, match: actMatch, order: actOrder,
    transform: actTransform, shorts: actShorts, detective: actDetective, sort: actSort,
    reading: actReading, listen: actListen, dictation: actDictation, translate: actTranslate,
    spelling: actSpelling, mix: actMix, speed: actSpeed
  };

  /* ---------------- FISZKI (kolejka zapisywana między wizytami) ---------------- */
  function newCardQueue() {
    /* najpierw karty, których dziecko jeszcze nie widziało nigdzie */
    const unseen = [], seen = [];
    D.flashcards.forEach(function (c, i) {
      (S.used["fc|" + norm(c.front)] === undefined ? unseen : seen).push(i);
    });
    return shuffle(unseen).concat(shuffle(seen));
  }
  let cardQueue = (Array.isArray(S.cardQueue) && S.cardQueue.length)
    ? S.cardQueue.filter(function (i) { return Number.isInteger(i) && i >= 0 && i < D.flashcards.length; })
    : newCardQueue();
  let cardFlipped = false;
  function saveCardQueue() { S.cardQueue = cardQueue.slice(); save(); }
  function renderCard() {
    if (!cardQueue.length) cardQueue = newCardQueue();
    const i = cardQueue[0], c = D.flashcards[i];
    S.used["fc|" + norm(c.front)] = ++S.useSeq; save();
    cardFlipped = false;
    const box = $("#flash-card");
    box.innerHTML = '<div class="front">' + esc(c.front) + '</div><span class="pill teal" style="margin-top:10px">' + esc(c.tag) + '</span><div class="flip-hint">👆 Kliknij, żeby zobaczyć odpowiedź</div>';
    const st = S.cards[i] || { box: 0, seen: 0, ok: 0 };
    const known = Object.keys(S.cards).filter(function (k) { return S.cards[k] && S.cards[k].ok > 0; }).length;
    $("#cards-stats").textContent = "Poziom: " + (st.box + 1) + " · powtórzeń: " + st.seen;
    $("#cards-queue").innerHTML = "W kolejce: " + cardQueue.length + " kart · oznaczonych „Umiem”: " + known + " z " + D.flashcards.length + "<br>Aktualna karta: <b>" + esc(c.tag) + "</b>";
  }
  $("#flash-card").onclick = function () {
    const i = cardQueue[0], c = D.flashcards[i];
    cardFlipped = !cardFlipped;
    this.innerHTML = cardFlipped
      ? '<div class="back">' + esc(c.back) + '</div><div class="flip-hint">👆 Kliknij, żeby wrócić do pytania</div>'
      : '<div class="front">' + esc(c.front) + '</div><span class="pill teal" style="margin-top:10px">' + esc(c.tag) + '</span><div class="flip-hint">👆 Kliknij, żeby zobaczyć odpowiedź</div>';
    if (cardFlipped) speak(c.back.replace(/\(.*?\)/g, ""));
  };
  $("#flash-card").onkeydown = function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); this.click(); } };
  function nextCard(known) {
    const i = cardQueue.shift();
    const st = S.cards[i] || { box: 0, seen: 0, ok: 0 };
    st.seen++;
    if (known) { st.ok++; st.box = Math.min(2, st.box + 1); S.xp += 5; }
    else st.box = 0;
    S.cards[i] = st; save(); updateChips();
    if (known) {
      const pos = Math.min(cardQueue.length, 3 + st.box * 4);
      cardQueue.splice(pos, 0, i);
      toast("✅ Umiem! Karta wróci później.");
    } else {
      cardQueue.splice(Math.min(2, cardQueue.length), 0, i);
      toast("🔁 Dobra, powtórzymy to wkrótce.");
    }
    saveCardQueue();
    renderCard();
  }
  $("#btn-card-again").onclick = function () { nextCard(false); };
  $("#btn-card-know").onclick = function () { nextCard(true); };
  $("#btn-shuffle-cards").onclick = function () {
    cardQueue = shuffle(cardQueue.slice()); saveCardQueue(); renderCard(); toast("🔀 Kolejka przetasowana");
  };
  $("#btn-reset-cards").onclick = function () {
    S.cards = {}; cardQueue = newCardQueue(); saveCardQueue(); renderCard(); toast("Fiszki zaczynają od nowa");
  };

  /* ---------------- KARTKÓWKA PRÓBNA ---------------- */
  let EX = null;
  function startExam(mode) {
    clearExamTimer();
    const sel = $("#exam-count");
    const n = Math.min(30, Math.max(5, +(sel ? sel.value : 15) || 15));
    const qs = deal("exam", D.examPool, n);
    EX = { mode: mode, qs: qs, i: 0, answers: [], seconds: 0 };
    $("#exam-intro").style.display = "none";
    $("#exam-result").style.display = "none";
    $("#exam-run").style.display = "block";
    $("#exam-title").textContent = mode === "real" ? "📝 Kartkówka próbna (bez podpowiedzi)" : "🧠 Kartkówka w trybie nauki";
    $("#exam-best").textContent = S.bestExam ? S.bestExam + "%" : "—";
    activeExamTimer = setInterval(function () {
      EX.seconds++;
      const m = Math.floor(EX.seconds / 60), s = EX.seconds % 60;
      const t = $("#exam-timer"); if (t) t.textContent = "⏱ " + String(m).padStart(2, "0") + ":" + String(s).padStart(2, "0");
    }, 1000);
    renderExamQ();
  }
  function renderExamQ() {
    const q = EX.qs[EX.i];
    $("#exam-progress").style.width = pct(EX.i, EX.qs.length) + "%";
    const body = q.type === "choice"
      ? '<div class="options">' + shuffle(q.o).map(function (o) { return '<button class="opt" data-v="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>"
      : '<input class="answer-input" id="exam-input" autocomplete="off" spellcheck="false" placeholder="wpisz odpowiedź po angielsku…">';
    $("#exam-host").innerHTML = '<div id="exam-q-wrap"><div class="exam-q"><div class="num">ZADANIE ' + (EX.i + 1) + " / " + EX.qs.length + ' · ' + esc(q.area) + '</div><div class="p">' + esc(q.p) + "</div>" + body +
      '<div class="feedback" id="exam-fb"></div></div></div>';
    const wrap = $("#exam-q-wrap");
    wrap.onclick = function (e) {
      const b = e.target.closest("[data-v]"); if (!b) return;
      $$("#exam-q-wrap .opt").forEach(function (x) { x.classList.remove("selected"); });
      b.classList.add("selected");
      EX.picked = b.dataset.v;
    };
    const inp = $("#exam-input");
    if (inp) {
      inp.onkeydown = function (e) { if (e.key === "Enter") $("#btn-exam-next").click(); };
      inp.focus();
    }
    delete EX.picked;
  }
  function examAnswer() {
    const q = EX.qs[EX.i];
    let given = q.type === "choice" ? (EX.picked || "") : ($("#exam-input") ? $("#exam-input").value.trim() : "");
    if (!given && EX.skip !== true) { toast("Zaznacz albo wpisz odpowiedź (możesz też kliknąć „Pomiń”)"); return false; }
    if (EX.skip) { given = "(pominięte)"; EX.skip = false; }
    let ok;
    if (q.type === "choice") ok = norm(given) === norm(q.a);
    else ok = q.a.some(function (x) { return norm(x) === norm(given); });
    EX.answers.push({ q: q, given: given, ok: ok });
    logAnswer(q, "exam", ok, Array.isArray(q.a) ? q.a[0] : q.a, q.w);
    mark("exam", ok);
    if (!ok) noteWeak(q.area);
    if (EX.mode === "learn") {
      const fb = $("#exam-fb");
      fb.className = "feedback show " + (ok ? "ok" : "no");
      fb.innerHTML = (ok ? "✅ Brawo! " : "❌ Poprawnie: <u>" + esc(Array.isArray(q.a) ? q.a[0] : q.a) + "</u>. ") + esc(q.w);
      $$("#exam-q-wrap .opt").forEach(function (x) { x.disabled = true; if (norm(x.dataset.v) === norm(q.a)) x.classList.add("correct"); });
      const inp = $("#exam-input"); if (inp) inp.disabled = true;
      $("#btn-exam-next").textContent = EX.i + 1 >= EX.qs.length ? "Zobacz wynik 🏁" : "Dalej →";
      EX.waiting = true;
      return true;
    }
    return true;
  }
  $("#btn-exam-next").onclick = function () {
    if (!EX) return;
    if (EX.waiting) { EX.waiting = false; EX.i++; nextExam(); return; }
    if (examAnswer()) {
      if (EX.waiting) return; // tryb nauki: najpierw pokazujemy wyjaśnienie
      EX.i++; nextExam();
    }
  };
  $("#btn-exam-skip").onclick = function () {
    if (!EX) return;
    if (EX.waiting) { EX.waiting = false; EX.i++; nextExam(); return; }
    EX.skip = true;
    if (examAnswer()) {
      if (EX.waiting) return;
      EX.i++; nextExam();
    }
  };
  function nextExam() {
    $("#btn-exam-next").textContent = "Dalej →";
    if (EX.i >= EX.qs.length) finishExam(); else renderExamQ();
  }
  $("#btn-exam-learn").onclick = function () { startExam("learn"); };
  $("#btn-exam-real").onclick = function () { startExam("real"); };
  $("#btn-exam-abort").onclick = function () {
    clearExamTimer(); EX = null;
    $("#exam-run").style.display = "none"; $("#exam-intro").style.display = "block";
    toast("Przerwano — wróć, gdy będziesz gotowy/a 🙂");
  };
  function finishExam() {
    clearExamTimer();
    const ok = EX.answers.filter(function (x) { return x.ok; }).length;
    const total = EX.qs.length, p = pct(ok, total);
    S.bestExam = Math.max(S.bestExam || 0, p);
    recordGrade("kartkówka", "exam", ok, total, EX.seconds);
    const act = S.acts.exam || { tried: 0, ok: 0, done: false }; act.done = true; S.acts.exam = act;
    save(); updateChips(); updateHub();
    $("#exam-run").style.display = "none";
    $("#exam-result").style.display = "block";
    const weak = {};
    EX.answers.forEach(function (a) { if (!a.ok) weak[a.q.area] = (weak[a.q.area] || 0) + 1; });
    const weakList = Object.keys(weak).sort(function (a, b) { return weak[b] - weak[a]; });
    $("#exam-result").innerHTML =
      '<div class="card hero center"><div style="font-size:3rem">' + (p >= 90 ? "🏆" : p >= 70 ? "👍" : "🌱") + '</div><h1>' + ok + " / " + total + " · " + p + "%</h1>" +
      '<div style="font-size:1.5rem;font-weight:900">🎓 Ocena: ' + gradeBadge(p) + '</div>' +
      "<p>" + (p >= 90 ? "Jesteś gotowy/a na kartkówkę! 💪" : p >= 70 ? "Dobrze idzie! Popraw jeszcze poniższe zadania." : "Spokojnie — to była próba. Wróć do ćwiczeń i spróbuj ponownie.") + "</p>" +
      '<div class="muted" style="color:rgba(255,255,255,.9)">🎲 Zestaw nr ' + (S.cycles.exam || 1) + " z " + D.examPool.length.toLocaleString("pl-PL") + " zadań · następny będzie inny</div>" +
      '<div class="muted" style="color:rgba(255,255,255,.9)">Czas: ' + Math.floor(EX.seconds / 60) + " min " + (EX.seconds % 60) + " s · tryb: " + (EX.mode === "real" ? "kartkówka" : "nauka") + "</div></div>" +
      '<div class="card"><div class="between"><h2>🔍 Omówienie wszystkich zadań</h2><span class="pill teal">Najlepszy wynik: ' + S.bestExam + "%</span></div>" +
      (weakList.length ? '<p class="muted">Nad tym warto popracować: <b>' + weakList.map(function (w) { return w + " (" + weak[w] + ")"; }).join(", ") + "</b></p>" : "<p class='muted'>Zero błędów — pięknie! 🎉</p>") +
      EX.answers.map(function (a, i) {
        return '<div class="exam-q review ' + (a.ok ? "ok" : "no") + '"><div class="num">' + (i + 1) + ". " + esc(a.q.area) + (a.ok ? " ✓" : " ✗") + '</div><div class="p">' + esc(a.q.p) + "</div>" +
          '<div class="muted">Twoja odpowiedź: <b>' + esc(a.given) + "</b></div>" +
          (a.ok ? "" : '<div>Poprawnie: <b>' + esc(Array.isArray(a.q.a) ? a.q.a[0] : a.q.a) + "</b></div>") +
          '<div class="review-note">💡 ' + esc(a.q.w) + "</div></div>";
      }).join("") +
      '<div class="row" style="margin-top:14px"><button class="btn" id="ex-again">🔁 Nowa kartkówka</button><button class="btn orange" id="ex-practice">🎮 Poćwicz słabe strony</button><button class="btn yellow" id="ex-review">🔁 Powtórka z błędów</button><button class="btn ghost" id="ex-back">← Wróć</button></div></div>';
    $("#ex-again").onclick = function () { startExam(EX.mode); };
    $("#ex-practice").onclick = function () { go("practice"); };
    $("#ex-review").onclick = function () { go("review"); };
    $("#ex-back").onclick = function () { $("#exam-result").style.display = "none"; $("#exam-intro").style.display = "block"; };
    if (p >= 80) confetti(40);
    checkBadges();
  }

  /* ---------------- POWTÓRKI: zakładka „Powtórki" ---------------- */
  let RV = null;
  function renderReviewStats() {
    const all = allMistakes();
    const due = dueReviews().length;
    const mastered = all.filter(function (r) { return r.mastered; }).length;
    const wk = weakestSkill();
    $("#review-stats").innerHTML = [
      ["🔁", String(due), "zadań do powtórki dziś"],
      ["🛠️", String(all.length), "trudnych zadań w bazie"],
      ["✅", String(mastered), "już opanowanych"],
      ["🎯", wk ? wk.p + "%" : "—", wk ? "najsłabszy: " + wk.k : "za mało danych"]
    ].map(function (x) {
      return '<div class="card flat center" style="margin:0"><div style="font-size:1.7rem">' + x[0] + '</div><b style="font-size:1.15rem">' + esc(x[1]) + '</b><small class="muted">' + esc(x[2]) + "</small></div>";
    }).join("");
    const list = all.slice().sort(function (a, b) { return (b.wrong - b.right) - (a.wrong - a.right); }).slice(0, 40);
    $("#review-list").innerHTML = list.length
      ? '<div class="table-wrap"><table><thead><tr><th>Zadanie</th><th>Temat</th><th>Pomyłki</th><th>Pudełko</th><th>Następna powtórka</th></tr></thead><tbody>' +
        list.map(function (r) {
          return '<tr><td>' + esc(String(r.prompt).slice(0, 70)) + '</td><td>' + esc(r.area) + '</td><td>' + r.wrong + '</td><td>' +
            (r.mastered ? '<span class="good">opanowane ✓</span>' : r.box + " / 6") + '</td><td>' + (r.mastered ? "—" : esc(r.due)) + "</td></tr>";
        }).join("") + "</tbody></table></div>"
      : "<p class='muted'>Nie ma jeszcze żadnych błędów — brawo! Gdy coś pójdzie nie tak, trafi tutaj i wróci w kolejnych dniach.</p>";
  }
  function renderReview() {
    renderReviewStats();
    renderLights();
    RV = { list: dueReviews(), i: 0, ok: 0, total: 0 };
    RV.total = RV.list.length;
    drawReview();
  }
  function finishReview() {
    const host = $("#review-host");
    if (!RV.total) {
      host.innerHTML = '<div class="center" style="padding:14px 0"><div style="font-size:3rem">🌟</div><h2>Wszystko opanowane!</h2>' +
        '<p class="muted">Nie ma dziś żadnych zadań do powtórki. Zrób nowy zestaw ćwiczeń, a błędy automatycznie trafią tutaj.</p>' +
        '<button class="btn" id="rv-practice">🎮 Idę poćwiczyć</button></div>';
      $("#rv-practice").onclick = function () { go("practice"); };
      return;
    }
    const p = pct(RV.ok, RV.total);
    recordGrade("powtórka", "review", RV.ok, RV.total);
    host.innerHTML = '<div class="center" style="padding:14px 0"><div style="font-size:3rem">' +
      (p >= 80 ? "🏆" : p >= 50 ? "👍" : "🌱") + '</div><h2>' + RV.ok + " / " + RV.total + " · " + p + '%</h2>' +
      '<div style="font-weight:900;color:var(--teal-d)">Ocena: ' + gradeBadge(p) + '</div>' +
      '<p class="muted">Zadania, które umiesz, wrócą za 1, 2, 4, 7, 14 lub 30 dni. Te z błędem — jutro.</p>' +
      '<div class="row" style="justify-content:center"><button class="btn" id="rv-reload">🔁 Jeszcze raz</button><button class="btn ghost" id="rv-practice2">🎮 Ćwiczenia</button></div></div>';
    $("#rv-reload").onclick = renderReview;
    $("#rv-practice2").onclick = function () { go("practice"); };
    if (p >= 80) confetti(25);
    checkBadges(); renderReviewStats();
  }
  function drawReview() {
    const host = $("#review-host");
    if (!RV || !RV.list.length || RV.i >= RV.list.length) { finishReview(); return; }
    const r = RV.list[RV.i];
    host.innerHTML =
      '<div class="qbox"><div class="q">' + esc(r.prompt) + '</div><div class="hint">🔁 Powtórka ' + (RV.i + 1) + " / " + RV.total +
      " · " + esc(r.area) + " · pomyłek: " + r.wrong + " · pudełko " + r.box + "/6</div></div>" +
      (r.options
        ? '<div class="options">' + shuffle(r.options).map(function (o) { return '<button class="opt" data-rv="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>"
        : '<input class="answer-input" id="rv-ans" autocomplete="off" spellcheck="false" placeholder="wpisz odpowiedź po angielsku…">') +
      '<div class="feedback" id="rv-fb"></div>' +
      '<div class="ex-actions"><button class="btn" id="rv-check">Sprawdź ✓</button><button class="btn small ghost" id="rv-show">👀 Pokaż odpowiedź</button><button class="btn small ghost" id="rv-skip">⏭ Pomiń</button></div>';
    let answered = false;
    function judge(ok) {
      if (answered) return;
      answered = true;
      if (ok) RV.ok++;
      const rec = S.mistakes[r.id];
      const st = S.skills[r.area] || (S.skills[r.area] = { tried: 0, ok: 0 });
      st.tried++; if (ok) st.ok++;
      if (!ok) S.weak[r.area] = (S.weak[r.area] || 0) + 1;
      if (rec) {
        if (ok) { rec.right++; rec.box = Math.min(6, rec.box + 1); rec.due = addDays(BOX_DAYS[rec.box] || 30); if (rec.box >= 6) rec.mastered = true; }
        else { rec.wrong++; rec.box = 0; rec.due = today(); rec.mastered = false; }
      }
      if (ok) S.xp += 10;
      save(); updateChips(); renderReviewStats();
      const fb = $("#rv-fb");
      fb.className = "feedback show " + (ok ? "ok" : "no");
      fb.innerHTML = (ok ? "✅ Brawo! " : "❌ Poprawnie: <u>" + esc(r.correct) + "</u>. ") + esc(r.why || "");
      $$("#review-host .opt").forEach(function (b) {
        b.disabled = true;
        if (norm(b.dataset.rv) === norm(r.correct)) b.classList.add("correct");
      });
      const inp = $("#rv-ans"); if (inp) inp.disabled = true;
      $("#rv-check").style.display = "none";
      $("#rv-show").style.display = "none";
      $("#rv-skip").textContent = RV.i + 1 >= RV.total ? "Zobacz wynik 🏁" : "Dalej →";
      $("#rv-skip").onclick = function () { RV.i++; drawReview(); };
    }
    $$("#review-host .opt").forEach(function (b) {
      b.onclick = function () { judge(norm(b.dataset.rv) === norm(r.correct)); };
    });
    $("#rv-check").onclick = function () {
      const inp = $("#rv-ans"); if (!inp) return;
      const v = norm(inp.value);
      if (!v) { toast("Najpierw coś wpisz 🙂"); return; }
      judge(v === norm(r.correct));
    };
    const inp0 = $("#rv-ans"); if (inp0) inp0.onkeydown = function (e) { if (e.key === "Enter") $("#rv-check").click(); };
    $("#rv-show").onclick = function () { judge(false); };
    $("#rv-skip").onclick = function () { RV.i++; drawReview(); };
  }
  const brw = $("#btn-review-weak");
  if (brw) brw.onclick = function () {
    const w = weakestSkill();
    if (!w) { toast("Za mało danych — zrób najpierw kilka zadań 🙂"); go("practice"); return; }
    openActivity(SKILL_ACT[w.k] || "choice");
    toast("🎯 Najsłabszy temat: " + w.k + " (" + w.p + "%)");
  };

  /* ---------------- LABORATORIUM: maszyna zdań, własne zdania, „Znajdź kogoś, kto…" ---------------- */
  let MCH = { subj: 0, mode: "+", end: 0, challenge: false };
  let FSW = { person: 0, q: 0, picked: null };
  const MCH_MODES = [
    { id: "+", ico: "➕", label: "Twierdzenie" },
    { id: "-", ico: "➖", label: "Przeczenie" },
    { id: "?", ico: "❓", label: "Pytanie" },
    { id: "ans", ico: "💬", label: "Krótka odpowiedź" }
  ];
  function cap(s) { return String(s).charAt(0).toUpperCase() + String(s).slice(1); }
  function machineBuild() {
    const subj = D.machine.subjects[MCH.subj];
    const be = D.be[subj.bi];
    const end = D.machine.endings[MCH.end];
    const stem = end.replace(/\.$/, "");
    const word = subj.word, pron = word === be.en;
    if (MCH.mode === "+") return {
      main: word + " " + be.form + " " + end,
      alt: pron ? be.short + " " + end : (be.form === "is" ? word + "'s " + end : null),
      note: "Osoba + be + reszta. „" + word + "\" → " + be.form + ".",
      gap: word + " ___ " + end, answer: be.form, options: ["am", "is", "are"], tip: "Która forma be?"
    };
    if (MCH.mode === "-") {
      const neg = be.form + " not";
      const shortWord = pron ? be.negShort : word + " " + (be.form === "is" ? "isn't" : "aren't");
      return { main: word + " " + neg + " " + end, alt: shortWord + " " + end,
        note: "Przeczenie = be + not. Uwaga: „amn't\" NIE istnieje!",
        gap: word + " ___ " + end, answer: neg,
        options: ["am not", "is not", "are not", "isn't", "aren't", "I'm not"], tip: 'Jak powiedzieć „nie jest"?' };
    }
    if (MCH.mode === "?") return {
      main: cap(be.form) + " " + word + " " + stem + "?", alt: null,
      note: "be wędruje na początek. Nie dodajemy do / does.",
      gap: "___ " + word + " " + stem + "?", answer: be.form,
      options: ["am", "is", "are", "do"], tip: "Co stoi na początku pytania?"
    };
    return {
      main: cap(be.form) + " " + word + " " + stem + "? → " + be.yes,
      alt: cap(be.form) + " " + word + " " + stem + "? → " + be.no,
      note: "W odpowiedzi TAK nie skracamy: Yes, I am. (nie: Yes, I'm.)",
      gap: null, answer: be.yes, options: [], tip: "Krótkie odpowiedzi"
    };
  }
  function renderMachine() {
    const host = $("#machine-host"); if (!host) return;
    const b = machineBuild();
    const subjBtns = D.machine.subjects.map(function (x, i) {
      return '<button class="m-chip' + (i === MCH.subj ? " on" : "") + '" data-kind="subj" data-i="' + i + '">' + esc(x.word) + "</button>";
    }).join("");
    const modeBtns = MCH_MODES.map(function (m) {
      return '<button class="m-chip' + (m.id === MCH.mode ? " on" : "") + '" data-kind="mode" data-mode="' + m.id + '">' + m.ico + " " + esc(m.label) + "</button>";
    }).join("");
    const endBtns = D.machine.endings.map(function (x, i) {
      return '<button class="m-chip small' + (i === MCH.end ? " on" : "") + '" data-kind="end" data-i="' + i + '">' + esc(x) + "</button>";
    }).join("");
    const out = MCH.challenge && b.options.length
      ? '<div class="machine-out challenge"><div class="muted">' + esc(b.tip) + "</div><div class=\"q\">" +
        esc(b.gap).replace("___", "<em>___</em>") + "</div>" +
        '<div class="options">' + shuffle([b.answer].concat(shuffle(b.options.filter(function (o) { return o !== b.answer; })).slice(0, 3)))
          .map(function (o) { return '<button class="opt" data-mans="' + esc(o) + '">' + esc(o) + "</button>"; }).join("") + "</div>" +
        '<div class="feedback" id="mch-fb"></div></div>'
      : '<div class="machine-out"><div class="muted">Gotowe zdanie 🔽</div><div class="q">' + esc(b.main) + "</div>" +
        (b.alt ? '<div class="alt">Krótko / inaczej: <b>' + esc(b.alt) + "</b></div>" : "") +
        '<div class="tip-line">💡 ' + esc(b.note) + "</div></div>";
    host.innerHTML =
      '<div class="machine-grid"><div><h3>1️⃣ Kto?</h3><div class="chip-list">' + subjBtns + "</div></div>" +
      '<div><h3>2️⃣ Co robimy?</h3><div class="chip-list">' + modeBtns + "</div></div>" +
      '<div><h3>3️⃣ Reszta zdania</h3><div class="chip-list">' + endBtns + "</div></div></div>" +
      out +
      '<div class="row" style="margin-top:12px">' +
      (b.options.length ? '<button class="btn small" id="mch-challenge">' + (MCH.challenge ? "👀 Pokaż zdanie" : "🎯 Sprawdź się") + "</button>" : "") +
      '<button class="btn small ghost" id="mch-say">🔊 Posłuchaj</button>' +
      '<button class="btn small ghost" id="mch-save">➕ Zapisz do moich zdań</button>' +
      '<button class="btn small ghost" id="mch-random">🎲 Losuj</button></div>';
    $$("#machine-host [data-kind]").forEach(function (btn) {
      btn.onclick = function () {
        if (btn.dataset.kind === "subj") MCH.subj = +btn.dataset.i;
        else if (btn.dataset.kind === "mode") MCH.mode = btn.dataset.mode;
        else MCH.end = +btn.dataset.i;
        MCH.challenge = false; renderMachine();
      };
    });
    const cc = $("#mch-challenge"); if (cc) cc.onclick = function () { MCH.challenge = !MCH.challenge; renderMachine(); };
    $("#mch-say").onclick = function () { speak(b.main); };
    $("#mch-save").onclick = function () {
      if (S.mySentences.indexOf(b.main) === -1) {
        S.mySentences.unshift(b.main);
        if (S.mySentences.length > 12) S.mySentences.length = 12;
        S.xp += 2; save(); updateChips(); renderMySentences();
        toast("✍️ Zapisano zdanie +2 pkt");
      } else toast("To zdanie już masz na liście 🙂");
    };
    $("#mch-random").onclick = function () {
      MCH.subj = Math.floor(Math.random() * D.machine.subjects.length);
      MCH.mode = MCH_MODES[Math.floor(Math.random() * MCH_MODES.length)].id;
      MCH.end = Math.floor(Math.random() * D.machine.endings.length);
      MCH.challenge = false; renderMachine();
    };
    $$("#machine-host [data-mans]").forEach(function (btn) {
      btn.onclick = function () {
        const ok = norm(btn.dataset.mans) === norm(b.answer);
        $$("#machine-host [data-mans]").forEach(function (x) {
          x.disabled = true;
          if (norm(x.dataset.mans) === norm(b.answer)) x.classList.add("correct");
          else if (x === btn) x.classList.add("wrong");
        });
        if (ok) { S.xp += 5; S.correct++; } else S.wrong++;
        save(); updateChips();
        const fb = $("#mch-fb");
        fb.className = "feedback show " + (ok ? "ok" : "no");
        fb.innerHTML = (ok ? "✅ Brawo! " : "❌ Poprawnie: <u>" + esc(b.answer) + "</u>. ") + esc(b.note);
      };
    });
  }
  function renderMySentences() {
    const host = $("#machine-sentences"); if (!host) return;
    host.innerHTML = S.mySentences.length
      ? '<div class="my-sentences">' + S.mySentences.map(function (s, i) {
          return '<div class="my-sentence"><span>' + esc(s) + '</span><button class="btn small ghost" data-del="' + i + '">✕</button><button class="btn small ghost" data-say="' + esc(s) + '">🔊</button></div>';
        }).join("") + "</div>"
      : "<p class='muted'>Jeszcze nie zapisałeś/aś żadnego zdania. Złóż je w maszynie powyżej i kliknij „➕ Zapisz\".</p>";
    $$("#machine-sentences [data-del]").forEach(function (b) {
      b.onclick = function () { S.mySentences.splice(+b.dataset.del, 1); save(); renderMySentences(); };
    });
    $$("#machine-sentences [data-say]").forEach(function (b) {
      b.onclick = function () { speak(b.dataset.say); };
    });
  }
  function fswTruth(p, q) {
    switch (q.test) {
      case "age11": return p.age === 11;
      case "age10": return p.age === 10;
      case "poland": return p.from === "Poland";
      case "brazil": return p.from === "Brazil";
      case "robot": return p.job === "a robot";
      case "teacher": return p.job === "a teacher";
      case "student": return p.job === "a student";
    }
    return false;
  }
  function renderFind() {
    const host = $("#find-host"); if (!host) return;
    const p = D.machine.people[FSW.person], q = D.machine.questions[FSW.q];
    const yes = fswTruth(p, q), correct = yes ? "Yes, I am." : "No, I'm not.";
    const why = p.name + (q.test === "age11" || q.test === "age10" ? " ma " + p.age + " lat" :
      q.test === "poland" || q.test === "brazil" ? " jest z " + p.from : " jest " + p.job) +
      ", więc " + (yes ? "tak" : "nie") + ".";
    const opts = shuffle(["Yes, I am.", "No, I'm not.", "Yes, I'm.", "No, I amn't."]);
    host.innerHTML =
      '<div class="find-people">' + D.machine.people.map(function (x, i) {
        return '<button class="person-chip' + (i === FSW.person ? " on" : "") + '" data-person="' + i + '">' + esc(x.name) + "</button>";
      }).join("") + "</div>" +
      '<div class="find-card"><div class="who">👤 ' + esc(p.name) + "</div>" +
      '<div class="muted">Wiek: <b>' + p.age + "</b> · Skąd: <b>" + esc(p.from) + "</b> · Kim jest: <b>" + esc(p.job) + "</b></div></div>" +
      '<div class="find-qs">' + D.machine.questions.map(function (x, i) {
        return '<button class="q-chip' + (i === FSW.q ? " on" : "") + '" data-q="' + i + '">' + esc(x.q) + "</button>";
      }).join("") + "</div>" +
      '<div class="bubble"><div class="muted">Pytasz „' + esc(q.q) + "\"</div><div class=\"q\">" + esc(correct) + "</div></div>" +
      '<div class="options" id="fsw-options">' + opts.map(function (o) {
        return '<button class="opt" data-fans="' + esc(o) + '">' + esc(o) + "</button>";
      }).join("") + "</div>" +
      '<div class="feedback" id="fsw-fb"></div>' +
      '<div class="row"><button class="btn small ghost" id="fsw-say">🔊 Posłuchaj pytania i odpowiedzi</button>' +
      '<button class="btn small ghost" id="fsw-random">🎲 Losuj rozmówcę i pytanie</button></div>' +
      '<p class="muted" style="margin-top:10px">Dane postaci są zmyślone na potrzeby zabawy — ćwiczymy tylko krótkie odpowiedzi. ' +
      "To samo ćwiczenie, które na lekcji nazywa się „Find Someone Who\".</p>";
    $$("#find-host [data-person]").forEach(function (b) {
      b.onclick = function () { FSW.person = +b.dataset.person; FSW.picked = null; renderFind(); };
    });
    $$("#find-host [data-q]").forEach(function (b) {
      b.onclick = function () { FSW.q = +b.dataset.q; FSW.picked = null; renderFind(); };
    });
    $$("#find-host [data-fans]").forEach(function (btn) {
      btn.onclick = function () {
        const ok = norm(btn.dataset.fans) === norm(correct);
        $$("#find-host [data-fans]").forEach(function (x) {
          x.disabled = true;
          if (norm(x.dataset.fans) === norm(correct)) x.classList.add("correct");
          else if (x === btn) x.classList.add("wrong");
        });
        if (ok) { S.xp += 5; S.correct++; } else S.wrong++;
        save(); updateChips();
        const fb = $("#fsw-fb");
        fb.className = "feedback show " + (ok ? "ok" : "no");
        fb.innerHTML = (ok ? "✅ Brawo! " : "❌ Poprawnie: <u>" + esc(correct) + "</u>. ") + esc(why) +
          "<br><b>W odpowiedzi TAK nie skracamy:</b> Yes, I am. (nie Yes, I'm.).";
      };
    });
    $("#fsw-say").onclick = function () { speak(q.q + " " + correct); };
    $("#fsw-random").onclick = function () {
      FSW.person = Math.floor(Math.random() * D.machine.people.length);
      FSW.q = Math.floor(Math.random() * D.machine.questions.length);
      FSW.picked = null; renderFind();
    };
  }
  function renderLights() {
    const host = $("#lights-grid"); if (!host) return;
    host.innerHTML = LIGHT_SKILLS.map(function (x) {
      const cur = S.lights[x.k];
      const st = S.skills[x.k];
      return '<div class="light-row"><div class="light-ican"><b>' + esc(x.ican) + "</b>" +
        (st && st.tried ? '<small class="muted"> · z zadań: ' + pct(st.ok, st.tried) + "%</small>" : "") + "</div>" +
        '<div class="light-btns">' + LIGHTS.map(function (l) {
          return '<button class="light-btn' + (cur === l.id ? " on" : "") + '" data-skill="' + esc(x.k) + '" data-light="' + l.id + '">' + l.ico + " " + esc(l.label) + "</button>";
        }).join("") + "</div></div>";
    }).join("");
    $$("#lights-grid .light-btn").forEach(function (b) {
      b.onclick = function () {
        S.lights[b.dataset.skill] = b.dataset.light;
        save(); renderLights(); renderReviewStats();
        toast("🚦 Zapisano samoocenę: " + b.dataset.skill);
      };
    });
    const sum = $("#lights-summary");
    if (sum) {
      const g = Object.keys(S.lights).filter(function (k) { return S.lights[k] === "green"; }).length;
      const a = Object.keys(S.lights).filter(function (k) { return S.lights[k] === "amber"; }).length;
      const r = Object.keys(S.lights).filter(function (k) { return S.lights[k] === "red"; }).length;
      sum.innerHTML = "🟢 " + g + " umiem · 🟡 " + a + " prawie · 🔴 " + r + " do powtórki" +
        (r ? ' — przycisk „🎯 Ćwicz najsłabszy temat" wybierze właśnie te.' : " — na razie nic na czerwono, brawo!");
    }
  }
  function exitTicketHtml(act) {
    const sk = skillOf(act);
    return '<hr><div class="exit-ticket"><b>🎫 Bilet wyjścia · jak poszło z „' + esc(sk) + '"?</b>' +
      '<div class="row" style="justify-content:center;margin-top:8px">' + LIGHTS.map(function (l) {
        return '<button class="light-btn' + (S.lights[sk] === l.id ? " on" : "") + '" data-exit="' + l.id + '">' + l.ico + " " + esc(l.label) + "</button>";
      }).join("") + "</div>" +
      '<small class="muted">Samoocena od razu po zadaniu jest najtrafniejsza i pomaga dobrać powtórki.</small></div>';
  }
  function renderLab() {
    renderMachine(); renderMySentences(); renderFind();
  }

  /* ---------------- POSTĘPY ---------------- */
  function renderProgress() {
    const total = S.correct + S.wrong;
    const done = D.activities.filter(function (a) { return S.acts[a.id] && S.acts[a.id].done; }).length;
    $("#stats-grid").innerHTML = [
      ["⭐", S.xp + " pkt", "zdobyte punkty"], ["✅", S.correct + " / " + total, "poprawnych odpowiedzi"],
      ["📈", pct(S.correct, total) + "%", "skuteczność"], ["🔥", S.bestStreak, "najdłuższa seria"],
      ["📝", (S.bestExam || 0) + "%", "najlepsza próbna kartkówka"], ["🎮", done + " / " + D.activities.length, "ukończonych ćwiczeń"],
      ["📅", S.days.length, "dni nauki"], ["🏅", S.badges.length + " / " + BADGES.length, "odznak"]
    ].map(function (x) {
      return '<div class="card flat center" style="margin:0"><div style="font-size:1.7rem">' + x[0] + "</div><b style='font-size:1.15rem'>" + x[1] + '</b><small class="muted">' + x[2] + "</small></div>";
    }).join("");

    $("#activity-bars").innerHTML = D.activities.map(function (a) {
      const st = S.acts[a.id] || { tried: 0, ok: 0 };
      const p = pct(st.ok, st.tried);
      return '<div><div class="between"><b>' + a.icon + " " + a.title + '</b><span class="muted">' + st.ok + " / " + st.tried + (st.tried ? " · " + p + "%" : " · nie zaczęte") + "</span></div>" +
        '<div class="bar"><i style="width:' + (st.tried ? p : 0) + '%"></i></div></div>';
    }).join("");

    /* --- Dzienniczek ocen + wykres --- */
    const gs = S.grades.slice(0, 12).reverse();
    const avg = S.grades.length ? Math.round(S.grades.reduce(function (s, g) { return s + g.p; }, 0) / S.grades.length) : 0;
    function spark(vals) {
      const w = 280, h = 70;
      if (!vals.length) return '<div class="muted">Zapisz pierwszy wynik, a pojawi się wykres 📈</div>';
      const pts = vals.map(function (v, i) {
        const x = vals.length === 1 ? w / 2 : i * w / (vals.length - 1);
        return x.toFixed(1) + "," + (h - (v / 100) * h).toFixed(1);
      }).join(" ");
      return '<svg viewBox="0 0 ' + w + " " + h + '" width="100%" height="70" preserveAspectRatio="none" aria-label="Wykres ostatnich wyników">' +
        '<line x1="0" y1="' + h + '" x2="' + w + '" y2="' + h + '" stroke="var(--line)" stroke-width="2"/>' +
        '<polyline points="' + pts + '" fill="none" stroke="var(--teal)" stroke-width="3" stroke-linejoin="round"/></svg>';
    }
    const gb = $("#gradebook");
    if (gb) gb.innerHTML =
      '<div class="grid2" style="align-items:start"><div>' +
      '<div class="qbox" style="background:var(--teal-l);border-color:#b7e9e3"><b>Średnia z ' + S.grades.length + " prac: " + avg + "%</b> · " + gradeBadge(avg) +
      '<div class="muted">Ocena pojawia się po każdym ćwiczeniu, szybkiej rundzie, powtórce i próbnej kartkówce.</div></div>' +
      "<h3 style='margin-top:14px'>Ostatnie wyniki</h3>" + spark(gs.map(function (g) { return g.p; })) +
      '</div><div><div class="table-wrap"><table><thead><tr><th>Data</th><th>Co</th><th>Wynik</th><th>Ocena</th></tr></thead><tbody>' +
      (S.grades.length ? S.grades.slice(0, 10).map(function (g) {
        const a = D.activities.find(function (x) { return x.id === g.act; });
        const label = g.kind === "kartkówka" ? "📝 Kartkówka" : g.kind === "powtórka" ? "🔁 Powtórka" : (a ? a.icon + " " + a.title : g.kind);
        return "<tr><td>" + esc(g.d) + "</td><td>" + esc(label) + "</td><td>" + g.ok + "/" + g.total + " · " + g.p + "%</td><td><b>" + gradeBadge(g.p).split(" · ")[0] + "</b></td></tr>";
      }).join("") : '<tr><td colspan="4" class="muted">Brak wyników — rozwiąż dowolne ćwiczenie.</td></tr>') +
      "</tbody></table></div></div></div>";
    /* --- Oceny cząstkowe według umiejętności --- */
    const sk = Object.keys(S.skills).map(function (k) {
      return { k: k, tried: S.skills[k].tried, p: pct(S.skills[k].ok, S.skills[k].tried) };
    }).sort(function (a, b) { return a.p - b.p; });
    const sg = $("#skill-grid");
    if (sg) sg.innerHTML = sk.length ? sk.map(function (x) {
      return '<div><div class="between"><b>' + esc(x.k) + '</b><span class="muted">' + x.p + "% · ocena " + gradeOf(x.p).g + " · " + x.tried + " zadań</span></div>" +
        '<div class="bar"><i style="width:' + x.p + '%"></i></div></div>';
    }).join("") : "<p class='muted'>Brak danych — zacznij od dowolnego ćwiczenia.</p>";

    const ranked = D.activities.map(function (a) {
      const st = S.acts[a.id] || { tried: 0, ok: 0 };
      return { name: a.title, p: st.tried >= 3 ? pct(st.ok, st.tried) : null };
    }).filter(function (x) { return x.p !== null; }).sort(function (a, b) { return a.p - b.p; });
    const weak = Object.keys(S.weak).sort(function (a, b) { return S.weak[b] - S.weak[a]; }).slice(0, 4);
    const best = ranked.length ? ranked[ranked.length - 1] : null, worst = ranked.length ? ranked[0] : null;
    $("#parent-report").innerHTML =
      "<p><b>Podsumowanie (" + today() + "):</b> dziecko rozwiązało <b>" + total + " zadań</b>, poprawnie <b>" + S.correct + "</b> (" + pct(S.correct, total) + "%), zdobyło " + S.xp + " pkt i ukończyło " + done + " z " + D.activities.length + " ćwiczeń. Najlepszy wynik próbnej kartkówki: <b>" + (S.bestExam || 0) + "%</b>.</p>" +
      (best ? "<p><b>Najlepiej idzie:</b> " + esc(best.name) + " (" + best.p + "%). <b>Warto poćwiczyć:</b> " + esc(worst.name) + " (" + worst.p + "%).</p>" : "<p>Za mało danych, by ocenić mocne i słabe strony — zacznij od dowolnego ćwiczenia.</p>") +
      (weak.length ? "<p><b>Tematy, w których pojawiły się błędy na kartkówkach:</b> " + weak.map(function (w) { return w + " (" + S.weak[w] + ")"; }).join(", ") + ".</p>" : "") +
      "<p><b>System oceniania:</b> średnia z " + S.grades.length + " prac to <b>" + avg + "%</b> (" + gradeBadge(avg) + "). Baza ma " + globalTotal().toLocaleString("pl-PL") + " różnych treści; dziecko zobaczyło już <b>" + seenTotal() + "</b>, pozostało <b>" + Math.max(0, globalTotal() - seenTotal()) + "</b> całkiem nowych.</p>" +
      '<p><b>Brak powtórek:</b> jeden wspólny rejestr obejmuje wszystkie ćwiczenia, próbną kartkówkę, Miks, szybką rundę, fiszki, pytanie dnia i sprawdziany do druku — to samo zdanie nie wróci w innym ćwiczeniu, dopóki nie wyczerpie się pula ' + globalTotal().toLocaleString("pl-PL") + ' zadań. Wyjątek: zakładka „Powtórki" celowo wraca do błędów.</p>' +
      "<p><b>Powtórki z błędów:</b> " + allMistakes().length + " trudnych zadań w kolejce, " + allMistakes().filter(function (r) { return r.mastered; }).length + " opanowanych, " + dueReviews().length + " do powtórki dziś" +
      (weakestSkill() ? "; najsłabszy temat: <b>" + esc(weakestSkill().k) + "</b> (" + weakestSkill().p + "%" + (weakestSkill().self ? ", wskazany w samoocenie" : "") + ")." : ".") + "</p>" +
      "<p><b>Samoocena i własna produkcja:</b> " + Object.keys(S.lights).length + " z " + LIGHT_SKILLS.length + " umiejętności ocenionych światełkami (" +
      ["green", "amber", "red"].map(function (l) { return LIGHTS.filter(function (x) { return x.id === l; })[0].ico + " " + Object.keys(S.lights).filter(function (k) { return S.lights[k] === l; }).length; }).join(" · ") +
      "), " + S.exitTickets.length + " biletów wyjścia, " + S.mySentences.length + " własnych zdań złożonych w <b>Laboratorium</b>.</p>" +
      '<p><b>Trening przeplatany:</b> ćwiczenie „🧩 Miks 5 minut" łączy 5 różnych typów zadań w jednym zestawie ' +
      '(interleaving), a „Powtórki" rozkładają błędy w czasie — oba mechanizmy mają potwierdzenie w badaniach nad pamięcią.</p>' +
      "<p><b>Rekomendacja:</b> 10–15 minut dziennie według planu z zakładki Start. Dzień przed kartkówką: tabela <i>to be</i>, fiszki z błędami i jedna próbna kartkówka bez podpowiedzi. Zwróć uwagę na trzy pułapki: <i>amn't</i>, <i>Yes, I'm</i> oraz wiek z <i>have</i>.</p>" +
      "<p class='muted'>Dane pochodzą wyłącznie z tej przeglądarki (localStorage) i nie są nigdzie wysyłane.</p>";
    renderBadges();
  }
  $("#btn-refresh-progress").onclick = renderProgress;
  $("#btn-print-report").onclick = function () { renderProgress(); doPrint("report"); };
  $("#btn-reset-all").onclick = function () {
    if (!window.confirm("Wyczyścić wszystkie punkty, odznaki, wyniki i plan? Tej operacji nie da się cofnąć.")) return;
    try { localStorage.removeItem(KEY); } catch (e) {}
    S = fresh(); S.days.push(today()); S.usedMigrated = true; save();
    S.sheets = []; SH = null;
    cardQueue = newCardQueue(); saveCardQueue();
    RV = null;
    renderStart(); renderCard(); renderProgress(); renderReview(); renderHub(); renderLab(); renderSheets(); updateChips();
    applyDark(); applyMute();
    toast("Postępy wyczyszczone — zaczynamy od nowa 🌱");
  };

  /* ---------------- init ---------------- */
  migrateSeenToUsed();
  renderStart();
  renderLearn();
  renderHub();
  renderCard();
  renderReview();
  renderLab();
  renderSheets();
  renderProgress();
  updateChips();
  applyDark();
  applyMute();
  $("#exam-best").textContent = S.bestExam ? S.bestExam + "%" : "—";

  // czyścimy zegary przy opuszczeniu strony
  window.addEventListener("beforeunload", function () { clearSpeed(); clearExamTimer(); });
})();
