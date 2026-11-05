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
             acts: {}, badges: [], plan: [], cards: {}, weak: {}, days: [], speedBest: 0 };
  }
  let S = (function () {
    try { const raw = localStorage.getItem(KEY); if (raw) return Object.assign(fresh(), JSON.parse(raw)); } catch (e) {}
    return fresh();
  })();
  function save() { try { localStorage.setItem(KEY, JSON.stringify(S)); } catch (e) {} }
  if (S.days.indexOf(today()) === -1) { S.days.push(today()); save(); }

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
    { id: "week", ico: "📅", name: "Systematyczność", need: "Nauka w 3 różne dni", t: function (s) { return s.days.length >= 3; } }
  ];

  function updateChips() {
    $("#chip-xp").textContent = "⭐ " + S.xp + " pkt";
    $("#chip-streak").textContent = "🔥 " + S.streak + " z rzędu";
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
    if (ok) { S.correct++; S.streak++; S.xp += 10; S.bestStreak = Math.max(S.bestStreak, S.streak); }
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
  const SHARE_URL = "https://cdn.jsdelivr.net/gh/zielinski25/Sterownik-Pieca-C.O.@arena%2Ff423a839-sterownik-pieca-c-o/docs/index.html";
  const SHARE_TEXT = "TO BE — trening przed kartkówką (angielski, 10.11) 🇬🇧 15 ćwiczeń, fiszki i próbna kartkówka. Bez logowania.";
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
    if (!text) return;
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
  function doPrint(kind) {
    const box = $("#print-report");
    box.innerHTML = kind === "report" ? "<div class='card'><h2>Raport postępów · " + today() + "</h2>" + $("#parent-report").innerHTML + "</div>" : "";
    try { window.print(); } catch (e) { toast("Drukowanie niedostępne w tej przeglądarce."); }
  }
  ["btn-print-start", "btn-print-learn"].forEach(function (id) { $("#" + id).onclick = function () { doPrint("cheat"); }; });

  /* ---------------- hub ćwiczeń ---------------- */
  function renderHub() {
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
  }
  $("#btn-random").onclick = function () { openActivity(shuffle(D.activities)[0].id); };
  $("#btn-back-hub").onclick = function () { clearSpeed(); $("#practice-run").style.display = "none"; $("#practice-hub").style.display = "block"; updateHub(); };

  /* ---------------- wspólny silnik sesji ---------------- */
  function Session(act, items, count, renderer) {
    this.act = act; this.items = shuffle(items).slice(0, count || 8);
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
    host.innerHTML = '<div class="center" style="padding:10px 0"><div style="font-size:3.4rem">' + (p >= 80 ? "🏆" : p >= 50 ? "👍" : "🌱") + '</div>' +
      '<h2>' + ok + " / " + total + " poprawnych · " + p + '%</h2><p class="muted">' + esc(msg) + "</p>" +
      '<div class="row" style="justify-content:center;margin-top:16px">' +
      '<button class="btn" id="btn-again">🔁 Jeszcze raz</button>' +
      '<button class="btn ghost" id="btn-other">🎮 Inne ćwiczenia</button>' +
      '<button class="btn orange" id="btn-to-exam">📝 Próbna kartkówka</button></div></div>';
    $("#btn-again").onclick = function () { openActivity(act); };
    $("#btn-other").onclick = function () { $("#practice-run").style.display = "none"; $("#practice-hub").style.display = "block"; updateHub(); go("practice"); };
    $("#btn-to-exam").onclick = function () { go("exam"); };
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
    let setIndex = Math.floor(Math.random() * D.pairSets.length);
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
      $("#mem-new").onclick = function () { buildMemory(si); };
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
    let si = Math.floor(Math.random() * D.matchSets.length);
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
    const items = shuffle(D.sort).slice(0, 12);
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
            it.done = true; placed[it.t].push(it.s); done++; okCount++; toast("✅ Dobrze!"); sel = null; draw();
            if (done === items.length) {
              award("sort", okCount, tries);
              summaryHost($("#run-host"), "sort", okCount, tries, "Wszystkie zdania posegregowane! 🗂️");
              return;
            }
          } else {
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
    let ti = Math.floor(Math.random() * D.reading.length);
    build(ti);
    function build(k) {
      const r = D.reading[k];
      let idx = 0, ok = 0, tries = 0;
      function draw() {
        if (idx >= r.items.length) {
          award("reading", ok, tries);
          summaryHost($("#run-host"), "reading", ok, tries, "Tekst przeczytany! 📖 Spróbuj też innego tekstu.");
          const b = document.createElement("button"); b.className = "btn ghost"; b.textContent = "📄 Inny tekst";
          b.onclick = function () { build((k + 1) % D.reading.length); };
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
    const words = shuffle(D.spelling).slice(0, 6);
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
    const pool = D.choice.map(function (x) { return { s: x.s, o: x.o, a: x.a }; });
    let time = 60, score = 0, asked = 0, cur = null;
    function draw() {
      cur = shuffle(pool)[0];
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
      save();
      $("#run-host").innerHTML = '<div class="center"><div style="font-size:3rem">⚡</div><h2>' + score + " poprawnych w 60 sekund!</h2>" +
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

  const IMPL = {
    choice: actChoice, gap: actGap, memory: actMemory, match: actMatch, order: actOrder,
    transform: actTransform, shorts: actShorts, detective: actDetective, sort: actSort,
    reading: actReading, listen: actListen, dictation: actDictation, translate: actTranslate,
    spelling: actSpelling, speed: actSpeed
  };

  /* ---------------- FISZKI ---------------- */
  let cardQueue = shuffle(D.flashcards.map(function (_, i) { return i; }));
  let cardFlipped = false;
  function renderCard() {
    if (!cardQueue.length) cardQueue = shuffle(D.flashcards.map(function (_, i) { return i; }));
    const i = cardQueue[0], c = D.flashcards[i];
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
    renderCard();
  }
  $("#btn-card-again").onclick = function () { nextCard(false); };
  $("#btn-card-know").onclick = function () { nextCard(true); };
  $("#btn-shuffle-cards").onclick = function () { cardQueue = shuffle(D.flashcards.map(function (_, i) { return i; })); renderCard(); toast("🔀 Karty przetasowane"); };
  $("#btn-reset-cards").onclick = function () { S.cards = {}; save(); cardQueue = shuffle(D.flashcards.map(function (_, i) { return i; })); renderCard(); toast("Fiszki zaczynają od nowa"); };

  /* ---------------- KARTKÓWKA PRÓBNA ---------------- */
  let EX = null;
  function startExam(mode) {
    clearExamTimer();
    const qs = shuffle(D.examPool).slice(0, 15);
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
    mark("exam", ok);
    if (!ok) noteWeak(q.area);
    if (EX.mode === "learn") {
      const fb = $("#exam-fb");
      fb.className = "feedback show " + (ok ? "ok" : "no");
      fb.innerHTML = (ok ? "✅ Brawo! " : "❌ Poprawnie: <u>" + esc(q.a[0] || q.a) + "</u>. ") + esc(q.w);
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
    const act = S.acts.exam || { tried: 0, ok: 0, done: false }; act.done = true; S.acts.exam = act;
    save(); updateChips(); updateHub();
    $("#exam-run").style.display = "none";
    $("#exam-result").style.display = "block";
    const weak = {};
    EX.answers.forEach(function (a) { if (!a.ok) weak[a.q.area] = (weak[a.q.area] || 0) + 1; });
    const weakList = Object.keys(weak).sort(function (a, b) { return weak[b] - weak[a]; });
    $("#exam-result").innerHTML =
      '<div class="card hero center"><div style="font-size:3rem">' + (p >= 90 ? "🏆" : p >= 70 ? "👍" : "🌱") + '</div><h1>' + ok + " / " + total + " · " + p + "%</h1>" +
      "<p>" + (p >= 90 ? "Jesteś gotowy/a na kartkówkę! 💪" : p >= 70 ? "Dobrze idzie! Popraw jeszcze poniższe zadania." : "Spokojnie — to była próba. Wróć do ćwiczeń i spróbuj ponownie.") + "</p>" +
      '<div class="muted" style="color:rgba(255,255,255,.9)">Czas: ' + Math.floor(EX.seconds / 60) + " min " + (EX.seconds % 60) + " s · tryb: " + (EX.mode === "real" ? "kartkówka" : "nauka") + "</div></div>" +
      '<div class="card"><div class="between"><h2>🔍 Omówienie wszystkich zadań</h2><span class="pill teal">Najlepszy wynik: ' + S.bestExam + "%</span></div>" +
      (weakList.length ? '<p class="muted">Nad tym warto popracować: <b>' + weakList.map(function (w) { return w + " (" + weak[w] + ")"; }).join(", ") + "</b></p>" : "<p class='muted'>Zero błędów — pięknie! 🎉</p>") +
      EX.answers.map(function (a, i) {
        return '<div class="exam-q review ' + (a.ok ? "ok" : "no") + '"><div class="num">' + (i + 1) + ". " + esc(a.q.area) + (a.ok ? " ✓" : " ✗") + '</div><div class="p">' + esc(a.q.p) + "</div>" +
          '<div class="muted">Twoja odpowiedź: <b>' + esc(a.given) + "</b></div>" +
          (a.ok ? "" : '<div>Poprawnie: <b>' + esc(Array.isArray(a.q.a) ? a.q.a[0] : a.q.a) + "</b></div>") +
          '<div class="review-note">💡 ' + esc(a.q.w) + "</div></div>";
      }).join("") +
      '<div class="row" style="margin-top:14px"><button class="btn" id="ex-again">🔁 Nowa kartkówka</button><button class="btn orange" id="ex-practice">🎮 Poćwicz słabe strony</button><button class="btn ghost" id="ex-back">← Wróć</button></div></div>';
    $("#ex-again").onclick = function () { startExam(EX.mode); };
    $("#ex-practice").onclick = function () { go("practice"); };
    $("#ex-back").onclick = function () { $("#exam-result").style.display = "none"; $("#exam-intro").style.display = "block"; };
    if (p >= 80) confetti(40);
    checkBadges();
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
      "<p><b>Rekomendacja:</b> 10–15 minut dziennie według planu z zakładki Start. Dzień przed kartkówką: tabela <i>to be</i>, fiszki z błędami i jedna próbna kartkówka bez podpowiedzi. Zwróć uwagę na trzy pułapki: <i>amn't</i>, <i>Yes, I'm</i> oraz wiek z <i>have</i>.</p>" +
      "<p class='muted'>Dane pochodzą wyłącznie z tej przeglądarki (localStorage) i nie są nigdzie wysyłane.</p>";
    renderBadges();
  }
  $("#btn-refresh-progress").onclick = renderProgress;
  $("#btn-print-report").onclick = function () { renderProgress(); doPrint("report"); };
  $("#btn-reset-all").onclick = function () {
    if (!window.confirm("Wyczyścić wszystkie punkty, odznaki, wyniki i plan? Tej operacji nie da się cofnąć.")) return;
    try { localStorage.removeItem(KEY); } catch (e) {}
    S = fresh(); S.days.push(today()); save();
    cardQueue = shuffle(D.flashcards.map(function (_, i) { return i; }));
    renderStart(); renderCard(); renderProgress(); updateChips();
    toast("Postępy wyczyszczone — zaczynamy od nowa 🌱");
  };

  /* ---------------- init ---------------- */
  renderStart();
  renderLearn();
  renderHub();
  renderCard();
  renderProgress();
  updateChips();
  $("#exam-best").textContent = S.bestExam ? S.bestExam + "%" : "—";

  // czyścimy zegary przy opuszczeniu strony
  window.addEventListener("beforeunload", function () { clearSpeed(); clearExamTimer(); });
})();
