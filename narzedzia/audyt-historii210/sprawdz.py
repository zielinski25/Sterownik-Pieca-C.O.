"""Kontrola inwentarza etapu 210. Kazde twierdzenie raportu jest tu sprawdzane maszynowo.

Uruchomienie: python3 -B narzedzia/audyt-historii210/sprawdz.py
Kod wyjscia 0 = wszystkie twierdzenia potwierdzone dla audytowanej wersji.
Zmiana firmware'u bez aktualizacji raportu WYWALA ten test - to jest celowe.
"""
from pathlib import Path
import hashlib, json, re

P = Path(__file__).resolve().parent
R = P.parents[1]
PKG = R / 'firmware-patch/v3.55.8-blokada-fs-po-starcie'
SRC = PKG / 'main_centrala.cpp'
sha = lambda b: hashlib.sha256(b).hexdigest()

checks = []


def ok(name, condition, detail=''):
    checks.append({'name': name, 'ok': bool(condition), 'detail': detail})
    if not condition:
        raise AssertionError(f'{name}: {detail}')


# 1. Wersja pliku jest ta, ktora opisuje raport (sumy z paczki v3.55.8).
sums = {}
for line in (PKG / 'SHA256SUMS.txt').read_text().splitlines():
    if line.strip():
        h, n = line.split()
        sums[n] = h
ok('main_centrala.cpp zgadza sie z SHA256SUMS.txt', sums.get('main_centrala.cpp') == sha(SRC.read_bytes()),
   sums.get('main_centrala.cpp', '')[:16])
for name, h in sums.items():
    f = PKG / name
    if f.exists() and name.endswith('.h'):
        ok(f'{name} zgodny z SHA256SUMS.txt', sha(f.read_bytes()) == h)

# 2. Inwentarz ze skanera jest aktualny (skaner uruchamiany tutaj od nowa).
import subprocess
subprocess.run(['python3', '-B', str(P / 'skan.py')], check=True, capture_output=True)
inv = json.loads((P / 'INWENTARZ.json').read_text())
hist = [u for u in inv['source_scan']['usages'] if u['path'] in ('HISTORIA_PLIK', 'HISTORIA_DLUGA_PLIK')]
fns = sorted({u['function'] for u in hist})
ok('dokladnie 8 wywolan z nazwa pliku historii', len(hist) == 8, str(len(hist)))
ok('dokladnie 4 funkcje dotykaja plikow historii', fns == ['historiaDlugaWczytajZFS', 'historiaWczytajZFS',
                                                           'sendTelegramDocumentHistDluga', 'sendTelegramDocumentHistKrotka'],
   str(fns))
ok('zadna z tych funkcji nie bierze blokady', all(not u['locks_in_function'] for u in hist),
   str([(u['function'], u['locks_in_function']) for u in hist]))
ok('brak bezposredniego zapisu (tryb a/w) do plikow historii',
   all(u['mode'] not in ('a', 'w') for u in hist), str([(u['function'], u['mode']) for u in hist]))

# 3. Jedyny zapis idzie przez komponent historii, pod logMutex.
text = SRC.read_text(errors='replace')
flush = text[text.index('bool historiaFlushDoFS() {'):]
flush = flush[:flush.index('\n}\n')]
ok('zapis historii przez historiaIo.append', 'historiaIo.append(' in flush)
ok('zapis historii pod logMutex', 'xSemaphoreTake(logMutex' in flush and 'xSemaphoreGive(logMutex)' in flush)

# 4. Czyszczenie historii jest wylaczone, wiec rotacji w czasie pracy nie ma.
clear = text[text.index('bool historiaWyczysc() {'):]
clear = clear[:clear.index('\n}\n')]
ok('historiaWyczysc jest wylaczone', 'return false;' in clear and 'systemFS196' not in clear)

# 5. Endpointy /api/history* serwuja bufory RAM, nie pliki.
for ep, fn in [('/api/history', 'historiaZbudujCsv'), ('/api/history/long', 'historiaDlugaZbudujCsv')]:
    block = text[text.index(f'exact("{ep}")'):]
    block = block[:block.index('});')]
    ok(f'{ep} korzysta z {fn}', fn in block and 'systemFS196.open' not in block, block[:80])

# 6. Edytor plikow nie moze nadpisac historii ani zrobic zadan A1.
prot = text[text.index('static bool historiaChronionaSciezka(const String &p) {'):]
prot = prot[:prot.index('\n}\n')]
ok('sciezki historii chronione przed edytorem', 'p == HISTORIA_PLIK' in prot and 'p == HISTORIA_DLUGA_PLIK' in prot)
ok('kanal A1 chroniony przed edytorem', '/history_reset_a1.req' in prot and '/history_reset_a1.done' in prot)

# 7. Czytelnicy startowi biegną przed zadaniem zapisujacym.
boot_call = text.index('historiaWczytajZFS();       // [WYKRESY-FS]')
task_start = text.index('historiaTaskWakeSem = xSemaphoreCreateBinary();')
ok('czytanie przy starcie przed utworzeniem zadania historii', boot_call < task_start,
   f'{boot_call} < {task_start}')

# 8. Wywolania wysylaczy nie leza w bloku logMutex (okno +-25 linii wokol wywolania).
lines = text.splitlines()
for callee in ('sendTelegramDocumentHistKrotka();', 'sendTelegramDocumentHistDluga();'):
    idx = [i for i, l in enumerate(lines, 1) if callee in l and not l.strip().startswith('bool sendTelegramDocument')]
    ok(f'{callee} ma dokladnie jedno wywolanie', len(idx) == 1, str(idx))
    window = '\n'.join(lines[max(0, idx[0] - 26):idx[0] + 25])
    ok(f'{callee} wywolywane bez logMutex w oknie', 'xSemaphoreTake(logMutex' not in window)

# 9. Wysylka dokumentow czyta plik bez blokady i bez zapinki (to jest ryzyko z raportu).
for fn_name in ('sendTelegramDocumentHistKrotka', 'sendTelegramDocumentHistDluga'):
    body = text[text.index(f'bool {fn_name}()'):]
    body = body[:body.index('\n}\n')]
    ok(f'{fn_name} otwiera plik bez blokady i bez zapinki',
       'systemFS196.open(HISTORIA_' in body and 'xSemaphoreTake' not in body and 'download207' not in body)
    ok(f'{fn_name} oglasza dlugosc z cache', 'totalLogSize' in body)

result = {'release_ready': False, 'status': 'PASS', 'source_sha256': sha(SRC.read_bytes()),
          'checked': len(checks), 'checks': checks}
(P / 'KONTROLA.json').write_text(json.dumps(result, indent=2) + '\n')
print(f'PASS audyt210 verified_claims={len(checks)} source_sha256={result["source_sha256"][:16]}...')
