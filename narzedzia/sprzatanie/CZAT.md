# Czat z agentem — konsola i Telegram

Dwa sposoby, żeby **rozmawiać** z agentem zamiast wpisywać komendy. Oba używają
tego samego lokalnego modelu w Ollama.

| | `czat.py` (konsola) | `telegram_bot.py` (telefon) |
|---|---|---|
| Gdzie działa | laptop | laptop + telefon |
| Czy wychodzi do internetu | **nie** | **tak** (serwery Telegramu) |
| Szybkość na 4750U | 4–6 tok/s | 4–6 tok/s |
| Przenoszenie plików | wymaga zgody | wymaga „TAK" w Telegramie |
| Kto może pisać | Ty przy klawiaturze | **tylko Twój chat_id** |

---

## 1. Konsola (zalecane)

```powershell
python czat.py
```

Przykłady pytań:

```
Ty > ile mam wolnego?
Ty > co zajmuje miejsce?
Ty > posprzataj cache
Ty > ile jest w kwarantannie?
```

Z `--potwierdzaj` każda zmiana na dysku wymaga wpisania `tak`:

```powershell
python czat.py --potwierdzaj
```

Szybszy model (mniej inteligentny, ale 2× szybszy):

```powershell
python czat.py --model qwen2.5:3b-instruct-q4_K_M
```

---

## 2. Telegram — **przeczytaj ostrzeżenie**

Bot Telegram to jedyna część tego projektu, która **wysyła dane poza laptopa**:

```
Twoja wiadomość  →  serwer Telegramu  →  laptop  →  odpowiedź  →  serwer  →  telefon
```

Nie wychodzą Twoje pliki. Wychodzą **nazwy folderów i rozmiary**, o które pytasz.
Jeśli to dla Ciebie nieakceptowalne — zostań przy `czat.py`.

### Konfiguracja (10 minut)

**Krok 1.** W Telegramie napisz do `@BotFather`, wybierz `/newbot`,
podaj nazwę. Dostaniesz token postaci `123456789:AAH...`.

**Krok 2.** Napisz cokolwiek do swojego nowego bota. Potem otwórz w przeglądarce:

```
https://api.telegram.org/bot<TWOJ_TOKEN>/getUpdates
```

Znajdź `"chat":{"id":123456789}` — to Twój `chat_id`.

**Krok 3.** Uruchom:

```powershell
python telegram_bot.py --token "123456789:AAH..." --dozwoleni 123456789
```

**Krok 4.** Napisz do bota. Odpowie.

### Zabezpieczenia w kodzie

- odpowiada **tylko** na `chat_id` z listy `--dozwoleni`; obcy dostaje informację
  zwrotną ze swoim ID, ale żadnych danych
- przenoszenie plików: bot pisze, co chce zrobić, i czeka na `TAK`
- **duplikaty i duże pliki są zablokowane** dla czatu — przy 305 zdjęciach
  decyzja musi być Twoja, nie bota zesłanego z telefonu
- tylko jedna wiadomość obsługiwana naraz (16 GB RAM)

**Jeśli nie chcesz bota publicznie:** token trzymaj w pliku, nie w historii
PowerShella. Nie commituj go do repo — `.gitignore` już blokuje `google_drive/`,
ale token to nie plik; po prostu nie wklejaj go do GitHuba.

---

## 3. Jak to działa pod spodem

Model **nie ma dostępu do dysku**. Ma tylko cztery narzędzia:

| Narzędzie | Co robi |
|---|---|
| `ile_wolnego` | czyta rozmiar dysku |
| `skanuj_dysk` | uruchamia skaner, zwraca grupy i rozmiary |
| `kwarantanna_info` | liczy, ile leży w kwarantannie |
| `przenies_do_kwarantanny` | przenosi — **tylko** cache, buildy, instalki, logi |

Przepływ:

```
Ty: "posprzataj cache"
  → model: {"name": "przenies_do_kwarantanny", "kategorie": ["cache_i_temp"]}
  → kod: sprawdza, że cache_i_temp jest dozwolone
  → kod: pyta Ciebie o zgodę
  → kod: przenosi
  → model: "Przeniosłem 3,29 GB. Pliki są w kwarantannie."
```

Model proponuje, **kod decyduje**. Nawet gdyby model „wymyślił" usunięcie
`C:\Windows`, kod odrzuci kategorię — nie ma jej na liście dozwolonych.

---

## 4. Czego ten czat NIE zrobi

- nie odpowie na pytania ogólne („napisz wiersz") — to model 7B na CPU,
  od rozmowy o plikach jest dobry, od wiedzy o świecie słaby
- nie usunie duplikatów ani dużych plików — celowo
- nie działa bez `ollama serve` w tle
- przy 3B będzie się mylić częściej; przy 7B jest wyraznie lepszy po polsku

**Realne oczekiwania:** każde pytanie z narzędziem (skan) trwa 30–90 s,
bo skan dysku to 1 minutę, a model musi jeszcze przeczytać wynik. Prosta
odpowiedź bez skanu: 3–10 s.
