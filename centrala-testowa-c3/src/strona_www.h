// strona_www.h
// ============================================================================
// CENTRALA TESTOWA NA ESP32-C3 — panel WWW (diagnostyka/sterowanie symulatorem)
// ============================================================================
// Po co osobna strona, skoro testujemy panel LCD? Bo na biurku trzeba miec
// czym:
//   - ustawic czas (C3 nie ma RTC ani internetu w trybie AP),
//   - wymusic alarm dymu / CO / panelu, zeby zobaczyc ekrany alarmowe LCD,
//   - przesunac klapa/syberek i sprawdzic odczyt na panelu,
//   - podejrzec, co dokladnie symulator wysyla (te same dane, ktore widzi LCD).
//
// Strona jest w jednym kawalku (bez CDN, bez zewnetrznych fontow) — dziala w
// sieci AP bez internetu. Wszystkie sciezki sa WZGLEDNE ("/api/..."), bo
// przegladarka klienta laczy sie z 192.168.4.1, nie z localhostem.
// ============================================================================
#pragma once

static const char STRONA_HTML[] = R"HTML(<!DOCTYPE html>
<html lang="pl"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Centrala testowa C3</title>
<style>
:root{--bg:#0b1220;--karta:#141f33;--karta2:#1b2942;--ramka:#24344f;
      --tekst:#d6e4f7;--dim:#7b93b3;--ogien:#ff9f43;--ok:#4ade80;--err:#ff4d6d;
      --woda:#38bdf8;--slonce:#ffd32a}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);
     color:var(--tekst);padding:12px 12px 40px;max-width:900px;margin:0 auto}
h1{font-size:1.05rem;letter-spacing:.02em;margin-bottom:2px}
h1 span{color:var(--ogien)}
.pod{color:var(--dim);font-size:.72rem;margin-bottom:12px}
.sek{background:var(--karta);border:1px solid var(--ramka);border-radius:12px;padding:10px 12px;margin-bottom:10px}
.sek h2{font-size:.72rem;text-transform:uppercase;letter-spacing:.08em;color:var(--dim);margin-bottom:8px}
.siatka{display:grid;grid-template-columns:repeat(auto-fit,minmax(96px,1fr));gap:8px}
.kaf{background:var(--karta2);border:1px solid var(--ramka);border-radius:10px;padding:8px}
.kaf .l{font-size:.6rem;text-transform:uppercase;letter-spacing:.06em;color:var(--dim)}
.kaf .w{font-size:1.15rem;font-weight:600;margin-top:2px}
.kaf .w small{font-size:.62rem;color:var(--dim);font-weight:400}
.kaf.alarm{border-color:var(--err);box-shadow:0 0 0 1px var(--err) inset}
.kaf.on{border-color:var(--ok)}
button{background:var(--karta2);color:var(--tekst);border:1px solid var(--ramka);
       border-radius:9px;padding:8px 10px;font-size:.8rem;cursor:pointer}
