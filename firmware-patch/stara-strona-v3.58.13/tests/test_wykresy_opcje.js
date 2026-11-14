const fs=require('fs'),vm=require('vm'),assert=require('assert');
const src=fs.readFileSync(process.argv[2],'utf8');
const a=src.indexOf('var WYKR_KATALOG =');
const b=src.indexOf('// Po zmianie wielkosci karty',a);
assert(a>=0 && b>a);
function Element(tag){this.tag=tag;this.children=[];this.style={};this.textContent='';this.value='';this.checked=false;}
Element.prototype.appendChild=function(x){this.children.push(x)};
const ids={wykrOpcje:new Element('div'),wykrSkala:new Element('select'),deltaCzujnikA:new Element('select'),deltaCzujnikB:new Element('select'),wykrDelta:new Element('div')};
let memory={}, renders=0;
const context={Date,Number,NaN,Math,JSON,Array,String,
  document:{getElementById:id=>ids[id]||null,createElement:t=>new Element(t),createTextNode:t=>({textContent:t})},
  localStorage:{getItem:k=>memory[k]||null,setItem:(k,v)=>{memory[k]=v}},
  _espStatusSnapshot:null,_espStatusSnapshotAt:0,ESP_STATUS_MAX_AGE_MS:6500,
  wykrRenderuj:()=>renders++,wykrLiczba:v=>v==null||String(v).trim()===''?NaN:Number(v),alert:()=>{}};
vm.createContext(context);vm.runInContext(src.slice(a,b),context);
context.wykrZaladujOpcje();context.wykrPokazOpcje();
assert.equal(ids.wykrOpcje.children.length,7);
let check=ids.wykrOpcje.children[0].children[0];check.checked=false;check.onchange();
assert.equal(context.wykrJestWlaczona('t_ogrz'),false);assert.equal(renders,1);
context.wykrUstawSkale('normalna'); assert.equal(context._wykrOpcje.skala,'normalna');
const beforePair=renders; context.wykrUstawDelta('a','t_trociny'); context.wykrUstawDelta('b','t_powrot'); assert.equal(renders,beforePair+2);
assert.equal(context._wykrOpcje.a,'t_trociny'); assert.equal(context._wykrOpcje.b,'t_powrot');
context._espStatusSnapshot={t_trociny:60,t_powrot:40};context._espStatusSnapshotAt=Date.now();context.wykrPokazDelta();
assert(ids.wykrDelta.textContent.includes('20.0 °C'));
context._espStatusSnapshot.t_powrot=null;context.wykrPokazDelta();assert(ids.wykrDelta.textContent.includes('Brak'));
context._espStatusSnapshot.t_powrot=40;context._espStatusSnapshotAt=Date.now()-7000;context.wykrPokazDelta();assert(ids.wykrDelta.textContent.includes('Brak'));
let n=Array.from(context.wykrNormalizujDane([10,15,20,null]));assert.deepEqual(n.slice(0,3),[0,50,100]);assert(Number.isNaN(n[3]));
assert.deepEqual(Array.from(context.wykrNormalizujDane([7,7])),[50,50]);
context._wykrOpcje={wylaczone:[],skala:'wspolna',a:'t_panel',b:'t_zewn'};context.wykrZaladujOpcje();
assert.equal(context._wykrOpcje.skala,'normalna');assert.equal(context._wykrOpcje.a,'t_trociny');assert.equal(context.wykrJestWlaczona('t_ogrz'),false);
console.log('OK: wybór serii, pamięć, normalizacja, A-B, brak i stary status');
const values=[10,10,10,30,10,10,10], times=[1,2,3,4,5,6,7];
assert.equal(context.wykrFiltrSzpilek(values,times,10)[3],10);
assert.equal(values[3],30); // raw samples unchanged
assert.equal(context.wykrFiltrSzpilek(values,[1,2,3,40,41,42,43],10)[3],30); // gap: not enough neighbors
assert(Number.isNaN(context.wykrFiltrSzpilek([10,10,10,null,10,10,10],times,10)[3]));
context.wykrUstawPrezentacje('filtr',true);context.wykrUstawPrezentacje('wypelnienie',true);context.wykrUstawPrezentacje('linia','schodki');
assert.equal(context._wykrOpcje.filtr,true);assert.equal(context._wykrOpcje.wypelnienie,true);assert.equal(context._wykrOpcje.linia,'schodki');
console.log('OK: filtr szpilek, przerwy, bez mutacji, ustawienia linii');
