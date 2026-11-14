import ast,re,html.parser,sys
s=open(sys.argv[1],encoding='utf-8').read()
start=s.index('  html += F("<div id=\'tab-solary\'')
end=s.index('// ── [PROGNOZA-ETAP3, PLAN_PROGNOZA.md] Zakladka "Pogoda" - WSZYSTKO z',start)
markup=''.join(ast.literal_eval(p) for p in re.findall(r'"(?:\\.|[^"\\])*"',s[start:end]))
class P(html.parser.HTMLParser):
 def __init__(self): super().__init__();self.ids={}
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if 'id' in d:
   assert d['id'] not in self.ids,d['id'];self.ids[d['id']]=(tag,d)
p=P();p.feed(markup)
for k in ['A','B']:
 assert p.ids['solarCzujnik'+k][0]=='select'
 assert p.ids['solarCzujnik'+k][1]['onchange']=="wykrUstawDelta('"+k.lower()+"',this.value)"
assert p.ids['solarData'][1]['type']=='date'
assert 'solarStatus' in p.ids and 'solarDni' in p.ids and 'solarDeltaBiezaca' in p.ids
assert 'data-tab="solary"' in s and "getElementById('tab-solary').classList.toggle" in s
print('OK: solarne HTML, zakladka, data, wybory')