button:active{transform:scale(.98)}
button.aktywny{background:var(--ogien);color:#20140a;border-color:var(--ogien);font-weight:700}
button.alarm{background:var(--err);color:#2a0a10;border-color:var(--err);font-weight:700}
.rzad{display:flex;flex-wrap:wrap;gap:6px;align-items:center;margin-top:6px}
.rzad label{font-size:.72rem;color:var(--dim);min-width:150px}
input[type=range]{flex:1;min-width:140px;accent-color:var(--ogien)}
input[type=number]{width:82px;background:var(--karta2);color:var(--tekst);
      border:1px solid var(--ramka);border-radius:8px;padding:6px}
.stopka{color:var(--dim);font-size:.68rem;margin-top:14px;line-height:1.5}
a{color:var(--woda)}
</style></head><body>

<h1>Centrala <span>testowa</span> (ESP32-C3)</h1>
<div class="pod">Zastepnik prawdziwej centrali — symulowane dane dla panelu LCD.
 <span id="lacze">lacze...</span></div>

<div class="sek">
  <h2>Dane, ktore widzi panel LCD (/api/status)</h2>
  <div class="siatka">
    <div class="kaf"><div class="l">Piec C.O.</div><div class="w" id="t_ogrz">--</div></div>
    <div class="kaf"><div class="l">Piec sr.</div><div class="w" id="t_ogrz_sr">--</div></div>
    <div class="kaf"><div class="l">Bojler</div><div class="w" id="t_bojler">--</div></div>
    <div class="kaf"><div class="l">Zewn.</div><div class="w" id="t_zewn">--</div></div>
    <div class="kaf"><div class="l">Pokoj</div><div class="w" id="t_pokoj">--</div></div>
    <div class="kaf"><div class="l">Panel</div><div class="w" id="t_panel">--</div></div>
    <div class="kaf" id="kafDym"><div class="l">Dym (ADC)</div><div class="w" id="dym">--</div></div>
    <div class="kaf"><div class="l">Klapa</div><div class="w" id="klapa">--</div></div>
    <div class="kaf"><div class="l">Syberka</div><div class="w" id="syberka">--</div></div>
    <div class="kaf" id="kafPompa"><div class="l">Pompa</div><div class="w" id="pompa">--</div></div>
    <div class="kaf"><div class="l">Czas</div><div class="w" id="czas">--</div></div>
    <div class="kaf"><div class="l">Tryb serwa</div><div class="w" id="tryb">--</div></div>
  </div>
  <div class="stopka" id="statusInfo"></div>
</div>

<div class="sek">
  <h2>Pompa</h2>
  <div class="rzad">
    <button onclick="cmdPompa('auto')">AUTO</button>
    <button onclick="cmdPompa('wl')">WYMUS WL</button>
    <button onclick="cmdPompa('wyl')">WYMUS WYL</button>
    <span class="pod" id="pompaInfo" style="margin:0"></span>
  </div>
</div>

<div class="sek">
  <h2>Serwo (klapa / syberka)</h2>
  <div class="rzad">
    <button id="b1" onclick="cmdUst('trybSerwa',1)">AUTO (1)</button>
    <button id="b2" onclick="cmdUst('trybSerwa',2)">RECZNY (2)</button>
    <button id="b3" onclick="cmdUst('trybSerwa',3)">BEZPIECZNA (3)</button>
  </div>
  <div class="rzad"><label>Klapa [%]</label>
    <input type="range" id="sKlapa" min="0" max="100" value="0"
           oninput="pokazProc('klapa')" onchange="cmdServo('klapa')">
    <span class="pod" id="klapaProc" style="margin:0;min-width:42px">0 %</span></div>
  <div class="rzad"><label>Syberka [%]</label>
    <input type="range" id="sSyberka" min="0" max="100" value="0"
           oninput="pokazProc('syberka')" onchange="cmdServo('syberek')">
    <span class="pod" id="syberkaProc" style="margin:0;min-width:42px">0 %</span></div>
  <div class="stopka">Suwaki dzialaja tylko w trybie RECZNY (2) — tak samo jak na panelu LCD.
   Uwaga: /api/status podaje pozycje serw w STOPNIACH (klapa 0-180, syberka 0-90),
   a panel LCD dopisuje do nich znak „%” — przy pelnym otwarciu zobaczysz „180 %”.</div>
</div>

<div class="sek">
  <h2>Wymus awarie (test ekranow alarmowych LCD)</h2>
  <div class="rzad">
    <button id="tDym" onclick="cmdTest('dym')">ALARM DYMU</button>
    <button id="tCo" onclick="cmdTest('co')">ALARM CO</button>
    <button id="tPanel" onclick="cmdTest('panel')">ALARM PANELU</button>
    <button id="tDeszcz" onclick="cmdTest('deszcz')">DESZCZ</button>
    <button onclick="cmdTest('historia')">WYCZYSC HISTORIE</button>
  </div>
  <div class="stopka">Alarm dymu zapala sie po „dymCzasStabilizacji” sekundach nad progiem
   (domyslnie 20 s) i gasnie po „dymCzasOff” sekundach pod progiem — dokladnie jak w centrali.</div>
</div>

<div class="sek">
  <h2>Najwazniejsze ustawienia (/api/ustawienia-get)</h2>
  <div id="ustawienia"></div>
  <div class="stopka">Zapis idzie przez TEN SAM endpoint co z panelu LCD
   (/api/ustawienia-set) i jest trzymany w NVS — przetrwa restart C3.</div>
</div>

<div class="sek">
  <h2>Czas</h2>
  <div class="rzad"><button onclick="syncCzas()">USTAW CZAS Z PRZEGLADARKI</button>
   <span class="pod" id="czasInfo" style="margin:0"></span></div>
  <div class="stopka">C3 nie ma RTC ani internetu (dziala jako AP), wiec czas ustawia sie
   recznie — po kazdym wlaczeniu zasilania. Bez ustawionego czasu panel LCD pokazuje „--:--”
   i nie przelacza widoku Lato/Zima po miesiacu (dokladnie jak centrala bez RTC).</div>
</div>

<div class="sek">
  <h2>Diagnostyka</h2>
  <div class="stopka" id="diag">...</div>
  <div class="stopka"><a href="/api/diag">/api/diag</a> ·
   <a href="/api/status">/api/status</a> ·
   <a href="/api/ustawienia-get">/api/ustawienia-get</a> ·
   <a href="/api/history">/api/history</a> ·
   <a href="/api/history/long">/api/history/long</a></div>
</div>

<script>
function $(id){return document.getElementById(id);}
function fmt(v,d){return (typeof v==='number')?v.toFixed(d===undefined?1:d):'--';}

let ostatniStatus=null, ostatnieUstaw=null;

async function odswiezStatus(){
  try{
    const r=await fetch('/api/status',{cache:'no-store'});
    const d=await r.json();
    ostatniStatus=d;
    $('lacze').textContent='polaczono';
    $('t_ogrz').innerHTML=fmt(d.t_ogrz,1)+'<small> °C</small>';
    $('t_ogrz_sr').innerHTML=fmt(d.t_ogrz_sr,1)+'<small> °C</small>';
    $('t_bojler').innerHTML=fmt(d.t_bojler,1)+'<small> °C</small>';
    $('t_zewn').innerHTML=fmt(d.t_zewn,1)+'<small> °C</small>';
    $('t_pokoj').innerHTML=fmt(d.t_pokoj,1)+'<small> °C</small>';
    $('t_panel').innerHTML=fmt(d.t_panel,1)+'<small> °C</small>';
    $('dym').textContent=d.dym;
    $('klapa').innerHTML=d.klapa+'<small> °</small>';
    $('syberka').innerHTML=d.syberka+'<small> °</small>';
    $('pompa').textContent=d.pompa?'WL':'WYL';
    $('kafPompa').className='kaf'+(d.pompa?' on':'');
    $('kafDym').className='kaf'+(d.dym_alarm?' alarm':'');
    $('tryb').textContent=d.tryb_serwa==1?'AUTO':(d.tryb_serwa==2?'RECZNY':'BEZP.');
    $('czas').textContent=d.rtc_ok?
      String(d.godzina).padStart(2,'0')+':'+String(d.minuta).padStart(2,'0'):'--:--';
    $('pompaInfo').textContent=d.pompa_override_min>0?
      ('reczny jeszcze '+d.pompa_override_min+' min'):'automatyka';
    $('statusInfo').textContent='wifi='+d.wifi_mode+' ip='+d.ip+
      ' | przelew '+d.pompa_ms_od_zmiany+' ms | deszcz_stan='+d.deszcz_stan+
      ' | alarmy: dym='+(d.dym_alarm?1:0)+' co='+(d.alarm_ogrzewanie?1:0)+
      ' panel='+(d.alarm_panel?1:0);
    // podswietlenie przyciskow trybu serwa
    [1,2,3].forEach(n=>{const b=$('b'+n); if(b) b.className=(d.tryb_serwa===n)?'aktywny':'';});
    $('tDym').className=d.symulacja_dym?'alarm':'';
    $('tCo').className=d.symulacja_ogrz?'alarm':'';
    $('tPanel').className=d.symulacja_panel?'alarm':'';
    $('tDeszcz').className=d.deszcz_stan>0?'aktywny':'';
    // suwaki pokazuja pozycje tylko gdy nie przeciagamy
    if(document.activeElement!==$('sKlapa')){
      const proc=Math.round(d.klapa*100/180);
      $('sKlapa').value=proc; $('klapaProc').textContent=proc+' %';
    }
    if(document.activeElement!==$('sSyberka')){
      const proc=Math.round(d.syberka*100/90);
      $('sSyberka').value=proc; $('syberkaProc').textContent=proc+' %';
    }
  }catch(e){ $('lacze').textContent='BRAK POLACZENIA'; }
}

const POLA_USTAWIEN=[
  ['wybor','Strategia pompy (1 cykl / 2 temp / 3 auto)',1,3],
  ['czasOn','Pompa ON [min]',1,180],
  ['czasOff','Pompa OFF [min]',1,180],
  ['tempOn','Zal. pompe pow. [C]',0,100],
  ['tempOff','Wyl. pompe pon. [C]',0,100],
  ['tempZadServo','Temp. zadana serwo [C]',0,100],
  ['histerServo','Histereza serwo [C]',0,50],
  ['skokKlapy','Skok klapy [%]',1,100],
  ['skokSyberka','Skok syberka [%]',1,100],
  ['odchylTemp','Prog odchylenia [C]',1,50],
  ['mnoznik','Mnoznik korekty',1,10],
  ['progAlarmTemp','Prog alarmu temp. [C]',0,100],
  ['progAlarmDym','Prog alarmu dymu',31,4095],
  ['dymCzasOn','Czas WL. dymu [s]',20,600],
  ['dymCzasOff','Czas WYL. dymu [s]',10,600],
  ['dymCzasStabilizacji','Stabilizacja dymu [s]',5,590],
  ['antystopDni','Antystop co [dni]',1,45],
  ['mieszadloCzasOn','Mieszadlo ON [s]',1,255],
  ['mieszadloCzasOff','Mieszadlo OFF [min]',1,180]
];

function zbudujUstawienia(){
  const box=$('ustawienia');
  box.innerHTML='';
  POLA_USTAWIEN.forEach(([klucz,etykieta,min,max])=>{
    const r=document.createElement('div'); r.className='rzad';
    r.innerHTML='<label>'+etykieta+'</label>'+
      '<button onclick="krok(\''+klucz+'\',-1)">–</button>'+
      '<input type="number" id="w_'+klucz+'" min="'+min+'" max="'+max+'" step="1">'+
      '<button onclick="krok(\''+klucz+'\',1)">+</button>';
    box.appendChild(r);
  });
}

async function odswiezUstawienia(){
  try{
    const r=await fetch('/api/ustawienia-get',{cache:'no-store'});
    const d=await r.json(); ostatnieUstaw=d;
    POLA_USTAWIEN.forEach(([klucz])=>{const el=$('w_'+klucz); if(el) el.value=d[klucz];});
  }catch(e){}
}

async function cmdUst(pole,wartosc){
  await fetch('/api/ustawienia-set',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'pole='+encodeURIComponent(pole)+'&wartosc='+wartosc});
  odswiezUstawienia(); odswiezStatus();
}

function krok(klucz,kierunek){
  const el=$('w_'+klucz); if(!el) return;
  const v=parseInt(el.value||'0',10)+kierunek;
  cmdUst(klucz,v);
}

function pokazProc(os){
  const v=$(os==='klapa'?'sKlapa':'sSyberka').value;
  $(os==='klapa'?'klapaProc':'syberkaProc').textContent=v+' %';
}

async function cmdServo(os){
  const procent=$(os==='klapa'?'sKlapa':'sSyberka').value;
  const r=await fetch('/api/servo-manual',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},
    body:'os='+os+'&procent='+procent});
  if(!r.ok){
    const d=await r.json().catch(()=>({error:'?'}));
    alert('Centrala odrzucila: '+(d.error||r.status)+'\n\nWskazowka: wybierz tryb RECZNY (2).');
  }
  odswiezStatus();
}

