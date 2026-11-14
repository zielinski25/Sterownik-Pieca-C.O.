const fs=require('fs'),vm=require('vm'),assert=require('assert');
const s=fs.readFileSync(process.argv[2],'utf8');
const a=s.indexOf('// Historyczne komendy wykonuja sie w kolejce LAN.'),b=s.indexOf('// [v3.58.16 KONIEC DIAG]',a);assert(a>0&&b>a);
const nodes={};for(const k of ['histFsWynik','histFsOn','histFsOff','histFsClear','otaGithubWynik'])nodes[k]={textContent:'',disabled:false};
let req=[],yes=false;const env={Date,Number,Math,Array,String,Error,Promise,encodeURIComponent,setTimeout,clearTimeout,confirm:()=>yes,document:{getElementById:k=>nodes[k]||null},fetch:(url,opts)=>new Promise((resolve,reject)=>req.push({url,opts,resolve,reject})),odswiezDashboard:()=>{}};vm.createContext(env);vm.runInContext(s.slice(a,b),env);
const tick=()=>new Promise(r=>setImmediate(r));const reply=(r,j,ok=true)=>r.resolve({ok,status:ok?200:503,json:()=>Promise.resolve(j)});
(async()=>{
 env.histFsCzysc();assert.equal(req.length,0);yes=true;env.histFsCzysc();assert(nodes.histFsClear.disabled);reply(req[0],{token:'secret'});await tick();let cmd=req[1], id=new URLSearchParams(cmd.opts.body).get('id');assert.equal(new URLSearchParams(cmd.opts.body).get('cmd'),'histfs_czysc');assert.equal(cmd.opts.headers['X-Piec-Token'],'secret');reply(cmd,{accepted:true,cmdId:id,done:false});await tick();assert(nodes.histFsWynik.textContent.includes('oczekiwanie'));reply(req[2],{cmdId:id,done:true,ok:true});await tick();assert(nodes.histFsWynik.textContent.includes('nie jest potwierdzenie wykonania'));assert(!nodes.histFsClear.disabled);
 env.histFsSprawdz();reply(req[3],{ok:true,pending:true,done_marker:false});await tick();assert(nodes.histFsWynik.textContent.includes('zadania: JEST'));
 env.otaGithubZlec(false);reply(req[4],{token:'x'});await tick();id=new URLSearchParams(req[5].opts.body).get('id');assert.equal(new URLSearchParams(req[5].opts.body).get('cmd'),'update');reply(req[5],{accepted:true,cmdId:id,done:true,ok:false,error:'kolejka pelna'});await tick();assert(nodes.otaGithubWynik.textContent.includes('Nie potwierdzono'));
 env.otaGithubZlec(true);reply(req[6],{token:'x'});await tick();id=new URLSearchParams(req[7].opts.body).get('id');assert.equal(new URLSearchParams(req[7].opts.body).get('cmd'),'update_panel');reply(req[7],{accepted:true,cmdId:id,done:true,ok:true});await tick();assert(nodes.otaGithubWynik.textContent.includes('NIE jest potwierdzenie instalacji'));
 env.otaGithubSprawdz();reply(req[8],[{tag_name:'<img>',prerelease:true,assets:[{name:'firmware.bin'}]}]);await tick();assert(nodes.otaGithubWynik.textContent.includes('<img>'));assert(nodes.otaGithubWynik.textContent.includes('prerelease'));
 env.otaGithubSprawdz();req[9].reject(Error('offline'));await tick();assert(nodes.otaGithubWynik.textContent.includes('Nie sprawdzono'));
 env.histFsCzysc();reply(req[10],{token:'x'});await tick();req[11].reject(Error('offline'));await tick();assert(nodes.histFsWynik.textContent.includes('moze juz czekac'));assert(!nodes.histFsClear.disabled);
 console.log('OK: potwierdzenie, token, kolejka, ACK, bledy i brak sieci, znaczniki, GitHub i OTA');
})().catch(e=>{console.error(e);process.exitCode=1});
