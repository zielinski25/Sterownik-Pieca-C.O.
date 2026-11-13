import ast, re, html.parser, sys
s=open(sys.argv[1], encoding='utf8').read()
s=s[s.index('html += F("<div id=\'tab-wykr\''):s.index('// koniec #tab-wykr')]
markup=''.join(ast.literal_eval(p) for p in re.findall(r'"(?:\\.|[^"\\])*"',s))
class Parser(html.parser.HTMLParser):
    def __init__(self): super().__init__(); self.ids={}
    def handle_starttag(self,tag,attrs):
        d=dict(attrs)
        if 'id' in d:
            assert d['id'] not in self.ids,d['id']
            self.ids[d['id']]=(tag,d)
p=Parser();p.feed(markup)
for c in ['piec','inne','serwa','pompa','cisnienie','wilgotnosc']:
    assert p.ids['chart-'+c][1].get('width')=='100%'
    assert p.ids['full-chart-'+c][1]['onclick']=="wykrPelnyEkran('chart-"+c+"')"
assert p.ids['wykrLinia'][1]['onchange']=="wykrUstawPrezentacje('linia',this.value)"
print('OK: kontrolki HTML, SVG, przyciski pełnego ekranu')
