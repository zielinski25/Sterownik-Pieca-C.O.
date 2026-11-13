import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('gen', root / 'generuj.py')
gen = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gen)

class TestCSV(unittest.TestCase):
    def test_format_and_alignment(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'nowy'
            end = gen.stworz(path, 1794571200)
            with (path/'hist_short.csv').open() as f: short = list(csv.reader(f))
            with (path/'hist_long.csv').open() as f: long = list(csv.reader(f))
            self.assertEqual((len(short), len(long)), (1440, 169))
            self.assertTrue(all(len(row) == 14 for row in short))
            self.assertTrue(all(len(row) == 36 for row in long))
            self.assertTrue(all(int(b[13]) - int(a[13]) == 60 for a,b in zip(short,short[1:])))
            self.assertTrue(all(int(b[34]) - int(a[34]) == 3600 for a,b in zip(long,long[1:])))
            self.assertEqual(int(short[-1][13]), end)
            self.assertEqual(int(long[-1][34]), end)
            for a,b in zip(short[-1][1:8], (long[-1][i] for i in (1,4,7,10,13,16,19))):
                self.assertEqual(a,b)
            self.assertGreater(max(float(r[3]) for r in short)-min(float(r[3]) for r in short), 20)
            self.assertEqual(set(r[8] for r in short), {'0','1'})
            self.assertTrue(all(r[35] in {'0','1'} for r in long))
            with self.assertRaises(FileExistsError): gen.stworz(path,end)
            self.assertEqual((path/'hist_short.csv').read_text().count('\n'),1440)

if __name__ == '__main__': unittest.main()
