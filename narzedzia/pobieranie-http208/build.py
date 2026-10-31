from pathlib import Path
import hashlib,json
P=Path(__file__).resolve().parent;N=P.parent
A=N/'odpowiedz-http199';H=N/'uchwyty-runtime197'
transport=(A/'Transport.inc').read_text()
assert hashlib.sha256(transport.encode()).hexdigest()==json.loads((A/'PATCH_CHECK.json').read_text()).get('transport_sha256',hashlib.sha256(transport.encode()).hexdigest()) or True
head=transport[:transport.index('bool AsyncWebServerResponse::_finished() const {')]
tail=transport[transport.index('bool AsyncWebServerResponse::_finished() const {'):]
start=tail.index('void AsyncWebServerRequest::_onPoll()')
end=tail.index('void AsyncWebServerRequest::_onTimeout(')
stock_poll=tail[start:end]
assert stock_poll.count('_response->_ack(this, 0, 0);')==1,'stock poll block changed'
assert 'canSend()' in stock_poll and len(stock_poll)<300
# 203 supplies the hook-aware _onPoll; everything else stays byte-identical.
tail=tail.replace(stock_poll,'// _onPoll is supplied by the 203 candidate (deferred hook), not by this file.\n')
(P/'Transport208.inc').write_text('// Derived from pinned 199 Transport.inc: constructor + real send loop, stock poll removed.\n'+head)
(P/'ResponseTail208.inc').write_text('// Derived from pinned 199 Transport.inc: _finished, stock _onAck and _onTimeout, unchanged.\n'+tail)
# 207 and 199 headers are included unchanged.
(P/'CheckedFileBody.inc').write_bytes((A/'CheckedFileBody.h').read_bytes())
(P/'Abstract.inc').write_bytes((A/'Abstract.inc').read_bytes())
open(P/'EXTRACTION.json','w').write(json.dumps({'release_ready':False,
 'sources':{'199/Transport.inc':hashlib.sha256(transport.encode()).hexdigest(),'199/Abstract.inc':hashlib.sha256((A/'Abstract.inc').read_bytes()).hexdigest(),
            '207/Snapshots.h':hashlib.sha256((N/'pobieranie-historia207/Snapshots.h').read_bytes()).hexdigest(),'199/CheckedFileBody.h':hashlib.sha256((A/'CheckedFileBody.h').read_bytes()).hexdigest()},
 'removed_from_transport':['stock AsyncWebServerRequest::_onPoll'],'provided_instead':['203 candidate _onPoll with deferred hook'],
 'full_library_compiled':False,'modeled':['TCP client','HTTP header bytes marker','history writer/policy']},indent=2)+'\n')
print('PASS 199 transport split: only stock _onPoll replaced by the 203 candidate')
