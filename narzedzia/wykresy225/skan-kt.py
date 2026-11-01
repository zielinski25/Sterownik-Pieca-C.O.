import os, hashlib


def skanuj(s):
    i = 0
    n = len(s)
    line = 1
    stack = []
    w_k = False
    while i < n:
        c = s[i]
        if c == '\n':
            line += 1
            i += 1
            continue
        if w_k:
            if c == '*' and i + 1 < n and s[i + 1] == '/':
                w_k = False
                i += 2
                continue
            i += 1
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '/':
            while i < n and s[i] != '\n':
                i += 1
            continue
        if c == '/' and i + 1 < n and s[i + 1] == '*':
            w_k = True
            i += 2
            continue
        if c == '*' and i + 1 < n and s[i + 1] == '/':
            return ("'*/' BEZ OTWARCIA", "wiersz %d" % line)
        if s.startswith('"""', i):
            i += 3
            while i < n and not s.startswith('"""', i):
                if s[i] == '\n':
                    line += 1
                i += 1
            i += 3
            continue
        if c == '"':
            i += 1
            while i < n:
                if s[i] == '\\':
                    i += 2
                    continue
                if s[i] == '"':
                    i += 1
                    break
                if s[i] == '\n':
                    line += 1
                i += 1
            continue
        if c == "'":
            i += 1
            while i < n:
                if s[i] == '\\':
                    i += 2
                    continue
                if s[i] == "'":
                    i += 1
                    break
                i += 1
            continue
        if c in '([{':
            stack.append((c, line))
        elif c in ')]}':
            if not stack:
                return ("NADMIAR", "%s w %d" % (c, line))
            o, l = stack.pop()
            if '([{'.index(o) != ')]}'.index(c):
                return ("NIEDOPASOWANIE", "%s@%d vs %s@%d" % (o, l, c, line))
        i += 1
    if w_k:
        return ("KOMENTARZ NIEZAMKNIETY", "")
    if stack:
        return ("NIEZAMKNIETE", str(stack[-3:]))
    return ("OK", "")


plik = []
for dp, dn, fn in os.walk("app/src"):
    for f in fn:
        if f.endswith(".kt"):
            plik.append(os.path.join(dp, f))

zle = 0
for p in sorted(plik):
    st, info = skanuj(open(p, encoding="utf-8").read())
    if st != "OK":
        print("  %s: %s -> %s" % (st, p, info))
        zle += 1
print("SKAN SKLADNI: %d plikow .kt, problemow: %d" % (len(plik), zle))
