const { JSDOM, VirtualConsole } = require('jsdom');
const fs = require('fs');
const path = require('path');
const FILE = process.env.PLIK || path.join(__dirname, '..', '..', 'porownanie-wykresow.html');

const draw = { stroke:0, fillText:0, lineTo:0, fillRect:0 };
function fakeCtx(){
  const noop = () => {};
  return new Proxy({
    measureText: () => ({ width: 40 }),
    stroke: () => draw.stroke++,
    fillText: () => draw.fillText++,
    lineTo: () => draw.lineTo++,
    fillRect: () => draw.fillRect++,
    setLineDash: noop, save: noop, restore: noop, translate: noop, rotate: noop,
    drawImage: noop, setTransform: noop, clearRect: noop, beginPath: noop, moveTo: noop,
    fill: noop, closePath: noop, arc: noop, rect: noop, canvas: null
  }, { get: (t,k) => (k in t ? t[k] : () => {}) });
}

function netMock(cfg, calls){
  return async (url, init) => {
    calls.push(String(url));
    if (String(url).includes('securetoken')){
      calls.refresh = (calls.refresh||0)+1;
      if (cfg.refreshFails) return { ok:false, status:400, json: async()=>({error:{message:'INVALID_REFRESH_TOKEN'}}) };
      return { ok:true, status:200, json: async()=>({ id_token:'IDT', refresh_token:'RT2', expires_in:'3600' }) };
    }
    if (String(url).includes('identitytoolkit')){
      calls.login = (calls.login||0)+1;
      if (cfg.loginFails) return { ok:false, status:400, json: async()=>({error:{message:'INVALID_PASSWORD'}}) };
      return { ok:true, status:200, json: async()=>({ idToken:'IDT', refreshToken:'RT2', expiresIn:'3600' }) };
    }
    if (String(url).includes('firebasedatabase')){
      calls.db = (calls.db||0)+1;
      const m = String(url).match(/v1\/(\d{4})\/(\d{2})\/(\d{2})/);
      const ts = m ? Date.UTC(+m[1], +m[2]-1, +m[3], 12)/1000 : 0;
      const body = { r1:{ ts, a:[100,200,650,640,450,-50,210,50,600,10130,300], k:90, s:45, state:1, q:0 } };
      return { ok:true, status:200, text: async()=>JSON.stringify(body) };
    }
    return { ok:false, status:500, text: async()=>'' };
  };
}

async function load(cfg){
  const errors = [];
  const calls = [];
  const vc = new VirtualConsole();
  vc.on('jsdomError', e => errors.push('jsdomError: ' + e.message));
  vc.on('error', e => errors.push('console.error: ' + e));
  vc.on('warn', () => {});
  const dom = await JSDOM.fromFile(FILE, {
    runScripts:'dangerously', pretendToBeVisual:true, url:'https://test.local/porownanie.html',
    virtualConsole: vc,
    beforeParse(win){
      win.HTMLCanvasElement.prototype.getContext = () => fakeCtx();
      win.HTMLCanvasElement.prototype.toDataURL = () => 'data:image/png;base64,AAA';
      win.fetch = netMock(cfg, calls);
      win.alert = m => { errors.push('alert: ' + m); };
      Object.keys(cfg.localStorage || {}).forEach(k => win.localStorage.setItem(k, cfg.localStorage[k]));
    }
  });
  await new Promise(r => setTimeout(r, 400));
  const w = dom.window;
  return { dom, w, doc: w.document, errors, calls, st: () => w.eval('state') };
}

