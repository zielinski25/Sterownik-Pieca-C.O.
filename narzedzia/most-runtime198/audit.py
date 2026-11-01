from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;R=P.parents[1];F=R/'firmware-patch/v3.55.8-blokada-fs-po-starcie/main_centrala.cpp'
s=F.read_text();digest=hashlib.sha256(F.read_bytes()).hexdigest();assert digest=='3fb7510ae8cd0f52256e04bb25ac2b30a7489be422f5d0bf8e18ba0e153c7a31'
refs={}
for marker,call in [('HISTORIA-IO','historiaIo.trim('),('HISTORIA-SHORT','historiaIo.append('),('HISTORIA-LONG','historiaIo.append(')]:
 start=s.index(marker+'-BEGIN]');end=s.index(marker+'-END]',start);body=s[start:end];assert body.count(call)==1
 pos=body.index(call);assert 'xSemaphoreTake(logMutex' in body[:pos] and 'xSemaphoreGive(logMutex)' in body[pos:]
 refs[marker]={'call_line':s[:start+pos].count('\n')+1,'current_order':'logMutex -> Engine; prospective 198 adds Owner Lease inside Engine facade'}
assert 'static void historiaBladIO() { historiaStart.runtimeFault(); }' in s
assert 'static void historiaIoService() { serwisujTWDTJesliZarejestrowany(); vTaskDelay(1); }' in s
for k in ['historiaStartSys','historiaA1Sys','historiaSys']:assert k in s
r={'status':'PASS','scope':'three history transaction call sites, not whole-program lock audit','firmware_modified':False,'release_ready':False,'firmware_sha256':digest,'sites':refs,'remaining_raw_objects':['historiaStartSys (boot)','historiaA1Sys (A1 boot)','historiaSys (current runtime, not replaced)'],'result_contract':'retire confirmed prefix even if a later ready snapshot is false; do not use ready as future admission token'}
(P/'INTEGRACJA.json').write_text(json.dumps(r,indent=2)+'\n');print('PASS frozen firmware: three transaction call sites audited, NOT integrated')
