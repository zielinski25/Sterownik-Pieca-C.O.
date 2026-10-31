# Etap 217 — plan

## Po co

Wszystkie wcześniejsze etapy sprawdzały kod **pojedynczo**: jeden bilet, jedno
pobranie, jedna wysyłka. Usterki zasobów ("po kilkunastu pobraniach przestaje
działać, pomaga restart") nie mają jak się w takim teście ujawnić — trzeba
powtórzyć operację setki razy i policzyć uchwyty plików.

## Kolejność

1. Odtworzyć plik po patchu v3.56.3 z przypiętego źródła i **sprawdzić SHA**
   wobec etapu 216 (żeby soak badał dokładnie ten kod, który pójdzie na urządzenie).
2. Wyciągnąć z niego (dopasowaniem nawiasów, nie przepisywaniem) oba wysyłacze
   Telegram, trasę, zapinkę i bramkę.
3. Napisać 9 grup testu: powtarzalność, zerwanie, równoległość, błąd VFS,
   zajęta blokada, wysyłki, ścieżki awarii, `hist_long`, bilans.
4. **Policzyć uchwyty**: `otwartych`, `otwarciem`, `zamkniec`, maksimum naraz.
5. Dokleić kontrole negatywne (M1/M2/M3) — bez nich brama jest ozdobą.
6. Przepuścić 3 profile (native, ASan/UBSan z wykrywaniem wycieków, libstdc++ assertions).
7. Spakować, powtórzyć z paczki, wysłać.

## Czego ten etap NIE robi

- nie zmienia firmware (v3.56.3 bez zmian),
- nie dotyka urządzenia,
- nie dodaje kolejki pobrań (to nadal świadomie jedno pobranie naraz),
- nie udaje LittleFS: model sprawdza umowę interfejsu, nie sektory i zasilanie.

## Definicja sukcesu

`PASS brama217`, w trzech profilach po 66 sprawdzeń i 0 błędów, trzy mutanty
czerwone, `otwartych == 0` i `otwarć == zamknięć` na końcu.
