const fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(process.argv[2],'utf8');let a=s.indexOf('// [v3.58.15] Pogoda'), b=s.indexOf('// [v3.58.15 KONIEC POGODA EXT]',a);assert(a>0&&b>a);
class El{constructor(tag){this.tag=tag;this.children=[];this.value='';this._text='';this.className='';}set textContent(v){this._text=String(v);this.children=[];if(this.tag==='select')this.value='';}get textContent(){return this._text+this.children.map(c=>c.textContent).join(' ');}appendChild(c){this.children.push(c);if(this.tag==='select'&&!this.value)this.value=c.value;}}
const ids={};for(const id of ['pogExtStatus','solarPrognozaStatus','pogExtTeraz','pogExtOpadPrawd','pogExtSlonce','solarGhiDzis','pogExtOkresInfo','pogExtDni','pogExtSzczegoly','solarPrognozaInfo','solarPrognozaGodziny'])ids[id]=new El('div');for(const id of ['pogExtGodzina','solarPrognozaDzien'])ids[id]=new El('select');
let current=Date.parse('2026-03-29T12:30:00Z'); class Clock extends Date{static now(){return current;}}
const pending=[], env={Date:Clock,Intl,URLSearchParams,Number,Math,Array,String,Error,NaN,encodeURIComponent,setInterval:()=>{},document:{getElementById:k=>ids[k]||null,createElement:t=>new El(t)},fetch:(url,opts)=>new Promise((resolve,reject)=>pending.push({url,opts,resolve,reject}))};
vm.createContext(env);vm.runInContext(s.slice(a,b),env);
let noon=env.pogExtPozycja(Date.parse('2026-03-20T10:35:00Z')/1000);assert(noon.wysokosc>37&&noon.wysokosc<40);assert(noon.azymut>175&&noon.azymut<185);
let midnight=env.pogExtPozycja(Date.parse('2026-03-20T23:00:00Z')/1000);assert(midnight.wysokosc<0);
function fixture(){
 const first=Date.parse('2026-03-27T23:00:00Z')/1000,last=Date.parse('2026-04-11T22:00:00Z')/1000;
 let h={time:[],shortwave_radiation:[],temperature_2m:[],relative_humidity_2m:[],precipitation_probability:[],precipitation:[],surface_pressure:[],cloud_cover:[],wind_speed_10m:[],wind_gusts_10m:[],uv_index:[]};
 for(let t=first;t<=last;t+=3600){h.time.push(t);for(let k in h)if(k!=='time')h[k].push(k==='shortwave_radiation'?100:0);}
 let daily={time:[],sunrise:[],sunset:[],temperature_2m_min:[],temperature_2m_max:[]};
 for(let n=0;n<14;n++){let t=(Date.parse('2026-03-28T06:00:00Z')/1000)+n*86400;daily.time.push(t);daily.sunrise.push(t);daily.sunset.push(t+12*3600);daily.temperature_2m_min.push(0);daily.temperature_2m_max.push(10);}
 return {timezone:'Europe/Warsaw',hourly:h,daily,current:{time:current/1000,temperature_2m:0,relative_humidity_2m:50,precipitation:0,cloud_cover:20,surface_pressure:1000,wind_speed_10m:5,wind_gusts_10m:9,uv_index:0}};
}
let raw=fixture(), hours=env.pogExtGodziny(raw), result=env.pogExtGhiDzis(raw,hours,current/1000);
assert.equal(result.expected,13);assert.equal(result.good,13);assert(Math.abs(result.sum-1.3)<1e-9);assert(result.complete);
const i=raw.hourly.time.indexOf(Date.parse('2026-03-29T05:00:00Z')/1000);raw.hourly.shortwave_radiation[i]=null;result=env.pogExtGhiDzis(raw,hours,current/1000);assert.equal(result.good,12);assert(!result.complete);assert(Math.abs(result.sum-1.2)<1e-9);
assert.equal(env.pogExtKey(Date.parse('2026-03-28T23:00:00Z')/1000),'2026-03-29');
assert.throws(()=>env.pogExtGodziny({hourly:{time:[1,1]}}));assert(Number.isNaN(env.pogExtNum(null)));assert.equal(env.pogExtFormat(0,'°C',1),'0.0 °C');
let f=env.pogExtDni(raw);assert.equal(f.length,14);assert.equal(f[1].key,'2026-03-29');
env._pogExt={raw,hours};env._pogExtAt=current;env.pogExtPokaz();
assert(ids.pogExtTeraz.textContent.includes('0.0 °C'));assert(ids.solarGhiDzis.textContent.includes('niepelne dane'));assert.equal(ids.pogExtDni.children.length,2);
env.pogExtUstawOkres('7d');assert.equal(ids.pogExtDni.children.length,7);env.pogExtUstawOkres('14d');assert.equal(ids.pogExtDni.children.length,14);
ids.solarPrognozaDzien.value='2026-03-29';env.pogExtSolarGodziny();assert.equal(ids.solarPrognozaGodziny.children.length,23);assert(ids.solarPrognozaInfo.textContent.includes('23 przedzialow'));
ids.pogExtGodzina.value=String(hours[80].t);env.pogExtSzczegoly();assert(ids.pogExtSzczegoly.textContent.includes('promieniowanie GHI'));
// Repeat on autumn time change: 25 physical hours in a local day.
let autumn={hourly:{time:[],shortwave_radiation:[]}};for(let t=Date.parse('2026-10-24T22:00:00Z')/1000;t<=Date.parse('2026-10-25T23:00:00Z')/1000;t+=3600){autumn.hourly.time.push(t);autumn.hourly.shortwave_radiation.push(0);}
env._pogExt={raw:autumn,hours:env.pogExtGodziny(autumn)};ids.solarPrognozaDzien.value='2026-10-25';env.pogExtSolarGodziny();assert.equal(ids.solarPrognozaGodziny.children.length,25);
const flush=()=>new Promise(r=>setImmediate(r));
(async()=>{
 env.pogExtPobierz(true);env.pogExtPobierz(true);assert(pending[0].url.includes('forecast_days=14'));assert(pending[0].url.includes('timezone=Europe%2FWarsaw'));assert(pending[0].url.includes('latitude=51.066389'));
 pending[1].resolve({ok:true,json:()=>Promise.resolve(raw)});await flush();assert(env._pogExt);assert(ids.solarGhiDzis.textContent.includes('nie pomiar'));
 pending[0].reject(Error('old'));await flush();assert(env._pogExt);
 env.pogExtPobierz(true);pending[2].reject(Error('offline'));await flush();assert.equal(env._pogExt,null);assert(ids.pogExtStatus.textContent.includes('Brak danych'));assert.equal(ids.solarPrognozaGodziny.children.length,0);
 env.pogExtPobierz(true);pending[3].resolve({ok:true,json:()=>Promise.resolve({...raw,timezone:'GMT'})});await flush();assert.equal(env._pogExt,null);
 console.log('OK: pogoda, GHI, 23/25 h DST, dane brakujace, slonce, odswiezanie, bledy');
})().catch(e=>{console.error(e);process.exitCode=1});
