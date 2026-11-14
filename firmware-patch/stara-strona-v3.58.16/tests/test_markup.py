import ast,re,html.parser,sys
s=open(sys.argv[1],encoding='utf8').read()
a=s.index('  html += F("<div id=\'tab-wykr\'');b=s.index('// Czytanie gotowych rekordow z centrali',a)
markup=''.join(ast.literal_eval(p) for p in re.findall(r'"(?:\\.|[^"\\])*"',s[a:b]))
class P(html.parser.HTMLParser):
 def __init__(self): super().__init__();self.ids={}
 def handle_starttag(self,tag,attrs):
  d=dict(attrs)
  if 'id' in d:
   assert d['id'] not in self.ids,d['id'];self.ids[d['id']]=d
p=P();p.feed(markup)
for id in ['diag-polaczenie','diag-pompa','diag-serwa','diag-alarm','diag-czujniki','diag-sym','diag-hist','histFsStan','histFsWynik','histFsOn','histFsOff']:assert id in p.ids,id
assert p.ids['histFsOn']['onclick']=='histFsUstaw(true)'
assert p.ids['histFsOff']['onclick']=='histFsUstaw(false)'
assert 'Czyszczenie obecnie zablokowane' in markup
assert 'wynikOk = nvs;' in s and 'zmieniono RAM, blad zapisu NVS' in s
print('OK: kontrolki historii i diagnostyki, blokada czyszczenia, kontrakt bledu NVS')
