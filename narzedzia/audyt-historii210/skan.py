"""Statyczny inwentarz dostepu do plikow historii w centrali v3.55.8.

Skaner nie uruchamia firmware'u. Czyta jeden plik zrodlowy i naglowki z paczki
v3.55.8, zamienia komentarze i literaly tekstowe na spacje (zachowujac numery linii),
a potem:
  1) odtwarza zakresy funkcji po nawiasach klamrowych (tylko przejscia 0 -> 1),
  2) znajduje wywolania open/remove/rename/exists z nazwa pliku,
  3) sprawdza, jakie prymitywy blokady wystepuja w ciele funkcji.

Wynik: INWENTARZ.json (dane) i RAPORT_LINIE.md (tabela do przegladu).
"""
from pathlib import Path
import hashlib, json, re

P = Path(__file__).resolve().parent
R = P.parents[1]
PKG = R / 'firmware-patch/v3.55.8-blokada-fs-po-starcie'
SRC = PKG / 'main_centrala.cpp'

HISTORY_MACROS = {'HISTORIA_PLIK': '/hist_short.csv', 'HISTORIA_DLUGA_PLIK': '/hist_long.csv'}
LOG_MACROS = {'LOG_FILE_A': '/log_a.txt', 'LOG_FILE_B': '/log_b.txt'}
ALL_MACROS = {**HISTORY_MACROS, **LOG_MACROS}
LOCK_PRIMITIVES = ['xSemaphoreTake', 'portENTER_CRITICAL', 'taskENTER_CRITICAL', 'std::lock_guard',
                   'noInterrupts', 'logMutex', 'historiaMux']
KEYWORDS = {'if', 'for', 'while', 'switch', 'else', 'do', 'struct', 'class', 'namespace', 'enum', 'union', 'extern'}


def mask(text: str) -> str:
    """Zamien komentarze i wnetrza literaly na spacje, zostawiajac numery linii."""
    out = list(text)
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '/' and i + 1 < n and text[i + 1] == '/':
            j = text.find('\n', i)
            j = n if j < 0 else j
            for k in range(i, j):
                out[k] = ' '
            i = j
        elif c == '/' and i + 1 < n and text[i + 1] == '*':
            j = text.find('*/', i + 2)
            j = n if j < 0 else j + 2
            for k in range(i, j):
                if out[k] != '\n':
                    out[k] = ' '
            i = j
        elif c == 'R' and i + 1 < n and text[i + 1] == '"':
            j = text.find('(', i + 2)
            if j < 0:
                i += 1
                continue
            delim = text[i + 2:j]
            end = text.find(')' + delim + '"', j)
            end = n if end < 0 else end + len(delim) + 2
            for k in range(i + 1, min(end, n)):
                if out[k] != '\n':
                    out[k] = ' '
            i = end
        elif c in '"\'':
            q = c
            j = i + 1
            while j < n:
                if text[j] == '\\':
                    j += 2
                    continue
                if text[j] == q:
                    break
                j += 1
            for k in range(i + 1, min(j, n)):
                if out[k] != '\n':
                    out[k] = ' '
            i = j + 1
        else:
            i += 1
    return ''.join(out)


def scopes(masked: str):
    """(name, start_line, end_line) dla kazdego bloku zaczynajacego sie na poziomie 0."""
    lines = masked.splitlines()
    depth = 0
    pending = []
    opened = None
    res = []
    for lineno, line in enumerate(lines, 1):
        if depth == 0:
            if line.strip():
                pending.append(line)
            before = 0
        else:
            before = depth
        depth += line.count('{') - line.count('}')
        if before == 0 and depth > 0 and opened is None:
            sig = ' '.join(pending).split('{', 1)[0].strip()
            name = None
            if '(' in sig:
                head = sig.split('(')[0].strip()
                token = head.split()[-1] if head.split() else ''
                token = token.split('::')[-1]
                if token and token not in KEYWORDS and not sig.startswith('#'):
                    name = token
            opened = (name or '<blok>', lineno)
            pending = []
        if depth <= 0:
            depth = 0
            pending = []
            if opened:
                res.append((opened[0], opened[1], lineno))
                opened = None
    return res


