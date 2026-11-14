const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(process.argv[2],'utf8'),a=src.indexOf('function wykrLiczba(v)'),b=src.indexOf('// Bazowa siatka/kolory/skalowanie',a);assert(a>0&&b>a);
let drawn={};const now=Math.floor(Date.now()/1000);const env={Number,NaN,Date,document:{getElementById:()=>null},WYKR_CZUJNIKI:[],_wykrOpcje:{a:'t_trociny',b:'t_panel'},wykrJestWlaczona:()=>true,drawChart:(id,L,T,series)=>drawn[id]=series};vm.createContext(env);vm.runInContext(src.slice(a,b),env);
let base=`A,1,2,3,4,5,6,7,1,1000,40,90,45,${now-60}`;
let d=env.wykrParsujCsvKrotkie([base,base+',0.0,0,1184,128',base+',-1.2,4095,1184,1024',base+',99,1,0,0',base+',1,2',base+',,0,1184,0'].join('\n'));
assert(Number.isNaN(d.t_trociny[0])&&Number.isNaN(d.dym[0]));assert.equal(env.wykrPochodzenie(d,0,'t_panel'),0);
assert.equal(d.t_trociny[1],0);assert.equal(d.dym[1],0);assert.equal(env.wykrPochodzenie(d,1,'t_trociny'),2);assert.equal(env.wykrPochodzenie(d,1,'t_panel'),1);assert.equal(env.wykrPochodzenie(d,1,'dym'),1);
assert.equal(d.t_trociny[2],-1.2);assert.equal(d.dym[2],4095);assert.equal(env.wykrPochodzenie(d,2,'dym'),2);
for(let i=3;i<6;i++){assert(Number.isNaN(d.t_trociny[i]));assert(Number.isNaN(d.dym[i]));assert.equal(d.origin_known[i],0)}
function long(){let x=['A'];for(let i=0;i<11;i++)x.push('1','1','1');x.push(String(now-60));return x.join(',')}
let longData=env.wykrParsujCsvDlugie([long(),long()+',1',long()+',0,12.1,123,1152,1024'].join('\n'));
assert(Number.isNaN(longData.dym[0]));assert(Number.isNaN(longData.pompa[0]));assert.equal(longData.pompa[1],1);assert.equal(longData.t_trociny[2],12.1);assert.equal(longData.dym[2],123);assert.equal(env.wykrPochodzenie(longData,2,'dym'),2);
env._wykrTryb='dzien';env._wykrDaneKrotkie=d;env._wykrDaneDlugie=longData;env.wykrRenderuj();assert.equal(drawn['chart-dym'][0].data[1],0);assert.equal(drawn['chart-dym'][0].origin[2],2);assert.equal(drawn['chart-piec'][3].origin[1],2);assert.equal(drawn['chart-delta'][0].origin[1],2);assert.equal(drawn['chart-delta'][0].origin[0],0);
const start=src.indexOf('function lokalnyWykresRysuj(st)'),end=src.indexOf('// [PROGNOZA-ETAP3',start);let svg={closest:()=>null,parentElement:{clientWidth:340},setAttribute(){},style:{},innerHTML:''};const r={Number,NaN,Infinity,Math,isFinite,Array,String,document:{getElementById:()=>svg},_wykrOpcje:{skala:'wspolna'},wykrLiczba:env.wykrLiczba,lokalnyWykresWidoczne:st=>st.labels.length,lokalnyWykresStart:()=>0,lokalnyWykresOdswiezInspector:()=>{}};vm.createContext(r);vm.runInContext(src.slice(start,end),r);
let st={id:'chart-dym',labels:['a','b','c'],times:[1,2,3],height:150,series:[{data:[0,50,60],origin:[0,2,1],color:'#aaa'}]};r.lokalnyWykresRysuj(st);assert(svg.innerHTML.includes('r="3.5"'));assert(!svg.innerHTML.includes('NaN'));
console.log('OK: CSV stare/nowe/bledne, maski symulacji, trociny, dym, delta i znaczniki SVG');
