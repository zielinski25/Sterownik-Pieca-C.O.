# 245 — 0.31.62: zakładka i wykres przeżywają odtworzenie (koniec „znowu pulpit”)

Data: 2026-10-30 · Dotyczy: aplikacja Android · Firmware: bez zmian (3.57.1)

## Zgłoszenie

> „Gdy odpakuję aplikację, wejdę do niej za parę sekund, to ona ładuje się
> od nowa na dashboard, a jak wejdę w Wykresy, od nowa odczytuje parę sekund
> wykresy — przypominam, że to jest po odpakowaniu, po spakowaniu aplikacji.”

Do tego pytanie: **dlaczego starsze wersje nie „ubijały się” tak szybko**
i propozycja: niech wykresy zostaną przez parę minut i pokazują się od razu.

## Co pokazał kod

| Miejsce | Fakt |
|---|---|
| `MainActivity.onCreate` | `model = AppModel(applicationContext, lifecycleScope)` — **nowy model przy każdym odtworzeniu okna** |
| `AppModel.kt:196` | `var page by mutableIntStateOf(0)` — zakładka **tylko w RAM**, brak zapisu |
| brak `onSaveInstanceState` / `rememberSaveable` | stan nie przeżywa nawet obrotu ekranu (obrót ratuje `configChanges`) |
| `AlarmNotify.kt:180,192,251` | `FLAG_ACTIVITY_NEW_TASK or FLAG_ACTIVITY_CLEAR_TOP` przy `launchMode` domyślnym → **CLEAR_TOP niszczy i tworzy MainActivity od nowa** |
| `AppModel.kt:43` | `HISTORY_MIN_INTERVAL_MS = 60_000` — po minucie każde wejście na Wykresy znów pyta sieć |
| `AppModel.kt:128` (do 0.31.61) | `TelemetryCache(ctx)` tworzony per model → **pamięć RAM próbek ginie razem z aktywnością** |

Wniosek: **w starszych wersjach nie było żadnej ochrony stanu** — ten sam
`page = 0` i to samo tworzenie modelu od zera. Różnica jest w tym, kiedy Android
decyduje się zabić proces: decyduje o tym wolna pamięć i polityka baterii
producenta, a nie numer wersji. Aplikacja trzyma w RAM całą historię telemetrii
(od 0.31.43, retencja 35 dni — przy 30 dniach dziesiątki tysięcy rekordów),
więc jest cięższym kandydatem do zabicia niż wcześniej.

## Co zmieniono w 0.31.62

1. **Zakładka zapamiętana** (`Prefs.K_LAST_PAGE`): `page` odtwarzane w `init`,
   zapisywane w `navigate()`. Po powrocie jesteś na Wykresach, nie na pulpicie.
2. **Wykres zostaje w pamięci procesu** — nowy plik `core/AppRuntime.kt`:
   - `telemetryCache(ctx)` — **jedna** pamięć historii na proces (nie na aktywność),
   - `lastTelemetry` / `lastLive` / `lastRangeSec` / `lastLoadedAtMs` —
     ostatnio policzony wykres rysuje się natychmiast po odtworzeniu,
   - zapis do `AppRuntime` po każdym udanym wczytaniu, odtworzenie w `init`.
3. **Okno „parę minut”**: `HISTORY_MIN_INTERVAL_MS` 60 s → **5 min**.
   Powrót na Wykresy w tym czasie = zero ruchu sieciowego.
   Przycisk „Odśwież” i zmiana zakresu nadal pobierają (`force = true`).
4. **Alarm nie odtwarza panelu**: `MainActivity` → `android:launchMode="singleTop"`,
   a trzy intencje alarmowe dostają dodatkowo `FLAG_ACTIVITY_SINGLE_TOP` —
   `onNewIntent` zamiast niszczenia aktywności.
5. **Diagnostyka w dzienniku** (TERMINAL):

```
Start panelu: pid=1234 wiek=37s · zakładka 1 · wykres z pamięci (1450 pkt)
```

PID inny niż poprzednio = proces został zabity (brak pamięci / oszczędzanie
baterii). Ten sam PID = tylko odtworzenie okna. To kończy zgadywanie.

## Czego to nie obejmuje

Po prawdziwym zabiciu procesu RAM znika. Wtedy ratują: zapamiętana zakładka
oraz pamięć historii na dysku — 0.31.61 rysuje z niej wykres od razu, a sieć
dociąga w tle.

Warto też sprawdzić w telefonie:

- Ustawienia → Opcje programisty → **„Nie zachowuj aktywności” = WYŁĄCZONE**,
- Ustawienia → Aplikacje → SterownikCO → Bateria → **„Bez ograniczeń”**.

## Weryfikacja paczki

| Co | Wynik |
|---|---|
| `versionCode` / `versionName` | 31062 / „0.31.62” |
| `AppRuntime.kt` w ZIP | jest |
| `launchMode="singleTop"` w manifeście | MainActivity + AlarmActivity |
| `unzip -tq` | bez błędów, 190 plików |
| SHA-256 | `69184ef2729ba10eecd835a8abc7ac01f2027af3cedf09120608ccc50a151012` (1 175 687 B) |
| Pobranie z Dysku i porównanie SHA | zgodne |

## Paczka

`SterownikCO-Android-v0.31.62-PELNY-PROJEKT.zip`
ID `1QYrRWmkzFemYmMPpBj1_Dg0B70AR99lu`
SHA-256 `69184ef2729ba10eecd835a8abc7ac01f2027af3cedf09120608ccc50a151012`
