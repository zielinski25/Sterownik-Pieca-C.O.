#!/usr/bin/env python3
"""Składa wersję jedną-plikową: to-be-kartkowka.html (CSS + JS w środku).

Użycie:  python3 build.py
Efekt:  plik można otworzyć bezpośrednio (file://), wysłać mailem,
        skopiować na tablet/telefon — nie potrzebuje internetu ani folderu.
"""
from pathlib import Path
import re

here = Path(__file__).parent
html = (here / "index.html").read_text(encoding="utf-8")
css = (here / "styles.css").read_text(encoding="utf-8")
data = (here / "data.js").read_text(encoding="utf-8")
app = (here / "app.js").read_text(encoding="utf-8")

for name, code in (("data.js", data), ("app.js", app)):
    if "</script" in code.lower():
        raise SystemExit(f"Uwaga: {name} zawiera zamknięcie tagu script — trzeba je zeskrapować.")

html = re.sub(
    r'<link rel="stylesheet" href="styles\.css">',
    "<style>\n" + css + "\n</style>",
    html,
    count=1,
)
html = html.replace(
    '<script src="data.js"></script>',
    "<script>\n" + data + "\n</script>",
    1,
).replace(
    '<script src="app.js"></script>',
    "<script>\n" + app + "\n</script>",
    1,
)
html = html.replace(
    "<title>TO BE · Trening przed kartkówką 🇬🇧</title>",
    "<title>TO BE · Trening przed kartkówką 🇬🇧 (wersja 1 plik)</title>",
    1,
)

out = here / "to-be-kartkowka.html"
out.write_text(html, encoding="utf-8")
print(f"OK → {out.name} ({out.stat().st_size / 1024:.0f} KB, samodzielny plik HTML)")
