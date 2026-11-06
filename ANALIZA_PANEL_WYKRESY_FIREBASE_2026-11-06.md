# Panel LCD × Firebase — czy panel może pobierać wykresy z chmury?

Data: 2026-11-06. Pytanie z sesji testowej: *„A nie może pobierać z Firebase? Czy za
słabe ESP to jest?”*

## Odpowiedź w jednym akapicie

**Technicznie może** — ESP32 w panelu (240 MHz, 320 KB RAM) udźwignie TLS i JSON.
**Ale nie warto**: panel siedzi w sieci `PiecCO_AP`, która **nie ma internetu**,
więc żeby sięgnąć do Firebase trzeba by przeprojektować sieć panelu, dołożyć mu
dane logowania do chmury i ~50 KB sterty na TLS — po to, żeby pobrać dane, które
**już leżą na flashu centrali** i są dostępne 5× taniej po LAN-ie.

## Fakty (z istniejących źródeł, nie z pamięci)

| co | wartość | źródło |
|---|---|---|
| historia w Firebase | RTDB, **shardy dzienne**, ~121 B/rekord | `RAPORT_ANDROID_FIREBASE_HISTORIA.md` |
| rozmiar doby z chmury | **≈170 KB** (30 dni ≈ 5 MB, 45 dni ≈ 7,5 MB) | tamże, sekcja 6.1 |
| historia w centrali (RAM + LittleFS) | 1440 próbek / 24 h (`/hist_short.csv`) + 8000 próbek długiej (`/hist_long.csv`) | `main_centrala.cpp` |
| to samo jako CSV dla panelu | 1440 wierszy × ~90 B ≈ **130 KB** | format `/api/history` |
| panel: łącze do centrali | SoftAP centrali, `PiecCO_AP`, `192.168.4.1:80`, **bez internetu** | `main.cpp` panelu, `[ETAP 3 / krok 1]` |
| ile TLS kosztuje na ESP32 | ~40–55 KB sterty (mbedTLS, bufory 16 KB in/out) + 2–4 s na handshake | szacunek inżynierski, do zmierzenia |

Wniosek z liczb: **rozmiar danych nie jest problemem** (170 KB z chmury vs 130 KB
CSV z centrali — to ta sama skala). Problem jest w *drodze*, nie w *objętości*.

## Cztery rzeczy, które to blokują lub psują

1. **Brak internetu w sieci panelu.** Panel łączy się z AP centrali, a AP centrali
   nie ma routingu do świata. Alternatywa: panel w domowym WiFi — ale wtedy traci
   gwarantowany adres `192.168.4.1` (trzeba mDNS albo statycznego IP centrali w
   routerze) i przestaje działać scenariusz „piec u kolegi, bez routera”.
2. **Zależność od chmury w funkcji lokalnej.** Wykresy to dane z pieca, który stoi
   3 metry dalej. Awaria routera/Internetu = brak wykresów, mimo że centrala ma
   komplet danych na flashu. To pogorszenie, nie ulepszenie.
3. **Bezpieczeństwo.** Pobieranie z RTDB wymaga klucza API i danych użytkownika
   (albo tokenu) **w panelu na ścianie**. Dziś panel zna tylko hasło do AP pieca.
4. **Budżet pamięci.** Panel trzyma LVGL + bufor TFT 480×320; przy typowym
   statycznym buforze cząstkowym zostaje ~60–120 KB sterty. TLS zjada z tego
   połowę i fragmentuje stertę — dokładnie tam, gdzie LVGL alokuje przy każdej
   zmianie ekranu. To przepis na rzadkie, trudne do odtworzenia restarty.

## Kiedy Firebase **ma** sens (i w jakiej roli)

* **archiwum dłuższe niż 24 h** — tego centrala nie trzyma w RAM, a shardy dzienne
  w RTDB trzymają 30–45 dni,
* **podgląd zdalny** — telefon poza domem; to już robi aplikacja Android
  (`loadRange()`, `RtdbClient.kt`), i to jest właściwe miejsce na TLS,
* **backup**, gdy centrala straci flash albo zostanie wymieniona.

We wszystkich trzech rolach **klientem chmury jest centrala (ESP32-S3 N16R8: PSRAM,
16 MB flash, TLS już skompilowany) albo telefon** — nigdy panel z LVGL.

## Wariant, który ma sens, jeśli chcesz chmury na panelu

Nie „panel → Firebase”, tylko **„panel → centrala → Firebase”**:

```
panel  --(LAN, CSV, bez TLS)-->  centrala  --(TLS)-->  Firebase RTDB
```

Centrala wystawia dodatkowy endpoint (np. `/api/history/remote?dni=7`), pobiera
shardy, skleja je w **dokładnie ten sam CSV** (14/35 kolumn) i oddaje panelowi.
Zysk: panel zostaje głupi i lekki, format CSV bez zmian, jedno miejsce z
poświadczeniami (centrala), a przy braku internetu endpoint zwraca 503 i panel
pokazuje „Brak danych” — bez zmiany firmware'u panelu.

## Co zmierzyć, zanim cokolwiek się napisze

1. `ESP.getFreeHeap()` w panelu **po** wejściu na ekran Wykresy (dziś nie logujemy —
   warto dodać jedną linijkę w `wykresyFetchWykonaj()`).
2. Czas `http.GET()` na `/api/history` z 1440 wierszami (dziś: 360 w symulatorze).
3. Czy `WiFiClientSecure` + `FirebaseClient` w ogóle się linkuje przy obecnym
   `partitions.csv` panelu (binarka panelu jest już duża).

## Rekomendacja

**Nie ciągnąć Firebase do panelu.** Panel zostaje przy CSV z centrali. Jeśli
potrzebne są dane > 24 h na ekranie — zrobić proxy w centrali (wariant wyżej) i
osobny zakres „Tydzień” w panelu. To jedna zmiana po stronie centrali, zero zmian
w panelu i zero nowych poświadczeń na ścianie.
