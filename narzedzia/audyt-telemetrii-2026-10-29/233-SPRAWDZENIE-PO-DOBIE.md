# 233 — co miałeś dzisiaj przysłać po wgraniu 3.56.8

Data 2026-10-30. Dotyczy firmware'u centrali **3.56.8** (wgrany 29.10).

## W jednym zdaniu

Po dobie pracy **log z centrali** — w nim są cztery nowe liczniki, których
nigdzie indziej nie ma. Z nich odczytam, czy detektor luk łapie ubytki,
i czy zapis do NVS nie mieli flasha.

## Czego NIE potrzebuję (i dlaczego to ważne)

Wczoraj napisałem w README 3.56.8, że liczniki są „w `/logi` oraz w `/api/status`".
**To było nieprawdziwe** i dziś to sprawdziłem w kodzie:

```
telemetry_sent        → 1 wystąpienie, linia 47121 (log HEALTH-A)
telemetry_ring        → 1 wystąpienie, linia 47122 (log HEALTH-A)
telemetry_gap_events  → 1 wystąpienie, linia 47123 (log HEALTH-A)
telemetry_gap_samples → 1 wystąpienie, linia 47124 (log HEALTH-A)
```

Każde pole istnieje dokładnie raz — w linii `tag=HEALTH-A`. W JSON-ie
`/api/status` **ich nie ma**. Więc `/status` nic nie da; potrzebny jest log.
README poprawione.

## Co przysłać — dwa sposoby

### Sposób 1: Telegram (najprostszy)

Wyślij do bota:

```
/logi
```

Przyjdzie plik z logiem. Przyślij mi go.

Jeśli nie pamiętasz, jaka wersja siedzi w środku, wyślij też `/version` —
odpowie np. `Obecna wersja: 3.56.8`.

### Sposób 2: przeglądarka / PowerShell

Wpisz IP centrali (w aplikacji: Ustawienia → sterownik lokalny) w miejsce `IP`:

```
http://IP/api/log-lines?lines=5000
```

Zapisze się plik tekstowy — też może być.

Albo jedna linia w PowerShellu (zapisuje na pulpit):

```
$ip='IP'; Invoke-WebRequest "http://$ip/api/log-lines?lines=5000" -OutFile "$env:USERPROFILE\Desktop\logi-centrala.txt"; (Get-Item "$env:USERPROFILE\Desktop\logi-centrala.txt").Length
```

## Co ja z tego odczytam

| co | skąd | czego oczekuję |
|---|---|---|
| wersja w środku | Telegram `/version` | `3.56.8` |
| ile wyprodukowano / wysłano / w kolejce / odrzucono | HEALTH-A | bilans doby |
| `telemetry_gap_events`, `telemetry_gap_samples` | HEALTH-A | **0 = detektor milczy**, >0 = mamy dowód |
| co ile zapisywany `srOstPompa` | log `POMPA` | kilka razy na dobę, nie co minutę |
| restarty (`boot=`) | wszystkie linie | ile startów i kiedy |

**Dlaczego to jest cała pointa 3.56.8:** audyt 29 dni wykazał 65 pojedynczych
strat, dla których w kodzie **nie ma żadnej ścieżki**. Detektor luk miał
powiedzieć wprost: „tu i tu przeskoczyły numery". Jeśli po dobie napisze
`telemetry_gap_events=0`, a próbki mimo to zniknęły — to znaczy, że giną
ścieżką, której detektor nie widzi (spool albo fałszywe potwierdzenie paczki).
Wtedy dopiero robimy 3.56.9, i wiemy już, co naprawiać.

## Czego jeszcze nie wiemy, dopóki log nie przyjdzie

- Czy zniknęła 20-minutowa dziura w „Ogrz. śr." po restarcie.
  **Uwaga:** przy pierwszym starcie po wgraniu mogła wystąpić ostatni raz —
  klucz `srOstPompa` powstaje dopiero przy pierwszym zaniku pompy po wgraniu.
  Od drugiego restartu ma zniknąć.
- Czy `srOstPompa` nie zapisuje się zbyt często (ryzyko zużycia sektora flash).

## Narzędzie

`sprawdz-po-dobie.py` w tym katalogu — czyta log i wypisuje bilans per start,
listę przerw z godzinami, listę restartów. Uruchomienie:

```
python sprawdz-po-dobie.py logi-centrala.txt
```

Liczniki siedzą w RAM i **zerują się przy restarcie**, dlatego bilans jest
liczony osobno dla każdego `boot=` — odejmowanie przez granicę startu dawałoby
liczby bez sensu. To też jest powód, dla którego z jednego, długiego logu
wychodzi więcej niż z samego `/api/status`.