async function cmdPompa(stan){
  await fetch('/api/pompa-manual',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'stan='+stan});
  odswiezStatus();
}

async function cmdTest(typ){
  if(typ==='historia'){
    await fetch('/api/test?typ=historia');
    return;
  }
  let stan='1';
  if(typ==='deszcz'){
    stan=String(((ostatniStatus&&ostatniStatus.deszcz_stan)||0)+1);
    if(stan==='3') stan='0';
  }else{
    const mapa={dym:'symulacja_dym',co:'symulacja_ogrz',panel:'symulacja_panel'};
    const klucz=mapa[typ];
    stan=(ostatniStatus&&ostatniStatus[klucz])?'0':'1';
  }
  await fetch('/api/test?typ='+typ+'&stan='+stan);
  odswiezStatus();
}

async function syncCzas(){
  const d=new Date();
  const p=new URLSearchParams({rok:d.getFullYear(),miesiac:d.getMonth()+1,
    dzien:d.getDate(),godzina:d.getHours(),minuta:d.getMinutes(),sekunda:d.getSeconds()});
  const r=await fetch('/api/rtc-sync?'+p.toString());
  const j=await r.json().catch(()=>({}));
  $('czasInfo').textContent=j.ok?'czas ustawiony':('blad: '+(j.blad||r.status));
  odswiezStatus();
}

async function odswiezDiag(){
  try{
    const r=await fetch('/api/diag',{cache:'no-store'}); const d=await r.json();
    $('diag').textContent='wersja '+d.wersja+' | uptime '+Math.floor(d.uptime_s/60)+
      ' min | heap '+Math.floor(d.heap_wolny/1024)+' KB (min '+Math.floor(d.heap_min/1024)+
      ' KB) | stacje AP '+d.stacje_ap+' | statusow '+d.statusy+
      ' | fw panelu '+(d.panel_fw||'-')+' | reset panelu '+(d.panel_reset||'-')+
      ' | historia '+d.hist_krotka+'/'+d.hist_dluga;
  }catch(e){}
}

zbudujUstawienia();
odswiezStatus(); odswiezUstawienia(); odswiezDiag(); syncCzas();
setInterval(odswiezStatus,2000);
setInterval(odswiezUstawienia,5000);
setInterval(odswiezDiag,10000);
</script>
</body></html>
)HTML";