CALL = re.compile(r'\b(open|remove|rename|exists)\s*\(')
PATH_ARG = re.compile(r'(HISTORIA_PLIK|HISTORIA_DLUGA_PLIK|LOG_FILE_A|LOG_FILE_B|"[^"]*\.(?:csv|txt)")')


def scan(path: Path):
    raw = path.read_text(errors='replace')
    masked = mask(raw)
    raw_lines = raw.splitlines()
    masked_lines = masked.splitlines()
    scope_list = scopes(masked)
    usages = []
    for lineno, line in enumerate(masked_lines, 1):
        for m in CALL.finditer(line):
            tail = line[m.end():m.end() + 160]
            pm = PATH_ARG.search(tail)
            if not pm:
                continue
            mode = re.search(r'"\s*(\w)\s*"', tail)
            enclosing = [s for s in scope_list if s[1] <= lineno <= s[2]]
            fn = enclosing[-1] if enclosing else ('<global>', lineno, lineno)
            body = '\n'.join(masked_lines[fn[1] - 1:fn[2]])
            locks = [p for p in LOCK_PRIMITIVES if p in body]
            usages.append({
                'line': lineno,
                'call': m.group(1),
                'mode': mode.group(1) if mode else '',
                'path': pm.group(1),
                'function': fn[0],
                'function_lines': [fn[1], fn[2]],
                'locks_in_function': locks,
                'source': raw_lines[lineno - 1].strip()[:200],
            })
    return {
        'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
        'lines': len(raw_lines),
        'functions': len(scope_list),
        'scopes': [{'name': n, 'start': a, 'end': b} for n, a, b in scope_list],
        'usages': usages,
    }


def main():
    src = scan(SRC)
    headers = {}
    for h in sorted(PKG.glob('*.h')):
        headers[h.name] = {'sha256': hashlib.sha256(h.read_bytes()).hexdigest(), 'lines': len(h.read_text().splitlines())}
    hist = [u for u in src['usages'] if u['path'] in HISTORY_MACROS]
    logs = [u for u in src['usages'] if u['path'] in LOG_MACROS]
    by_fn = {}
    for u in hist:
        by_fn.setdefault(u['function'], []).append(u)
    summary = {
        'history_calls': len(hist),
        'log_calls': len(logs),
        'history_functions': sorted(by_fn),
        'history_readers': sorted({u['function'] for u in hist if u['mode'] == 'r' or u['call'] == 'exists'}),
        'history_writers': sorted({u['function'] for u in hist if u['mode'] in ('a', 'w')}),
        'history_functions_without_lock': sorted({u['function'] for u in hist if not u['locks_in_function']}),
    }
    data = {'release_ready': False, 'status': 'SCAN', 'source': str(SRC.relative_to(R)), 'source_scan': src,
            'headers': headers, 'summary': summary}
    (P / 'INWENTARZ.json').write_text(json.dumps(data, indent=2) + '\n')

    rows = ['| linia | funkcja | wywolanie | tryb | plik | blokady w funkcji |', '|---|---|---|---|---|---|']
    for u in sorted(hist, key=lambda x: x['line']):
        rows.append(f"| {u['line']} | `{u['function']}` | {u['call']} | {u['mode'] or '-'} | `{u['path']}` | "
                    f"{', '.join(u['locks_in_function']) or 'BRAK'} |")
    (P / 'RAPORT_LINIE.md').write_text('# Dostep do plikow historii w v3.55.8\n\n' + '\n'.join(rows) + '\n')
    print(f"PASS scan functions={src['functions']} history_calls={len(hist)} log_calls={len(logs)} "
          f"functions={len(by_fn)} without_lock={len(summary['history_functions_without_lock'])}")


if __name__ == '__main__':
    main()
