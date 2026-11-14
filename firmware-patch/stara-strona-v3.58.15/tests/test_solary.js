const fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(process.argv[2],'utf8');
const start=s.indexOf('var _solarNumerPobrania=0;'),end=s.indexOf('// [v3.58.15] Pogoda',start);
assert(start>0&&end>start);
class Element {
 constructor(tag){this.tag=tag;this.children=[];this._text='';this.className='';this.value='';}
 set textContent(v){this._text=String(v);this.children=[]} get textContent(){return this._text+this.children.map(c=>c.textContent).join(' ')}
 appendChild(c){this.children.push(c)}
}
const ids={solarDni:new Element('div'),solarStatus:new Element('div'),solarData:new Element('input')};
const pending=[];const env={Date,Number,Math,Array,String,Error,encodeURIComponent,NaN,
 wykrLiczba:v=>v==null||String(v).trim()===''?NaN:Number(v),
 document:{getElementById:id=>ids[id]||null,createElement:t=>new Element(t)},
 fetch:(url,opts)=>new Promise((resolve,reject)=>pending.push({url,opts,resolve,reject}))};
vm.createContext(env);vm.runInContext(s.slice(start,end),env);
assert(env.solarDataPoprawna('2024-02-29'));assert(!env.solarDataPoprawna('2023-02-29'));assert(!env.solarDataPoprawna('2025-99-01'));assert(!env.solarDataPoprawna('2026-1-1'));
const rec={schemaVersion:2,localDate:'2026-10-20',updatedAtUtc:'2026-10-20T12:00:00Z',forecast:{available:true,sumGhiKwhM2:3.25,expectedHours:24,hourCount:8,complete:false},actual:{channel:'t_panel',available:true,validSamples:5,expectedSampleCount:1440,simulatedSamples:2,invalidSamples:1,coverageRatio:0.5,firstC:30,lastC:35,deltaC:5,equivalent100lKwh:0.58,finalized:false}};
let container=new Element('div');env.solarPokazRekord(container,rec);let txt=container.textContent;
for(const t of ['3.25 kWh/m²','niepelna','5.0 °C','50.0%','0.58 kWh','NIE pomiar','niezamkniety'])assert(txt.includes(t),t+': '+txt);
container=new Element('div');env.solarPokazRekord(container,{...rec,schemaVersion:1});txt=container.textContent;assert(txt.includes('Stary lub nieznany'));assert(!txt.includes('5.0 °C'));assert(!txt.includes('0.58 kWh'));
container=new Element('div');env.solarPokazRekord(container,{...rec,actual:{...rec.actual,channel:'t_bojler'}});assert(container.textContent.includes('Stary lub nieznany'));
container=new Element('div');env.solarPokazRekord(container,{...rec,forecast:{...rec.forecast,sumGhiKwhM2:null,hourCount:0},actual:{...rec.actual,validSamples:1}});txt=container.textContent;assert(txt.includes('Prognoza GHI centrali: brak'));assert(txt.includes('mniej niz 2 probki'));assert(!txt.includes('0.58 kWh'));
container=new Element('div');env.solarPokazRekord(container,{...rec,actual:{...rec.actual,deltaC:0,equivalent100lKwh:0}});assert(container.textContent.includes('0.0 °C'));assert(container.textContent.includes('0.00 kWh'));
const flush=()=>new Promise(r=>setImmediate(r)); const answer=(req,days)=>req.resolve({ok:true,json:()=>Promise.resolve({ok:true,days})});
(async()=>{
 env.solarPobierz();assert.equal(pending[0].url,'/api/solar-archive');assert.equal(pending[0].opts.cache,'no-store');
 ids.solarData.value='2026-10-20';env.solarPobierz();assert.equal(pending[1].url,'/api/solar-archive?date=2026-10-20');
 answer(pending[1],[rec]);await flush();assert(ids.solarDni.textContent.includes('3.25 kWh/m²'));
 answer(pending[0],[]);await flush();assert(ids.solarDni.textContent.includes('3.25 kWh/m²'));
 env.solarPobierz();assert.equal(ids.solarDni.children.length,0);pending[2].reject(Error('offline'));await flush();assert(ids.solarStatus.textContent.includes('Nie udalo'));assert.equal(ids.solarDni.children.length,0);
 env.solarPobierz();ids.solarData.value='2026-02-31';env.solarPobierz();assert(ids.solarStatus.textContent.includes('Nieprawidlowa'));answer(pending[3],[rec]);await flush();assert.equal(ids.solarDni.children.length,0);
 console.log('OK: archiwum, stary kanal, braki/zero, data, bledy i wyscigi');
})().catch(e=>{console.error(e);process.exitCode=1});
