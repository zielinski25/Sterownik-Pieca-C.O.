# Hasła i tokeny — co wyciekło i jak to naprawić

Data: 2026-10-23
Dotyczy: repozytorium `zielinski25/Sterownik-Pieca-C.O.`

---

## 1. Co było publiczne

Repozytorium było **publiczne**. W kodzie były wpisane na sztywno cztery sekrety:

| Sekret | Gdzie | W ilu plikach |
|---|---|---|
| hasło konta Firebase | `main_centrala.cpp`, `Piec.html` | 8 |
| token komend | `main_centrala.cpp`, `Piec.html`, `Prefs.kt` | 6 |
| hasło hotspota pieca | `main_centrala.cpp` | 4 |
| hasło OTA | `main_centrala.cpp` | 4 |

**Wszystkie cztery zostały usunięte z kodu** i przeniesione do `secrets.h`,
którego git nie widzi.

### Dlaczego to było groźne

Razem te cztery rzeczy dawały **pełną kontrolę nad piecem**:

| Element | Stan przed |
|---|---|
| adres bazy Firebase | publiczny |
| e-mail konta | publiczny |
| klucz API Firebase | publiczny |
| **hasło konta** | **publiczne** |
| **token komend** | **publiczny** |

Ktoś, kto znał te pięć rzeczy, mógł wysłać do pieca:

- `pompa_wyl` — **wyłączyć pompę na 60 minut** przy gorącym kotle
- `reset_alarmow` — **skasować alarm**
- `update_centrala` — **zlecić wgranie firmware**
- oraz 14 innych poleceń

---

## 2. Ważne: samo usunięcie z kodu NIE wystarcza

Sekrety są **w historii gita od wersji v3.38.0**. Usunięcie ich z obecnych
plików nie usuwa ich z przeszłości — ktoś może przejrzeć historię.

**Dlatego trzeba zmienić same hasła.** Kolejność ma znaczenie.

---

## 3. Procedura zmiany — krok po kroku

### Krok 0 — przygotuj `secrets.h`

W folderze z `main_centrala.cpp`:

1. skopiuj `secrets.h.example` → `secrets.h`
2. wpisz **obecne** hasła (te, które działają dziś)
3. resztę kroków zrobisz później

To zapewni, że po każdej kompilacji piec nadal się łączy.

### Krok 1 — ustaw repozytorium na prywatne

GitHub → Settings → General → Danger Zone → **Change visibility** → Private.

Nic nie trzeba wgrywać, jedno kliknięcie. Zamyka dostęp do historii.

### Krok 2 — zmień hasło konta Firebase

Konsola Firebase → Authentication → Users → konto `piec_co_boot@akwarium.local`
→ **Reset password** / zmień hasło.

**Uwaga:** od tej chwili piec **traci połączenie z bazą**, bo ma w pamięci
NVS stare hasło. To jest oczekiwane — i dlatego ważny jest krok 3.

Telegram **dalej działa**, bo niekorzysta z Firebase.

### Krok 3 — wyślij nowe hasło do pieca przez Telegram

Na Telegramie wyślij do bota pieca:

```
/fbprov|KLUCZ_API|piec_co_boot@akwarium.local|NOWE_HASLO
```

gdzie:
- `KLUCZ_API` to klucz z `secrets.h` (`SECRET...` — ten sam, który jest w `Prefs.kt`)
- `NOWE_HASLO` to hasło ustawione w kroku 2

**Dlaczego to działa:** firmware przyjmuje `/fbprov` **tylko wtedy, gdy Firebase
nie jest połączony**. Po zmianie hasła dokładnie tak jest — więc warunek jest
spełniony. Piec zapisuje nowe dane do NVS i łączy się ponownie.

Odpowiedź bota: `FB-PROV: ...` — potwierdzenie albo błąd formatu.

### Krok 4 — sprawdź, czy piec wrócił

Wyślij na Telegramie:

```
/status
```

Jeśli odpowiada — działa. Jeśli w Firebase widać świeże wpisy — połączenie wróciło.

### Krok 5 — zmień token komend

Token podpisuje polecenia (`pompa_wl`, `reset_alarmow`...). Wybierz nowy, np.
długi losowy ciąg. Trzeba go wpisać w **trzech miejscach**:

| Gdzie | Jak |
|---|---|
| `secrets.h` | `#define SECRET_CMD_TOKEN "nowy-token"` — potem kompilacja i wgranie |
| aplikacja Android | Ustawienia → token komend |
| panel `Piec.html` | pole tokenu / `localStorage` |

**Uwaga:** dopóki nie wgrasz nowego firmware, piec nadal przyjmuje **stary** token.
Więc zmiana tokenu wymaga wgrania firmware. Zaplanuj to na spokojny moment.

### Krok 6 — zmień hasła WiFi i OTA

| Co | Gdzie zmienić | Gdzie wpisać |
|---|---|---|
| hasło hotspota | `SECRET_WIFI_AP_PASSWORD` | `secrets.h` → wgranie |
| hasło OTA | `SECRET_OTA_HASLO` | `secrets.h` → wgranie |

Obowiązują po wgraniu nowego firmware.

---

## 4. Co jest w `secrets.h`

```cpp
#define SECRET_WIFI_AP_SSID     "PiecCO_AP"
#define SECRET_WIFI_AP_PASSWORD "..."   // min. 8 znakow
#define SECRET_OTA_HASLO        "..."
#define SECRET_FIREBASE_PASS    "..."
#define SECRET_CMD_TOKEN        "..."
```

Plik `secrets.h`:

- jest w `.gitignore` — **git go nie widzi**
- **nie jest w żadnej kopii zapasowej** — jeśli zgubisz komputer, wpiszesz od nowa
- **skopiuj go na pendrive**

Wzorem jest `secrets.h.example`, który **jest** w gicie (bez prawdziwych wartości).

### Co się stanie bez `secrets.h`

Kompilacja **zatrzyma się** z komunikatem:

```
Brak pliku secrets.h — skopiuj secrets.h.example jako secrets.h i wpisz hasla.
```

Jeśli masz stary `secrets.h` bez nowych wpisów:

```
secrets.h jest nieaktualny: brak SECRET_FIREBASE_PASS.
Skopiuj nowe wpisy z secrets.h.example.
```

To celowe — lepiej, żeby się nie skompilowało, niż żeby piec poszedł
w świat z pustym hasłem.

---

## 5. Gdzie firmware bierze hasło

Kolejność ważności:

1. **pamięć NVS** (`loadFirebaseConfig()`) — jeśli ma kompletne dane, używa ich
2. `FIREBASE_BOOT_*` z `secrets.h` — tylko gdy NVS jest pusty lub niepełny

**Konsekwencja:** zmiana `secrets.h` **nie zmienia** hasła w piecu, który już
ma dane w NVS. Dlatego krok 3 (Telegram `/fbprov`) jest konieczny — albo trzeba
wyczyścić NVS.

---

## 6. Lista kontrolna

- [ ] repo ustawione na prywatne
- [ ] hasło Firebase zmienione w konsoli
- [ ] nowe hasło wysłane `/fbprov` na Telegramie
- [ ] `/status` odpowiada
- [ ] token komend zmieniony i wgrany
- [ ] hasło WiFi zmienione
- [ ] hasło OTA zmienione
- [ ] `secrets.h` skopiowany na pendrive
- [ ] hasła z raportu `PORZADKI_2026-10-23.md` usunięte
