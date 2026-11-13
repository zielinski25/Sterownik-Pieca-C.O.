// Tests extract the real graph logic from the private firmware source.
const fs=require('fs'), vm=require('vm'), assert=require('assert');
const source=fs.readFileSync(process.argv[2], 'utf8');
const start=source.indexOf('function wykrLiczba(v)');
const end=source.indexOf('// Bazowa siatka/kolory/skalowanie',start);
assert(start>0 && end>start);
const drawn={}, elements={wykrInfo:{},wykrPompaInfo:{}};
const env={Number, NaN, Date, document:{getElementById:id=>elements[id]||null},
  wykrJestWlaczona:()=>true, drawChart:(id,L,T,series)=>drawn[id]={L,T,series}};
vm.createContext(env); vm.runInContext(source.slice(start,end),env);
const s=env.wykrParsujCsvKrotkie('A,1,2,3,4,5,6,7,1,1000,40,90,45,1700000000\nB,,2,3,4,5,6,7,,1000,40,90,45,1700000060');
assert.equal(s.pompa[0],1); assert(Number.isNaN(s.pompa[1])); assert(Number.isNaN(s.t_zewn[1]));
function long(pump){const r=['X'];for(let i=0;i<11;i++)r.push('1','0','2');r.push('1700000000'); if(pump!==undefined)r.push(pump);return r.join(',');}
assert.equal(env.wykrParsujCsvDlugie(long('0')).pompa[0],0);
assert(Number.isNaN(env.wykrParsujCsvDlugie(long('-1')).pompa[0]));
assert(Number.isNaN(env.wykrParsujCsvDlugie(long()).pompa[0]));
assert.deepEqual(Array.from(env.wykrIndeksyOkresu([0,99,100,101,200,201,NaN],100,200)),[2,3,4]);
let ps=env.wykrStatPompy([1,0,NaN,1,0,1],[1,2,3,4,10000,10001],360);
assert.deepEqual([ps.on,ps.off,ps.zmiany],[3,2,2]);
const now=Math.floor(Date.now()/1000); s.czas_unix=[now-100,now-10];
env._wykrTryb='6h'; env._wykrDaneKrotkie=s;env._wykrDaneDlugie=null;env.wykrRenderuj();
assert.equal(drawn['chart-pompa'].series[0].schodki,true);
assert.equal(drawn['chart-pompa'].series[0].data[0],1);
assert(Number.isNaN(drawn['chart-pompa'].series[0].data[1]));
assert.equal(drawn['chart-serwa'].series[0].data[0],90);
assert.equal(drawn['chart-cisnienie'].series[0].data[0],1000);
assert(elements.wykrInfo.textContent.includes('2 probek'));
console.log('OK: CSV historyczne, brak danych, zakresy, pompa, jednostki');
// Render the actual SVG routine in a tiny DOM; absent samples must not become 0.
const rendererStart=source.indexOf('function lokalnyWykresRysuj(st)');
const rendererEnd=source.indexOf('// [PROGNOZA-ETAP3',rendererStart);
assert(rendererStart>0 && rendererEnd>rendererStart);
const svg={closest:()=>null,parentElement:{clientWidth:340},setAttribute(){},style:{},innerHTML:''};
const renderEnv={Number,NaN,Infinity,Math,isFinite,Array,String,document:{getElementById:()=>svg},
  _wykrOpcje:{skala:'wspolna'},wykrLiczba:env.wykrLiczba,lokalnyWykresWidoczne:st=>st.labels.length,
  lokalnyWykresStart:()=>0,lokalnyWykresOdswiezInspector:()=>{}};
vm.createContext(renderEnv);vm.runInContext(source.slice(rendererStart,rendererEnd),renderEnv);
const st={id:'x',labels:['a','b','c'],times:[1,2,3],height:150,progDziuryS:360,series:[{data:[1,NaN,0],color:'#fff',schodki:true}]};
renderEnv.lokalnyWykresRysuj(st); assert(!svg.innerHTML.includes('polyline')); assert(!svg.innerHTML.includes('NaN'));
st.series[0].data=[1,0,0];renderEnv.lokalnyWykresRysuj(st);
const points=svg.innerHTML.match(/<polyline points="([^"]+)/)[1].split(' ');
assert.equal(points.length,5); assert.equal(points[0].split(',')[1],points[1].split(',')[1]);
console.log('OK: renderer braki i schodkowa pompa');
// Same renderer: actual presentation options, without modifying raw series.
const optionsStart=source.indexOf('function wykrMediana('),optionsEnd=source.indexOf('function wykrPelnyEkran(',optionsStart);
vm.runInContext(source.slice(optionsStart,optionsEnd),renderEnv);
const temp={id:'chart-piec',labels:['a','b','c','d','e','f','g'],times:[1,2,3,4,5,6,7],height:150,progDziuryS:10,series:[{data:[10,10,10,30,10,10,10],color:'#fff'}]};
renderEnv._wykrOpcje={skala:'normalna',filtr:true,wypelnienie:true,linia:'gladka'};
renderEnv.lokalnyWykresRysuj(temp);
assert(svg.innerHTML.includes('<path d='));assert(svg.innerHTML.includes('<polygon points='));assert(!svg.innerHTML.includes('NaN'));assert.equal(temp.series[0].data[3],30);
renderEnv._wykrOpcje.linia='schodki';renderEnv._wykrOpcje.wypelnienie=false;renderEnv.lokalnyWykresRysuj(temp);
assert(svg.innerHTML.includes('<polyline'));assert(!svg.innerHTML.includes('<polygon'));assert.equal(svg.innerHTML.match(/<polyline points="([^"]+)/)[1].split(' ').length,13);
console.log('OK: renderer normalizacja, filtr, wypelnienie, gladkie i schodki');
