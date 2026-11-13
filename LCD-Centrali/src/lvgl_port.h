#pragma once
// [LVGL - KROK 4] Most miedzy LVGL a Twoim sprzetem. Nic z Twojego istniejacego
// sterownika ekranu (tft, zdefiniowany w main.cpp) ani dotyku (touchPobierzStabilnaPozycje,
// tez w main.cpp) nie jest tu duplikowane - ten plik tylko WOLA te funkcje z
// wnetrza callbackow, ktorych LVGL oczekuje.
//
// Wymaga, zeby main.cpp:
//   1) deklarowal `extern TFT_eSPI tft;`            (juz masz - to Twoj obiekt)
//   2) udostepnial `bool touchPobierzStabilnaPozycje(uint16_t&, uint16_t&);`
//      (juz masz t   ą funkcje - patrz linia ~237 main.cpp)
//   3) w setup() wolal lvgl_port_init() PO tft.init() i PO touchInicjalizujPiny()
//   4) w loop() wolal lvgl_port_tick() na kazdym obrocie petli

void lvgl_port_init();
void lvgl_port_tick();
