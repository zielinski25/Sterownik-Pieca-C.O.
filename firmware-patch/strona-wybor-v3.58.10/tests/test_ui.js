// Testuje skrypt z obu prawdziwych HTML w prywatnym pliku, bez uruchamiania ESP.
const fs=require('fs'), vm=require('vm'), assert=require('assert');
const src=fs.readFileSync(process.argv[2], 'utf8');
const scripts=[...src.matchAll(/<script>\n\/\/ Wybor otwartego widoku[\s\S]*?<\/script>/g)].map(m=>m[0].slice(8,-9));
assert.equal(scripts.length,2); assert.equal(scripts[0],scripts[1]);
async function run(path, fail) {
  const calls=[], events={}, alerts=[];
  const location={pathname:path,href:path};
  const win={location,addEventListener:(e,cb)=>events[e]=cb};
  const fetch=async (url,opts)=>{
    calls.push({url,opts});
    if(url.endsWith('-token')) return {ok:true,json:async()=>({token:'abc'})};
    return {ok:!fail,status:500};
  };
  vm.runInNewContext(scripts[0],{window:win,location,fetch,alert:s=>alerts.push(s),Promise,Error,encodeURIComponent});
  events.pageshow();
  await new Promise(r=>setImmediate(r));
  win.zmienWyborStrony('nowa','/nowa');
  await new Promise(r=>setImmediate(r));
  assert.equal(calls.length,4);
  assert.equal(calls[1].opts.body,'widok='+ (path==='/stara'?'stara':'nowa') +'&token=abc');
  assert.equal(calls[3].opts.body,'widok=nowa&token=abc');
  assert.equal(location.href,fail?path:'/nowa');
  assert.equal(alerts.length,fail?1:0);
}
(async()=>{await run('/stara',false); await run('/nowa',false); await run('/stara',true); console.log('OK: wybór strony JS, kolejka, błąd zapisu');})().catch(e=>{console.error(e);process.exit(1)});
