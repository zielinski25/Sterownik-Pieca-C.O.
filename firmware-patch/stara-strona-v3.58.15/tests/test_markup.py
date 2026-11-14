import ast,re,html.parser,sys
s=open(sys.argv[1],encoding='utf8').read()
start=s.index('  html += F("<div id=\'tab-solary\'')
end=s.index('// ── ZAKLADKA "Logi"',start)
markup=''.join(ast.literal_eval(p) for p in re.findall(r'"(?:\\.|[^"\\])*"',s[start:end]))
class P(html.parser.HTMLParser):
 def __init__(self):super().__init__();self.ids={};self.actions=[]
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if 'id' in d:
   assert d['id'] not in self.ids,d['id'];self.ids[d['id']]=(tag,d)
  if 'onclick' in d:self.actions.append(d['onclick'])
p=P();p.feed(markup)
for id in ['pogExtStatus','pogExtTeraz','pogExtOpadPrawd','pogExtSlonce','pogExtOkresInfo','pogExtDni','pogExtSzczegoly','solarGhiDzis','solarPrognozaStatus','solarPrognozaInfo','solarPrognozaGodziny','solarDni']:
 assert id in p.ids,id
assert p.ids['pogExtGodzina'][1]['onchange']=='pogExtSzczegoly()'
assert p.ids['solarPrognozaDzien'][1]['onchange']=='pogExtSolarGodziny()'
for period in ['24h','3d','7d','14d']:assert "pogExtUstawOkres('"+period+"')" in p.actions
assert p.actions.count('pogExtPobierz(true)')==2
for existing in ['pog-temp-max','chart-pog-temp','pog-naslonecznienie']:assert existing in p.ids
print('OK: HTML nowej pogody, solarow i starej prognozy')
