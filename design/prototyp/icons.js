/* ═══════════════════════════════════════════════════════════════════════════
   ILUSTRACJE KAFELKÓW — port ICO/BOJLER_SVG z main_centrala.cpp (ESP) + dodatki:
   noc (księżyc + gwiazdy), mróz, manometr 270°, kanał przepustnicy serwa,
   pierścień przepływu pompy tylko gdy pracuje, poświaty.
   Każda ilustracja: { svg(state) -> markup, update(el, state) -> ustawia stan }.
   To samo, co już jest w TileArt.kt (Android) — tu służy do dopracowania designu.
   ═══════════════════════════════════════════════════════════════════════════ */
(function (global) {
  const NS = 'http://www.w3.org/2000/svg';
  const mix = (a, b, f) => {
    const p = h => [parseInt(h.slice(1, 3), 16), parseInt(h.slice(3, 5), 16), parseInt(h.slice(5, 7), 16)];
    const A = p(a), B = p(b), k = Math.max(0, Math.min(1, f));
    return 'rgb(' + A.map((v, i) => Math.round(v + (B[i] - v) * k)).join(',') + ')';
  };
  const wrap = inner => '<svg viewBox="0 0 24 24" fill="none" xmlns="' + NS + '">' + inner + '</svg>';
  let uid = 0;

  const SLONCE = '<g class="ico-slonce"><circle cx="9.5" cy="9.5" r="3.3" fill="#ffd94d"/><path d="M9.5 3v1.8M9.5 15.2v1M3 9.5h1.8M15.2 9.5h1M5 5l1.3 1.3M12.7 12.7l1 1M14 5l-1.3 1.3M6.3 12.7l-1 1" stroke="#ffd94d" stroke-width="1.3" stroke-linecap="round"/></g>';
  const KSIEZYC = '<g class="ico-moon"><circle cx="3.6" cy="4.6" r=".55" fill="#fef3c7" class="star s1"/><circle cx="16.4" cy="4.2" r=".45" fill="#fef3c7" class="star s2"/><circle cx="5.2" cy="13.4" r=".4" fill="#fef3c7" class="star s3"/><path d="M9.5 6.1a3.4 3.4 0 1 0 3.3 4.3 2.9 2.9 0 0 1-3.3-4.3Z" fill="#e2e8f0"/></g>';
  const CHMURA = f => '<g class="ico-chmura"><ellipse cx="14.5" cy="16.5" rx="6" ry="3" fill="' + (f ? '#aecbe9' : '#cbd5e1') + '"/><circle cx="11.3" cy="15" r="2.4" fill="' + (f ? '#d3e3f7' : '#e2e8f0') + '"/><circle cx="15.5" cy="13.8" r="2.9" fill="' + (f ? '#d3e3f7' : '#e2e8f0') + '"/></g>';

  const ILU = {
    zewn: {
      svg: s => wrap('<circle cx="9.5" cy="9.5" r="6.5" fill="url(#gl-' + (s.night ? 'moon' : 'sun') + ')"/>' + (s.night ? KSIEZYC : SLONCE) + CHMURA(s.frost)),
      key: s => (s.night ? 'n' : 'd') + (s.frost ? 'f' : '')
    },
    ogrz: {
      svg: s => wrap('<circle cx="12" cy="17" r="9" fill="url(#gl-' + (s.alarm ? 'err' : 'ember') + ')" class="glow-pulse"/>'
        + '<path class="plomien-zew' + (s.alarm ? ' fast' : '') + '" d="M12 21c-3.3 0-5.8-2.3-5.8-5.6 0-2.7 1.8-4.6 2.7-7.3.5 1.8 1.8 2.7 1.8 2.7-.5-2.7 1-5.5 2.9-6.4-1 2.7 0 4.6 1.4 6 1.4 1.4 2.4 3.2 2.4 4.9 0 3.3-2.3 5.7-5.4 5.7Z" fill="#c2410c"/>'
        + '<path class="plomien-sr' + (s.alarm ? ' fast' : '') + '" d="M12.2 19.5c-2.1 0-3.7-1.5-3.7-3.7 0-1.8 1.2-3 1.8-4.8.3 1.2 1.2 1.8 1.2 1.8-.3-1.8.7-3.7 1.9-4.3-.7 1.8 0 3 .9 3.9 1 1 1.6 2.1 1.6 3.3 0 2.1-1.5 3.8-3.7 3.8Z" fill="#fb923c"/>'
        + '<path class="plomien-wew' + (s.alarm ? ' fast' : '') + '" d="M12.3 17.6c-1 0-1.9-.8-1.9-2 0-1 .6-1.6.9-2.6.15.6.6 1 .6 1-.15-1 .4-2 1-2.4-.4 1 0 1.6.5 2.2.5.5.9 1.1.9 1.8 0 1.2-.9 2-2 2Z" fill="#fde047"/>'),
      key: s => s.alarm ? 'a' : ''
    },
    bojler: {
      // zbiornik c.w.u.: poziom = temperatura (jak ESP), ale zamiast szarego mieszania barw
      // warstwa gorąca (bursztyn) narasta od góry (stratyfikacja), zimna woda zostaje niebieska
      svg: s => {
        const id = 'bclip' + (++uid);
        return wrap('<defs><clipPath id="' + id + '"><path d="M12 3.4c3.4 0 5.8 1.2 5.8 2.8v11.2c0 1.6-2.4 2.8-5.8 2.8s-5.8-1.2-5.8-2.8V6.2c0-1.6 2.4-2.8 5.8-2.8Z"/></clipPath></defs>'
          + '<g class="bojler-para"><path d="M9.2 3.2c-.8-.9-.8-1.7 0-2.5" stroke="#e2e8f0" stroke-width="1" stroke-linecap="round"/><path d="M12.4 2.6c-.8-.9-.8-1.7 0-2.5" stroke="#e2e8f0" stroke-width="1" stroke-linecap="round"/><path d="M15.6 3.2c-.8-.9-.8-1.7 0-2.5" stroke="#e2e8f0" stroke-width="1" stroke-linecap="round"/></g>'
          + '<path d="M17.8 8.6h2.6M17.8 15.4h2.6" stroke="#64748b" stroke-width="1.5" stroke-linecap="round"/>'
          + '<path d="M12 3.4c3.4 0 5.8 1.2 5.8 2.8v11.2c0 1.6-2.4 2.8-5.8 2.8s-5.8-1.2-5.8-2.8V6.2c0-1.6 2.4-2.8 5.8-2.8Z" fill="#0b1324"/>'
          + '<g clip-path="url(#' + id + ')"><rect class="bojler-cold" x="5" y="20.2" width="14" height="0" fill="url(#gl-cold)"/>'
          + '<rect class="bojler-hot" x="5" y="20.2" width="14" height="0" fill="url(#gl-hot)"/>'
          + '<rect class="bojler-surf" x="5" y="20.2" width="14" height="2.6" fill="url(#gl-surf)"/>'
          + '<g class="bojler-babelki"><circle class="bojler-babel" cx="9" cy="17" r=".7" style="animation-delay:0s"/><circle class="bojler-babel" cx="13.2" cy="18" r=".55" style="animation-delay:.8s"/><circle class="bojler-babel" cx="15" cy="16" r=".6" style="animation-delay:1.6s"/></g></g>'
          + '<path d="M12 3.4c3.4 0 5.8 1.2 5.8 2.8v11.2c0 1.6-2.4 2.8-5.8 2.8s-5.8-1.2-5.8-2.8V6.2c0-1.6 2.4-2.8 5.8-2.8Z" stroke="#7c8ea3" stroke-width="1.5"/>'
          + '<path d="M7.7 7.2v9" stroke="rgba(255,255,255,.16)" stroke-width=".9" stroke-linecap="round"/>'
          + '<path d="M9.6 21.6h4.8" stroke="#64748b" stroke-width="1.5" stroke-linecap="round"/>');
      },
      update: (el, s) => {
        const t = s.t_bojler;
        const pct = Math.max(.08, Math.min(.95, (t - 15) / 55));        // poziom słupka (ESP)
        const k = Math.max(0, Math.min(1, (t - 30) / 28));              // udział warstwy gorącej 30→58 °C
        const H = 15.4, top = 20.2 - H * pct, hotH = H * pct * k;
        const q = sel => el.querySelector(sel);
        const c = q('.bojler-cold'), h = q('.bojler-hot'), sf = q('.bojler-surf'), p = q('.bojler-para'), b = q('.bojler-babelki');
        if (c) { c.setAttribute('y', top.toFixed(2)); c.setAttribute('height', (H * pct).toFixed(2)); }
        if (h) { h.setAttribute('y', top.toFixed(2)); h.setAttribute('height', hotH.toFixed(2)); }
        if (sf) sf.setAttribute('y', top.toFixed(2));
        if (p) p.style.opacity = t > 45 ? 1 : 0;
        if (b) b.style.opacity = t > 40 ? 1 : 0;
      }
    },
    panel: {
      // kolektor pod kątem + słońce (w nocy przygaszone, bez „iskier” energii)
      svg: s => wrap('<g class="ico-slonce2" style="opacity:' + (s.night ? '.22' : '1') + '"><circle cx="18" cy="5.6" r="2.6" fill="url(#gl-sun)"/><circle cx="18" cy="5.6" r="1.9" fill="#ffd94d"/><path d="M18 1.6v1.1M18 8.5v1.1M14 5.6h1.1M20.9 5.6H22M15.2 2.8l.8.8M20 7.6l.8.8M20.8 2.8l-.8.8M16 7.6l-.8.8" stroke="#ffd94d" stroke-width="1.1" stroke-linecap="round"/></g>'
        + '<path d="M5.2 9.6h11.2l2.4 9H2.8Z" fill="' + (s.alarm ? '#3b1d2a' : '#152238') + '" stroke="' + (s.alarm ? '#f87171' : '#5b6f86') + '" stroke-width=".7" stroke-linejoin="round"/>'
        + '<path d="M4.4 12.6h13.2M3.6 15.6h14.8M9 9.6l-1.9 9M12.6 9.6l1.9 9" stroke="' + (s.alarm ? '#fca5a5' : '#38bdf8') + '" stroke-width=".55" opacity=".75"/>'
        + '<path d="M6.2 10.4h2.2l-1.4 7.4H5Z" fill="rgba(255,255,255,.07)"/>'
        + '<path d="M8 18.6v2.2M15.2 18.6v2.2M6.4 20.8h10.4" stroke="#64748b" stroke-width="1.1" stroke-linecap="round"/>'
        + (s.night ? '' : '<circle class="energia" cx="9.4" cy="14.1" r=".55" fill="#fde047"/><circle class="energia" cx="14.3" cy="17" r=".5" fill="#fde047" style="animation-delay:.9s"/>')),
      key: s => (s.alarm ? 'a' : '') + (s.night ? 'n' : '')
    },
    pokoj: {
      svg: () => wrap('<circle cx="12" cy="14" r="5" fill="url(#gl-okno)" class="okno"/>'
        + '<path d="M4 11 12 4l8 7" stroke="#94a3b8" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"/><path d="M6 10v9h12v-9" stroke="#94a3b8" stroke-width="1.6" stroke-linejoin="round"/>'
        + '<rect class="okno" x="9.3" y="12" width="5.4" height="4.2" rx=".4" fill="#fbbf24"/><path d="M10 19v-3.2h4V19" stroke="#64748b" stroke-width="1.4"/>')
    },
    cisnienie: {
      // manometr 270°: łuk 7:30 → 4:30 przez górę, kolor zielony → czerwony, igła z odczytu
      svg: () => wrap('<circle cx="12" cy="12" r="8" stroke="#334155" stroke-width="2.2" stroke-dasharray="37.7 50.3" transform="rotate(135 12 12)" stroke-linecap="round"/>'
        + '<circle class="cis-luk" cx="12" cy="12" r="8" stroke="#4ade80" stroke-width="2.2" stroke-linecap="round" stroke-dasharray="0 50.3" transform="rotate(135 12 12)"/>'
        + '<g stroke="rgba(241,245,249,.35)" stroke-width=".7" stroke-linecap="round"><path d="M12 5.6v1" transform="rotate(-135 12 12)"/><path d="M12 5.6v1" transform="rotate(-67.5 12 12)"/><path d="M12 5.6v1"/><path d="M12 5.6v1" transform="rotate(67.5 12 12)"/><path d="M12 5.6v1" transform="rotate(135 12 12)"/></g>'
        + '<line class="cis-igla" x1="12" y1="12" x2="12" y2="5.3" stroke="#f1f5f9" stroke-width="1.4" stroke-linecap="round"/><circle cx="12" cy="12" r="1.5" fill="#f1f5f9"/>'),
      update: (el, s) => {
        const fr = Math.max(.03, Math.min(.97, (s.cisnienie - 970) / 70));
        const l = el.querySelector('.cis-luk'), i = el.querySelector('.cis-igla');
        if (l) { l.setAttribute('stroke-dasharray', (37.7 * fr).toFixed(1) + ' 50.3'); l.style.stroke = mix('#4ade80', '#f87171', fr); }
        if (i) i.style.transform = 'rotate(' + (fr * 270 - 135).toFixed(1) + 'deg)';
      }
    },
    wilgotnosc: {
      svg: () => wrap('<path d="M12 3.2s6 6.8 6 11.1a6 6 0 0 1-12 0c0-4.3 6-11.1 6-11.1Z" fill="url(#kropGrad)"/>'
        + '<ellipse cx="9.9" cy="12.6" rx=".8" ry="1.7" fill="rgba(255,255,255,.32)" transform="rotate(-18 9.9 12.6)"/>'
        + '<ellipse class="fala fala1" cx="12" cy="21.5" rx="3" ry="1" stroke="#38bdf8" stroke-width=".8"/><ellipse class="fala fala2" cx="12" cy="21.5" rx="5" ry="1.6" stroke="#38bdf8" stroke-width=".6"/>')
    },
    pompa: {
      svg: s => wrap((s.pompa ? '<circle cx="12" cy="12" r="11" fill="url(#gl-ringcyan)"/>' : '')
        + '<circle class="przeplyw' + (s.pompa ? ' on' : '') + '" cx="12" cy="12" r="9.3" stroke="#38bdf8" stroke-width=".9" stroke-dasharray="3 5" opacity="' + (s.pompa ? '.55' : '.28') + '"/>'
        + '<g class="wirnik' + (s.pompa ? ' spin' : '') + '"><path d="M12 10.4c0-3 1.6-5.2 3.6-5.2 1.6 0 2.4 1.3 1.6 2.7-.9 1.5-3 2.5-5.2 2.5Z" fill="#22d3ee"/><path d="M13.6 12c3 0 5.2 1.6 5.2 3.6 0 1.6-1.3 2.4-2.7 1.6-1.5-.9-2.5-3-2.5-5.2Z" fill="#0ea5e9"/><path d="M12 13.6c0 3-1.6 5.2-3.6 5.2-1.6 0-2.4-1.3-1.6-2.7.9-1.5 3-2.5 5.2-2.5Z" fill="#0284c7"/><path d="M10.4 12c-3 0-5.2-1.6-5.2-3.6 0-1.6 1.3-2.4 2.7-1.6 1.5.9 2.5 3 2.5 5.2Z" fill="#0891b2"/></g>'
        + '<circle cx="12" cy="12" r="1.7" fill="#e0f2fe"/>'),
      key: s => s.pompa ? 'on' : 'off'
    },
    serwo: {
      // przepustnica w kanale (widok z boku): klapa 0 % = pionowo (zamknięta), 100 % = poziomo (otwarta);
      // AUTO = cyjan + delikatny ruch regulacji, RĘCZNY = bursztyn, BEZPIECZNY = czerwień (klapa zamknięta)
      svg: s => {
        const m = s.tryb_serwa, auto = !(m === 2 || m === 3);
        const col = m === 2 ? '#fbbf24' : m === 3 ? '#f87171' : '#22d3ee', col2 = m === 2 ? '#fde68a' : m === 3 ? '#fecaca' : '#a5f3fc';
        return wrap('<rect x="1.6" y="6.6" width="20.8" height="10.8" rx="2.4" fill="#0b1324" stroke="#4b5c73" stroke-width="1.3"/>'
          + '<path d="M1.6 9.6h20.8M1.6 14.4h20.8" stroke="rgba(255,255,255,.05)" stroke-width=".8"/>'
          + '<g class="serwo-przeplyw' + (auto ? ' on' : '') + '" stroke="' + col2 + '" stroke-width="1.1" stroke-linecap="round" stroke-linejoin="round" opacity=".55"><path d="M4.2 10.4l1.4 1.6-1.4 1.6"/><path d="M7 10.4l1.4 1.6L7 13.6"/><path d="M15.6 10.4l1.4 1.6-1.4 1.6"/><path d="M18.4 10.4l1.4 1.6-1.4 1.6"/></g>'
          + '<g class="klapa-kat"><g class="klapa-wobble' + (auto ? ' aktywne' : '') + '"><rect x="11" y="3.6" width="2" height="16.8" rx="1" fill="' + col + '"/><rect x="11.5" y="4.2" width=".6" height="15.6" rx=".3" fill="rgba(255,255,255,.35)"/></g></g>'
          + '<circle cx="12" cy="12" r="2" fill="#0f172a" stroke="' + col2 + '" stroke-width="1"/><circle cx="12" cy="12" r=".7" fill="' + col2 + '"/>');
      },
      update: (el, s) => {
        const m = s.tryb_serwa;
        const pct = m === 3 ? 0 : Math.max(0, Math.min(1, (s.klapa || 0) / 180));
        const g = el.querySelector('.klapa-kat'), f = el.querySelector('.serwo-przeplyw');
        if (g) g.style.transform = 'rotate(' + (pct * 90).toFixed(1) + 'deg)';
        if (f) f.style.opacity = (0.12 + 0.6 * pct).toFixed(2);
      },
      key: s => String(s.tryb_serwa)
    },
    mieszadlo: {
      // mieszadło: silnik, wał, dwa piętra łopat — gdy pracuje, łopaty „obracają się” (scaleX) i woda wiruje
      svg: s => {
        const on = !!s.mieszadlo, pad = on ? '#4ade80' : '#64748b', pad2 = on ? '#86efac' : '#94a3b8';
        return wrap((on ? '<circle cx="12" cy="13" r="10.5" fill="url(#gl-ringgreen)"/>' : '')
          + '<rect x="8.4" y="1.8" width="7.2" height="4.4" rx="1.3" fill="#64748b"/><rect x="9.2" y="2.5" width="5.6" height="1.1" rx=".5" fill="rgba(255,255,255,.22)"/>'
          + '<line x1="12" y1="6.2" x2="12" y2="20.2" stroke="#94a3b8" stroke-width="1.6" stroke-linecap="round"/>'
          + '<g class="wir' + (on ? ' on' : '') + '" stroke="#38bdf8" stroke-width=".9" stroke-linecap="round" fill="none" opacity="' + (on ? '.6' : '.18') + '"><path d="M3.8 17.2c1.4-1.2 3-1.6 4.4-1.2"/><path d="M20.2 17.2c-1.4-1.2-3-1.6-4.4-1.2"/><path d="M4.6 20.6c1.2-.9 2.6-1.2 3.8-1"/><path d="M19.4 20.6c-1.2-.9-2.6-1.2-3.8-1"/></g>'
          + '<rect class="paddle p1' + (on ? ' spin' : '') + '" x="7.2" y="11.6" width="9.6" height="2" rx="1" fill="' + pad2 + '"/>'
          + '<rect class="paddle p2' + (on ? ' spin' : '') + '" x="5.4" y="17.6" width="13.2" height="2.3" rx="1.15" fill="' + pad + '"/>'
          + '<circle cx="12" cy="18.75" r=".9" fill="#0f172a"/><circle cx="12" cy="12.6" r=".8" fill="#0f172a"/>');
      },
      key: s => s.mieszadlo ? 'on' : 'off'
    },
    dym: {
      // czujnik dymu: komora pomiarowa z diodą + smugi dymu unoszące się nad nią; alarm = czerwień + puls
      svg: s => {
        const a = s.dym_alarm, off = s.off, c1 = a ? '#fca5a5' : '#94a3b8', c2 = a ? '#f87171' : '#cbd5e1', f = a ? ' fast' : '';
        const led = a ? '#f87171' : off ? '#475569' : '#4ade80';
        return wrap((a ? '<circle cx="12" cy="13" r="10.5" fill="url(#gl-err)" class="glow-pulse"/>' : '')
          + '<g class="dym-smugi" style="opacity:' + (off ? '.3' : '1') + '">'
          + '<path class="dym-w1' + f + '" d="M7.4 16.4c1-1.3 1-2.4 0-3.7s-1-2.4 0-3.7 1-2.4 0-3.7" stroke="' + c1 + '" stroke-width="1.6" stroke-linecap="round" opacity=".75"/>'
          + '<path class="dym-w2' + f + '" d="M12 16.4c1-1.3 1-2.4 0-3.7s-1-2.4 0-3.7 1-2.4 0-3.7 1-2.4 0-3.7" stroke="' + c2 + '" stroke-width="1.7" stroke-linecap="round"/>'
          + '<path class="dym-w3' + f + '" d="M16.6 16.4c1-1.3 1-2.4 0-3.7s-1-2.4 0-3.7 1-2.4 0-3.7" stroke="' + c1 + '" stroke-width="1.6" stroke-linecap="round" opacity=".75"/></g>'
          + '<rect x="4.2" y="17" width="15.6" height="4.6" rx="1.8" fill="#162032" stroke="' + (a ? '#f87171' : '#4b5c73') + '" stroke-width="1.1"/>'
          + '<path d="M7.4 19.3h6.4" stroke="rgba(255,255,255,.18)" stroke-width="1" stroke-linecap="round"/><path d="M7.4 20.4h4.2" stroke="rgba(255,255,255,.1)" stroke-width=".8" stroke-linecap="round"/>'
          + '<circle class="dym-led' + (a ? ' alarm' : '') + '" cx="16.6" cy="19.3" r="1" fill="' + led + '"/>');
      },
      key: s => (s.dym_alarm ? 'a' : '') + (s.off ? 'o' : '')
    },
    wykresy: {
      svg: () => wrap('<path d="M3 20.7h18" stroke="#334155" stroke-width=".8" stroke-linecap="round"/><rect class="slupek s1" x="3.5" y="14" width="3.4" height="6" rx=".6" fill="#00d4f5"/><rect class="slupek s2" x="10.3" y="8" width="3.4" height="12" rx=".6" fill="#ff9f43"/><rect class="slupek s3" x="17.1" y="11" width="3.4" height="9" rx=".6" fill="#ffd32a"/>')
    },
    czas: {
      svg: () => wrap('<circle cx="12" cy="12" r="9" stroke="#64748b" stroke-width="1.4"/><path d="M12 4v1.3M12 18.7V20M4 12h1.3M18.7 12H20" stroke="#64748b" stroke-width="1.2" stroke-linecap="round"/>'
        + '<line class="wsk-h" x1="12" y1="12" x2="12" y2="7.8" stroke="#cbd5e1" stroke-width="1.5" stroke-linecap="round"/><line class="wsk-m" x1="12" y1="12" x2="12" y2="5.8" stroke="#cbd5e1" stroke-width="1.3" stroke-linecap="round"/><line class="wsk-s" x1="12" y1="12" x2="12" y2="5.2" stroke="#f87171" stroke-width=".8" stroke-linecap="round"/><circle cx="12" cy="12" r="1.1" fill="#f1f5f9"/>'),
      update: el => {
        const d = new Date(), h = d.getHours() % 12, m = d.getMinutes(), s = d.getSeconds() + d.getMilliseconds() / 1000;
        const set = (q, a) => { const e = el.querySelector(q); if (e) e.style.transform = 'rotate(' + a.toFixed(1) + 'deg)'; };
        set('.wsk-h', (h + m / 60) * 30); set('.wsk-m', (m + s / 60) * 6); set('.wsk-s', s * 6);
      }
    }
  };

  // Wspólne gradienty (jeden <svg> z <defs> w dokumencie; referencje url(#id) działają globalnie)
  ILU.defs = '<svg width="0" height="0" style="position:absolute"><defs>'
    + '<linearGradient id="kropGrad" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#7dd3fc"/><stop offset="1" stop-color="#0284c7"/></linearGradient>'
    + '<radialGradient id="gl-sun"><stop offset="0" stop-color="#ffd94d" stop-opacity=".32"/><stop offset="1" stop-color="#ffd94d" stop-opacity="0"/></radialGradient>'
    + '<radialGradient id="gl-moon"><stop offset="0" stop-color="#e2e8f0" stop-opacity=".18"/><stop offset="1" stop-color="#e2e8f0" stop-opacity="0"/></radialGradient>'
    + '<radialGradient id="gl-ember"><stop offset="0" stop-color="#fb923c" stop-opacity=".3"/><stop offset="1" stop-color="#fb923c" stop-opacity="0"/></radialGradient>'
    + '<radialGradient id="gl-err"><stop offset="0" stop-color="#f87171" stop-opacity=".32"/><stop offset="1" stop-color="#f87171" stop-opacity="0"/></radialGradient>'
    + '<radialGradient id="gl-okno"><stop offset="0" stop-color="#fbbf24" stop-opacity=".28"/><stop offset="1" stop-color="#fbbf24" stop-opacity="0"/></radialGradient>'
    + '<radialGradient id="gl-ringcyan"><stop offset="0" stop-color="#38bdf8" stop-opacity="0"/><stop offset="1" stop-color="#38bdf8" stop-opacity=".16"/></radialGradient>'
    + '<radialGradient id="gl-ringgreen"><stop offset="0" stop-color="#4ade80" stop-opacity="0"/><stop offset="1" stop-color="#4ade80" stop-opacity=".14"/></radialGradient>'
    + '<linearGradient id="gl-cold" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#5bb8ff"/><stop offset="1" stop-color="#1d5fd1"/></linearGradient>'
    + '<linearGradient id="gl-hot" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffc46b"/><stop offset=".82" stop-color="#ff8a3d"/><stop offset="1" stop-color="#ff8a3d" stop-opacity=".15"/></linearGradient>'
    + '<linearGradient id="gl-surf" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#fff" stop-opacity=".16"/><stop offset="1" stop-color="#fff" stop-opacity="0"/></linearGradient>'
    + '</defs></svg>';

  // Ikony nawigacji / kart (proste, jednokolorowe — jak NativeIconView)
  const stroke = (d, extra) => '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" xmlns="' + NS + '">' + d + (extra || '') + '</svg>';
  ILU.nav = {
    dashboard: stroke('<rect x="3.5" y="3.5" width="7" height="7" rx="2"/><rect x="13.5" y="3.5" width="7" height="7" rx="2"/><rect x="3.5" y="13.5" width="7" height="7" rx="2"/><rect x="13.5" y="13.5" width="7" height="7" rx="2"/>'),
    chart: stroke('<path d="M4 19V5M4 19h16"/><path d="M7 15c2-4 4-1 6-5s3 1 6-3"/>'),
    weather: stroke('<circle cx="9" cy="9" r="3.2"/><path d="M9 3v1.5M9 13.5V15M3 9h1.5M13.5 9H15M4.8 4.8l1 1M12.2 12.2l1 1M13.2 4.8l-1 1M5.8 12.2l-1 1"/><path d="M12 19h7a3 3 0 0 0 0-6 4 4 0 0 0-7.6-1"/>'),
    settings: stroke('<circle cx="12" cy="12" r="3"/><path d="M12 3v2.2M12 18.8V21M3 12h2.2M18.8 12H21M5.6 5.6l1.6 1.6M16.8 16.8l1.6 1.6M18.4 5.6l-1.6 1.6M7.2 16.8l-1.6 1.6"/>'),
    more: stroke('<circle cx="5" cy="12" r="1.4" fill="currentColor"/><circle cx="12" cy="12" r="1.4" fill="currentColor"/><circle cx="19" cy="12" r="1.4" fill="currentColor"/>'),
    shield: stroke('<path d="M12 3l7 3v5c0 5-3.2 8.3-7 10-3.8-1.7-7-5-7-10V6l7-3Z"/><path d="M9 12l2 2 4-4"/>'),
    next: stroke('<path d="M9 6l6 6-6 6"/>'),
    close: stroke('<path d="M6 6l12 12M18 6L6 18"/>'),
    clock: stroke('<circle cx="12" cy="12" r="8.5"/><path d="M12 7.5V12l3 2"/>'),
    pump: stroke('<circle cx="12" cy="12" r="8"/><path d="M12 12c0-3 1.4-4.5 3-4.5M12 12c3 0 4.5 1.4 4.5 3M12 12c0 3-1.4 4.5-3 4.5M12 12c-3 0-4.5-1.4-4.5-3"/>'),
    servo: stroke('<circle cx="12" cy="12" r="8.5"/><ellipse cx="12" cy="12" rx="7" ry="2.6"/>'),
    mixer: stroke('<path d="M12 3v18"/><path d="M12 12l4-2.5M12 12l-4 2.5M12 12l3 3.5"/>'),
    thermo: stroke('<path d="M10 14.5V5a2 2 0 0 1 4 0v9.5a3.5 3.5 0 1 1-4 0Z"/><path d="M12 10v6"/>'),
    outside: stroke('<circle cx="12" cy="12" r="4"/><path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4M18.4 5.6L17 7M7 17l-1.4 1.4"/>'),
    logs: stroke('<path d="M5 5h14v14H5z"/><path d="M8 9h8M8 12h8M8 15h5"/>'),
    terminal: stroke('<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="M7 9l3 3-3 3M12 15h5"/>'),
    upload: stroke('<path d="M12 16V5M7 10l5-5 5 5"/><path d="M5 19h14"/>'),
    session: stroke('<circle cx="12" cy="9" r="3.5"/><path d="M5 20c1-4 4-5.5 7-5.5s6 1.5 7 5.5"/>')
  };

  // HERO: bojler z poziomem wody (t_ogrz), płomieniami gdy grzeje, poświatą w alarmie
  ILU.heroBoiler = {
    svg: () => '<svg viewBox="0 0 112 112" fill="none" xmlns="' + NS + '">'
      + '<defs><clipPath id="heroClip"><rect x="34" y="14" width="44" height="66" rx="12"/></clipPath><linearGradient id="heroWater" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#ffb04a"/><stop offset="1" stop-color="#ff7a3d"/></linearGradient></defs>'
      + '<circle class="hero-glow" cx="56" cy="60" r="46" fill="url(#gl-ember)" opacity=".6"/>'
      + '<g clip-path="url(#heroClip)"><rect class="hero-fill" x="34" y="80" width="44" height="0" fill="url(#heroWater)" opacity=".85"/>'
      + '<circle class="bojler-babel" cx="46" cy="72" r="1.6" style="animation-duration:2.8s"/><circle class="bojler-babel" cx="58" cy="76" r="1.3" style="animation-delay:.9s;animation-duration:2.8s"/><circle class="bojler-babel" cx="66" cy="70" r="1.4" style="animation-delay:1.7s;animation-duration:2.8s"/></g>'
      + '<rect x="34" y="14" width="44" height="66" rx="12" stroke="#64748b" stroke-width="2.4"/>'
      + '<path d="M44 18v58" stroke="rgba(255,255,255,.12)" stroke-width="2" stroke-linecap="round"/>'
      + '<rect x="30" y="80" width="52" height="6" rx="3" fill="#1e293b" stroke="#334155"/>'
      + '<path d="M50 8h12v6H50zM40 86v8M72 86v8" stroke="#475569" stroke-width="2.4" stroke-linecap="round"/>'
      + '<g class="hero-flames"><path class="plomien-zew" d="M56 104c-5 0-8.8-3.5-8.8-8.5 0-4 2.7-7 4.1-11 .8 2.7 2.7 4 2.7 4-.8-4 1.5-8.3 4.4-9.7-1.5 4 0 7 2.1 9.1 2.1 2.1 3.6 4.8 3.6 7.4 0 5-3.5 8.7-8.1 8.7Z" fill="#c2410c" style="transform-origin:56px 104px"/>'
      + '<path class="plomien-sr" d="M56.3 101.7c-3.2 0-5.6-2.3-5.6-5.6 0-2.7 1.8-4.5 2.7-7.2.5 1.8 1.8 2.7 1.8 2.7-.5-2.7 1-5.6 2.9-6.5-1 2.7 0 4.5 1.4 5.9 1.5 1.5 2.4 3.2 2.4 5 0 3.2-2.3 5.7-5.6 5.7Z" fill="#fb923c" style="transform-origin:56px 101.7px"/>'
      + '<path class="plomien-wew" d="M56.4 98.8c-1.5 0-2.9-1.2-2.9-3 0-1.5.9-2.4 1.4-3.9.2.9.9 1.5.9 1.5-.2-1.5.6-3 1.5-3.6-.6 1.5 0 2.4.8 3.3.8.8 1.4 1.7 1.4 2.7 0 1.8-1.4 3-3.1 3Z" fill="#fde047" style="transform-origin:56.4px 98.8px"/></g>'
      + '<text class="hero-t" x="56" y="52" text-anchor="middle" font-size="15" font-weight="700" fill="#fff" font-family="Roboto,sans-serif">--</text>'
      + '</svg>',
    update: (el, s) => {
      const t = s.t_ogrz, pct = Math.max(.04, Math.min(.96, t / 90));
      const f = el.querySelector('.hero-fill'); if (f) { f.setAttribute('y', (80 - 66 * pct).toFixed(1)); f.setAttribute('height', (66 * pct).toFixed(1)); }
      const fl = el.querySelector('.hero-flames'); if (fl) fl.style.opacity = t > 45 ? 1 : .18;
      const g = el.querySelector('.hero-glow'); if (g) g.setAttribute('fill', s.alarm_ogrzewanie || s.dym_alarm ? 'url(#gl-err)' : 'url(#gl-ember)');
      const tx = el.querySelector('.hero-t'); if (tx) tx.textContent = Math.round(t) + '°';
    }
  };

  global.ILU = ILU;
})(window);