(async () => {
  let ok=0, bad=0;
  const A=(n,c,x)=>{ if(c){ok++;console.log('  OK   '+n);} else {bad++;console.log('  FAIL '+n+(x!==undefined?'  -> '+x:''));} };

  console.log('\n[1] DOM + start bez zapamiętanej sesji');
  const r1 = await load({});
  {
    const d = r1.doc;
    // każdy element, którego szuka skrypt
    const ids = ['fbEmail','fbPass','fbKey','rememberPass','btnLogout','credBox','authLine','fbFrom','fbTo','btnFb','btnFbClear','fbBar','fbStatus','drop','fileIn','fileFmt','fileStatus','btnFileClear','chips','modeSeg','tolS','btnCsv','btnPng','charts','boxA','boxB','boxSingle','cvA','cvB','cvS','capA','capB','capSingle','capSingleInfo','statSamples','statDiff','statTable'];
    const missing = ids.filter(i => !d.getElementById(i));
    A('wszystkie elementy istnieją ('+ids.length+')', missing.length===0, missing.join(','));
    A('brak błędów skryptu', r1.errors.length===0, r1.errors.slice(0,3).join(' | '));
    A('authLine ostrzega o haśle', /wpisz hasło/i.test(d.getElementById('authLine').textContent), d.getElementById('authLine').textContent);
    A('sekcja konta rozwinięta', d.getElementById('credBox').open === true);
    A('brak zapytań sieciowych', r1.calls.length===0, r1.calls.join(','));
    A('checkbox zapamiętaj zaznaczony', d.getElementById('rememberPass').checked === true);
    A('3 przyciski trybu', d.querySelectorAll('#modeSeg button').length===3);
    A('14 kanałów na liście', d.querySelectorAll('#chips button, #chips .chip, #chips label, #chips input').length>=14, d.getElementById('chips').innerHTML.length);
    A('data w polach zakresu', !!d.getElementById('fbFrom').value && !!d.getElementById('fbTo').value, d.getElementById('fbFrom').value + '..' + d.getElementById('fbTo').value);
    r1.dom.window.close();
  }

  console.log('\n[2] DOM + zapamiętana sesja → wchodzi sam i rysuje');
  const r2 = await load({ localStorage: { piec_fb_ref_token:'RT1', piec_fb_email:'piec_co_boot@akwarium.local' } });
  {
    const d = r2.doc;
    A('brak błędów skryptu', r2.errors.length===0, r2.errors.slice(0,3).join(' | '));
    A('odświeżył token', r2.calls.refresh === 1, r2.calls.refresh);
    A('nie logował hasłem', !r2.calls.login);
    A('pobrał bazę', r2.calls.db > 0, r2.calls.db);
    A('authLine: zalogowano', /zalogowano/i.test(d.getElementById('authLine').textContent), d.getElementById('authLine').textContent);
    A('fbStatus: OK', /^OK/.test(d.getElementById('fbStatus').textContent), d.getElementById('fbStatus').textContent);
    A('są próbki w A', r2.st().A.length > 0, r2.st().A.length);
    A('rysowanie poszło (stroke>0)', draw.stroke > 0, draw.stroke);
    A('statystyki wypełnione', d.getElementById('statSamples').textContent.length > 0 || d.getElementById('statSamples').innerHTML.length > 0);
    A('tabela kanałów wypełniona', d.getElementById('statTable').innerHTML.length > 100, d.getElementById('statTable').innerHTML.length);
    A('konto zwinięte po sukcesie', d.getElementById('credBox').open === false);
    A('refresh zapisany w localStorage', r2.w.localStorage.getItem('piec_fb_ref_token') === 'RT2', r2.w.localStorage.getItem('piec_fb_ref_token'));

    // przełączanie trybów klikaniem
    const btns = [...d.querySelectorAll('#modeSeg button')];
    let clickErr = null;
    for (const b of btns){
      try { b.click(); } catch(e){ clickErr = b.dataset.mode + ': ' + e.message; }
    }
    A('klikanie 3 trybów bez błędu', clickErr === null, clickErr);
    A('tryb ustawiony na ostatni', ['two','overlay','diff'].includes(r2.st().mode), r2.st().mode);
    A('canvas cvS rysowany w pojedynczym widoku', d.getElementById('boxSingle').classList.contains('hidden') === false || r2.st().mode === 'two', d.getElementById('boxSingle').className);

    // wylogowanie
    d.getElementById('btnLogout').click();
    A('wyloguj czyści token', r2.w.localStorage.getItem('piec_fb_ref_token') === null, r2.w.localStorage.getItem('piec_fb_ref_token'));
    A('wyloguj czyści hasło', r2.w.localStorage.getItem('piec_fb_password') === null);
    A('po wylogowaniu otwarte konto', d.getElementById('credBox').open === true);
    r2.dom.window.close();
  }

  console.log('\n[3] DOM + token martwy, hasło zapamiętane');
  const r3 = await load({ refreshFails:true, localStorage:{ piec_fb_ref_token:'RT_STARY', piec_fb_password:'Tajne123', piec_fb_email:'piec_co_boot@akwarium.local' } });
  {
    A('brak błędów skryptu', r3.errors.length===0, r3.errors.slice(0,3).join(' | '));
    A('próba odświeżenia', r3.calls.refresh === 1);
    A('logowanie hasłem', r3.calls.login === 1, r3.calls.login);
    A('dane pobrane', r3.st().A.length > 0, r3.st().A.length);
    r3.dom.window.close();
  }

  console.log('\n[4] DOM + złe hasło');
  const r4 = await load({ loginFails:true, localStorage:{ piec_fb_password:'Zle', piec_fb_email:'piec_co_boot@akwarium.local' } });
  {
    const d = r4.doc;
    A('komunikat o odrzuceniu', /Logowanie odrzucone/.test(d.getElementById('fbStatus').textContent), d.getElementById('fbStatus').textContent);
    A('konto rozwinięte', d.getElementById('credBox').open === true);
    A('brak danych', r4.st().A.length === 0);
    r4.dom.window.close();
  }

  console.log('\nWYNIK jsdom: ' + ok + ' OK, ' + bad + ' FAIL');
  process.exit(bad ? 1 : 0);
})();
