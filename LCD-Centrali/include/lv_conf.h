/*
 * [LVGL - KROK 4] Konfiguracja pod TWOJ sprzet: zwykly ESP32 WROOM, BEZ PSRAM,
 * 4MB flash. Kazda wartosc ponizej jest swiadomie dobrana pod te ograniczenia
 * - nie kopiuj tego 1:1 na inna plytke bez przemyslenia, zwlaszcza sekcji
 * PAMIEC i EKRAN.
 *
 * Ten plik MUSI lezec w /include (obok platformio.ini), nie w /src -
 * PlatformIO domyslnie szuka lv_conf.h wlasnie tam.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*==================
   PAMIEC
   [FIX linker] Statyczny bufor LV_MEM_SIZE (rezerwowany na sztywno w .bss
   juz przy LINKOWANIU) razem ze statycznym buforem rysowania w lvgl_port.cpp
   nie miescily sie w DRAM zwyklego ESP32 (bez PSRAM) obok reszty Twojego
   kodu - stad blad "dram0_0_seg overflowed by 10720 bytes". Zamiast
   zmniejszac limity "na styk", przenosimy zarzadzanie pamiecia LVGL na
   zwykly malloc/free z ESP-IDF (LV_MEM_CUSTOM=1) - wtedy LVGL prosi o
   pamiec ZE STERTY W RUNTIME zamiast rezerwowac ja na sztywno przy
   kompilacji. To standardowa rekomendacja dla ESP32 bez PSRAM.
 *==================*/
#define LV_MEM_CUSTOM      1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc
/* LV_MEM_SIZE celowo bez #define - nieuzywane, gdy LV_MEM_CUSTOM=1 */

/*==================
   TICK
   LVGL potrzebuje wiedziec ile czasu uplynelo - podpinamy to recznie w
   lvgl_port.cpp (lv_tick_inc), wiec tu wylaczamy automatyczny tick.
 *==================*/
#define LV_TICK_CUSTOM     0

/*==================
   KOLOR
   16-bit (RGB565) - dokladnie to, czego uzywa TFT_eSPI/Twoj wyswietlacz.
   LV_COLOR_16_SWAP=0 bo TFT_eSPI sam zajmuje sie kolejnoscia bajtow przy
   pushImage (nie duplikujemy tej pracy w LVGL).
 *==================*/
#define LV_COLOR_DEPTH     16
#define LV_COLOR_16_SWAP   0
#define LV_COLOR_SCREEN_TRANSP 0

/*==================
   FUNKCJE / WIDGETY
   Wylaczamy wszystko, czego dashboard nie potrzebuje - kazdy wylaczony
   widget to mniej flash (masz tylko 4MB, dzielone z WiFi/TLS stackiem,
   ktory sam potrafi zjesc >1MB).
 *==================*/
#define LV_USE_LOG          0   // wlacz na 1 tylko przy debugowaniu - kosztuje flash+CPU
#define LV_USE_ASSERT_NULL  1
#define LV_USE_ASSERT_MALLOC 1

#define LV_USE_ARC          1   // przyda sie np. do wskaznika suwaka KLAPA/SYBERKA
#define LV_USE_BAR          1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    0
#define LV_USE_CANVAS       0
#define LV_USE_CHECKBOX     0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMG          1   // ikony (plomien/kropla/suwak/alarm)
#define LV_USE_LABEL        1
#define LV_USE_LINE         0
#define LV_USE_ROLLER       0
#define LV_USE_SLIDER       1   // panel sterowania KLAPA/SYBERKA (docelowo)
#define LV_USE_SWITCH       0
#define LV_USE_TEXTAREA     0
#define LV_USE_TABLE        0

// [FIX build] Te ponizej NIE byly jawnie wypisane w pierwszej wersji tego
// pliku - lv_conf_internal.h wtedy podstawia DOMYSLNA wartosc z szablonu
// LVGL, a domyslnie prawie wszystkie "extra" widgety sa WLACZONE (1).
// LV_USE_KEYBOARD i LV_USE_MSGBOX wymagaja LV_USE_BTNMATRIX (wylaczone
// powyzej), LV_USE_SPINBOX wymaga LV_USE_TEXTAREA (tez wylaczone) -> stad
// blad kompilacji "lv_btnm is required". Dashboard nie potrzebuje ZADNEGO
// z tych widgetow, wiec wylaczamy je wszystkie jawnie, zamiast polegac na
// domyslnych wartosciach szablonu.
#define LV_USE_ANIMIMG      0
#define LV_USE_CALENDAR     0
#define LV_USE_CHART       1   // [PARTIA 7, ta sesja] realny wykres na ekranie Wykresy (byl 0 - stad blad kompilacji "lv_chart_series_t does not name a type")
#define LV_USE_COLORWHEEL   0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          0
#define LV_USE_LIST         0
#define LV_USE_MENU         0
#define LV_USE_METER        0
#define LV_USE_MSGBOX       0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_TABVIEW      0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0
#define LV_USE_SPAN         0

/*==================
   TEMATY
   Jeden prosty, ciemny motyw bazowy - stylujemy kafelki recznie (kolory
   identyczne jak w obecnym main.cpp: KOL_TLO/KOL_KARTA/KOL_TEKST itd.),
   wiec nie potrzebujemy rozbudowanego theme engine.
 *==================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_USE_THEME_BASIC   0

// [FIX build] Uklad "kolumna przyciskow" w panelu POMPA (AUTO/RECZNIE WL/WYL)
// korzysta z flex layout - bez tego lv_obj_set_flex_flow nie istnieje.
#define LV_USE_FLEX 1

/*==================
   FONTY
   Montserrat 14 (etykiety) i 28 (duze wartosci liczbowe) - odpowiedniki
   fontu 1 i fontu 4 z Twojego obecnego main.cpp.
 *==================*/
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT       &lv_font_montserrat_14

/*==================
   INNE
 *==================*/
#define LV_USE_PERF_MONITOR 0   // wlacz na 1 tymczasowo, jesli chcesz zobaczyc FPS/uzycie CPU w rogu ekranu
#define LV_USE_MEM_MONITOR  0
#define LV_DPI_DEF 130          // orientacyjne, wplywa tylko na domyslne odstepy/rozmiary w px

#endif // LV_CONF_H
