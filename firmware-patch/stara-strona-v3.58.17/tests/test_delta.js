const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(process.argv[2],'utf8');
const d0=src.indexOf('function wykrLiczba(v)'), d1=src.indexOf('// Bazowa siatka/kolory/skalowanie',d0);
const l0=src.indexOf('var _wykrNumerPobrania = 0;'),l1=src.indexOf('// [v2.0.0, Wariant B]',l0);
assert(d0>0&&d1>d0&&l0>0&&l1>l0);
const nodes={wykrInfo:{},wykrPompaInfo:{},wykrDeltaInfo:{}};
const drawn={}; const pending=[];
let ctx={Number,NaN,Infinity,Date,Array,String,Math,Error,
 document:{getElementById:id=>nodes[id]||null},
 _wykrTryb:'dzien',_wykrDaneKrotkie:null,_wykrDaneDlugie:null,
 WYKR_CZUJNIKI:[['t_panel','Panel'],['t_zewn','Zewnatrz'],['t_bojler','Bojler'],['t_trociny','Trociny']],
 _wykrOpcje:{a:'t_panel',b:'t_zewn'},wykrJestWlaczona:()=>true,
 drawChart:(id,labels,times,series,h,gap)=>{drawn[id]={labels,times,series,gap}},
 fetch:url=>new Promise((resolve,reject)=>pending.push({url,resolve,reject}))};
vm.createContext(ctx); vm.runInContext(src.slice(d0,d1),ctx);vm.runInContext(src.slice(l0,l1),ctx);
const now=Math.floor(Date.now()/1000), short=[
 `a,10,20,30,31,25,50,12,1,1000,40,90,45,${now-120}`,
 `b,,21,30,31,25,30,12,0,1000,40,90,45,${now-60}`,
 `c,12,21,30,31,25,8,12,0,1000,40,90,45,${now-1}`].join('\n');
const one=ctx.wykrParsujCsvKrotkie(short);
ctx._wykrDaneKrotkie=one;ctx.wykrRenderuj();
let delta=drawn['chart-delta'];assert.equal(delta.series[0].data[0],40);assert(Number.isNaN(delta.series[0].data[1]));assert.equal(delta.series[0].data[2],-4);assert(nodes.wykrDeltaInfo.textContent.includes('2/3'));
ctx._wykrOpcje={a:'t_zewn',b:'t_panel'};ctx.wykrRenderuj();assert.equal(drawn['chart-delta'].series[0].data[0],-40);
ctx._wykrOpcje={a:'t_trociny',b:'t_panel'};ctx.wykrRenderuj();assert(drawn['chart-delta'].series[0].data.every(Number.isNaN));assert(nodes.wykrDeltaInfo.textContent.includes('Trociny dostepne od wersji'));
ctx._wykrOpcje={a:'t_panel',b:'t_panel'};ctx.wykrRenderuj();assert.equal(drawn['chart-delta'].series[0].data[0],0);
// Historical long rows: includes older format (no pump column), values in same row.
let row=['x'];for(let i=0;i<11;i++)row.push(String(i+10),'0','50');row.push(String(now-10));
ctx._wykrDaneDlugie=ctx.wykrParsujCsvDlugie(row.join(','));ctx._wykrTryb='tydz';ctx._wykrOpcje={a:'t_panel',b:'t_zewn'};ctx.wykrRenderuj();
assert.equal(drawn['chart-delta'].series[0].data[0],5);
assert.equal(drawn['chart-delta'].gap,90*60);
// Independent same-family refreshes cannot paint a stale response.
const resolve=(req,csv)=>req.resolve({ok:true,text:()=>Promise.resolve(csv)});
const flush=()=>new Promise(r=>setImmediate(r));
(async()=>{
 ctx._wykrDaneKrotkie=null;ctx._wykrDaneDlugie=null;ctx._wykrTryb='dzien';ctx.loadCharts();
 assert.equal(drawn['chart-delta'].labels.length,0);assert.equal(pending[0].url,'/api/history');
 ctx._wykrTryb='tydz';ctx.loadCharts();assert.equal(pending[1].url,'/api/history/long');
 resolve(pending[1],row.join(','));await flush();assert.equal(drawn['chart-delta'].series[0].data[0],5);
 resolve(pending[0],short);await flush();assert.equal(drawn['chart-delta'].series[0].data[0],5);assert.equal(ctx._wykrDaneKrotkie,null);
 ctx._wykrTryb='dzien';ctx.loadCharts(); assert.equal(drawn['chart-delta'].labels.length,0);pending[2].reject(Error('network'));await flush();
 assert.equal(drawn['chart-delta'].labels.length,0);assert(nodes.wykrInfo.innerHTML.includes('Blad'));
 console.log('OK: ΔT, braki, ujemne/identyczne, CSV, zakresy, wyscigi i blad sieci');
})().catch(e=>{console.error(e);process.exitCode=1});
