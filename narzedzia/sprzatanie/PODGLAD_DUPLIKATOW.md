# Jak przejrzeć duplikaty (i nie skasować zdjęć)

Numeracja grup **zmienia się** z każdym skanem — zależy od rozmiarów.
Dlatego nie zgaduj numeru: uruchom agenta i wpisz samo `v`.

```powershell
python agent.py --bez-ai
```

Na pytanie `Twoj wybor:` wpisz:

```
v
```

Program wypisze:

```
Grupy do wyboru:
  1. duplikaty              2.05 GB (292 poz.)
  2. cache_i_temp           0.70 GB (1 poz.)
  Wpisz np. 'v 1', aby zobaczyc pelna liste grupy 1.
```

Teraz `v 1` pokaże **wszystkie 292 ścieżki** duplikatów i nic nie przeniesie.

## Jak czytać wynik

Co druga linia to plik, który **zostanie przeniesiony**. Reguła: zostaje
**najstarszy** egzemplarz, reszta do kwarantanny.

```
  68.1 MB  Ender3V2S1-...-v19 (1).zip     <- ten leci (bo "(1)" = kopia)
   3.4 MB  IMG20260417162454 (1).jpg      <- ten leci
```

Nazwa z `(1)`, `(2)` na końcu to prawie zawsze kopia z pobierania.

## Co zrobić, jeśli chcesz zachować zdjęcia

Pomiń całą kategorię numerem z listy:

```
Twoj wybor: 1
```

Wtedy `duplikaty` zostają, a reszta (cache, buildy, instalki) leci.
Potem `t` na pytanie o zgodę.

## Czego agent NIE zrobi z duplikatami

- nie usunie żadnego zdjęcia na stałe — wszystko idzie do kwarantanny
- **nie zostawi** dwóch kopii, jeśli są identyczne bajt w bajt (SHA-256)
- nie ruszy `Pictures\realme_RMX3085_camera\VID...mp4` — to nie duplikat, tylko duży plik

## Jeśli po tygodniu okaże się, że czegoś brakuje

```powershell
python agent.py --przywroc
```

Przywraca **wszystko** z ostatniej kwarantanny na miejsce.
