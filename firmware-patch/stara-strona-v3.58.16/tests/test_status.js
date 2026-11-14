const fs=require('fs'),vm=require('vm'),assert=require('assert');
let s=fs.readFileSync(process.argv[2],'utf8'),a=s.indexOf('var _dashSeq=0'),b=s.indexOf('function synchronizujCzasZPrzegladarki()',a);assert(a>0&&b>a);
let rq=[],saved=[],lost=0,banners=[],elements={pillStatus:{classList:{add(){},remove(){}}},pillTekst:{textContent:''}};
let env={Date,Array,Error,ESP_STATUS_MAX_AGE_MS:6500,fetch:()=>new Promise((resolve,reject)=>rq.push({resolve,reject})),espStatusZapamietaj:d=>saved.push(d),espStatusUtracony:()=>lost++,diagOdswiez:()=>{},odswiezKafelki:()=>{},dpBaner:(...x)=>banners.push(x),fmt:v=>v,document:{getElementById:id=>elements[id]||null}};vm.createContext(env);vm.runInContext(s.slice(a,b),env);
const done=(r,d)=>r.resolve({ok:true,json:()=>Promise.resolve(d)});const flush=()=>new Promise(r=>setImmediate(r));
(async()=>{env.odswiezDashboard();env.odswiezDashboard();done(rq[1],{wifi_mode:'STA',ip:'nowy'});await flush();done(rq[0],{wifi_mode:'STA',ip:'stary'});await flush();assert.equal(saved.length,1);assert.equal(saved[0].ip,'nowy');
 env.odswiezDashboard();env.odswiezDashboard();done(rq[3],{wifi_mode:'STA',ip:'jeszcze nowszy'});await flush();rq[2].reject(Error('spozniony blad'));await flush();assert.equal(lost,0);
 env.odswiezDashboard();rq[4].resolve({ok:false,status:500,json:()=>Promise.resolve({})});await flush();assert.equal(lost,1);assert(banners.some(x=>x[1]==='offline'));
 console.log('OK: kolejnosc statusow, opoznione bledy i HTTP offline');})().catch(e=>{console.error(e);process.exitCode=1});
