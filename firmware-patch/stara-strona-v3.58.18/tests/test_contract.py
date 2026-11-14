import sys,re,ast,html.parser,subprocess,tempfile,pathlib
s=pathlib.Path(sys.argv[1]).read_text(encoding='utf8')
assert 'p == "/history_reset_a1.req.tmp"' in s
f=s[s.index('static bool historiaFsCzyscZadajBezBlokady() {'):s.index('bool historiaFsCzyscZadaj() {',s.index('static bool historiaFsCzyscZadajBezBlokady() {'))]
assert f.index('exists("/history_reset_a1.req")') < f.index('remove("/history_reset_a1.done")')
assert f.index('f.flush()') < f.index('rename("/history_reset_a1.req.tmp"')
for cmd in ['TG_UPDATE','TG_UPDATE_PANEL','TG_UPDATE_PANEL_ANULUJ']:
 assert 'if (!tgKolejkujAlboOdpowiedzBledem('+cmd+')) { wynikOk = false;' in s
assert 'AsyncURIMatcher::exact("/api/history/reset-status"), HTTP_GET' in s
assert 'if (!historiaWyczysc())' in s
x=s[s.index('  html += F("<div id=\'tab-wykr\''):s.index('// Czytanie gotowych rekordow z centrali')]
markup=''.join(ast.literal_eval(p) for p in re.findall(r'"(?:\\.|[^"\\])*"',x))
class Parser(html.parser.HTMLParser):
 def __init__(self):super().__init__();self.ids={}
 def handle_starttag(self,t,attrs):
  d=dict(attrs)
  if 'id' in d:
   assert d['id'] not in self.ids,d['id'];self.ids[d['id']]=d
p=Parser();p.feed(markup);assert p.ids['histFsClear']['onclick']=='histFsCzysc()'
# Check all embedded raw-string scripts without executing them.
for raw in re.findall(r'const char WIFI_STRONA_HTML_FOOTER\[\] PROGMEM = R"HTML\((.*?)\)HTML"',s,re.S):
 for js in re.findall(r'<script(?:\s[^>]*)?>(.*?)</script>',raw,re.S|re.I):
  with tempfile.NamedTemporaryFile(suffix='.js',mode='w',encoding='utf8') as out:
   out.write(js);out.flush();subprocess.run(['node','--check',out.name],check=True)
print('OK: kontrakty protokolu, markup, skladnia skryptow')
