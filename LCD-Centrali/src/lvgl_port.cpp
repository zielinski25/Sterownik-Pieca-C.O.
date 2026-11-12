// [LVGL - KROK 4] Implementacja mostu LVGL <-> Twoj sprzet.
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "lvgl_port.h"

// Zdefiniowane w main.cpp - uzywamy tych samych obiektow/funkcji, zero duplikacji.
extern TFT_eSPI tft;
bool dotykOdczytaj(int16_t &px, int16_t &py);

// [PAMIEC] Brak PSRAM -> bufor rysowania MUSI byc maly. 480px szerokosci x 40
// wierszy x 2 bajty (RGB565) = 38400B (~37.5KB) na JEDEN bufor. LVGL odswieza
// ekran "paskami" tej wysokosci zamiast calym ekranem naraz - troche wolniej
// niz pelny framebuffer, ale 480x320x2 = 307KB w ogole by sie nie zmiescilo w
// SRAM zwyklego ESP32 (dzielonym jeszcze z WiFi/HTTPClient/JSON). To jest
// STANDARDOWA rekomendacja LVGL dla ESP32 bez PSRAM - 1/8 do 1/10 wysokosci
// ekranu na bufor.
//
// [FIX RAM - linker "dram0_0_seg overflowed"] Bufor NIE moze byc "static"
// tablica - static/global tablice ladowane sa do .bss/.dram0.bss, ktora ma
// STALY, ciasny rozmiar (dzielony ze WSZYSTKIMI globalnymi zmiennymi w
// projekcie - WiFi/HTTPClient/ArduinoJson/reszta main.cpp). Przy 38.4KB na
// sztywno to sie nie miescilo (przekroczenie o ~10.7KB przy linkowaniu).
// Zamiast tego alokujemy bufor w RUNTIME z malloc() na stercie (heap) - ten
// sam fizyczny RAM, ale liczony/przydzielany dynamicznie, wiec linker nie
// blokuje sie na etapie kompilacji. To jest zalecany sposob dla LVGL na
// ESP32 bez PSRAM.
#define LVGL_BUF_WIERSZE 40
static lv_disp_draw_buf_t drawBuf;
static lv_color_t *buf1 = nullptr;
// Tylko JEDEN bufor (nie podwojny) - oszczedza kolejne ~37.5KB RAM kosztem
// tego, ze CPU czeka na zakonczenie wysylki SPI przed rozpoczeciem rysowania
// kolejnego paska (brak nakladania sie rysowania z transferem). Jesli po
// testach na sprzecie odczujesz, ze UI "przycina" przy przewijaniu/animacjach,
// mozna dodac buf2 tej samej wielkosci - ale to nastepny krok, nie teraz.

static lv_disp_drv_t dispDrv;
static lv_indev_drv_t indevDrv;

// LVGL prosi: "wypelnij ten prostokat (area) danymi z color_p, potem zawolaj
// lv_disp_flush_ready()". Dokladnie to samo zadanie co Twoj sprKafelek.pushSprite(),
// tylko zamiast calego kafelka to jest pasek/fragment ekranu zdecydowany przez LVGL.
static void lvgl_disp_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  // pushColors zamiast pushImage - LVGL juz dostarcza dane w formacie RGB565,
  // wiec nie potrzeba konwersji, tylko surowy blok bajtow po SPI.
  tft.pushColors((uint16_t *)color_p, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(drv);
}

// LVGL prosi cyklicznie: "czy w tej chwili jest dotyk, a jesli tak - gdzie?"
// Uzywamy Twojej JUZ ISTNIEJACEJ, skalibrowanej funkcji dotykOdczytaj() -
// zero duplikacji logiki kalibracji/SPI bit-bang XPT2046.
//
// [FIX, ta sesja - gesty czasem "nie doliczaly sie" do konca] Dodana
// HISTEREZA PUSZCZENIA. Przyczyna: dotykOdczytaj() -> touchOdczytajSurowy()
// w main.cpp wymaga 4 KOLEJNYCH probek nacisku nad progiem TOUCH_Z_PROG -
// jesli w trakcie trwajacego przeciagniecia trafi sie ChOCIAZ JEDNA gorsza
// probka (normalny szum panelu rezystancyjnego przy ruchu palca), cala
// funkcja od razu zwraca false ("brak dotyku"), NAWET JESLI palec caly czas
// jest na szybie. LVGL, wolajac ta funkcje cyklicznie z lv_timer_handler(),
// widzi wtedy CHWILOWE PUSZCZENIE w SRODKU gestu i resetuje jego sledzenie -
// reszta ruchu liczy sie juz jako nowa, osobna sesja dotyku, ktora czesto
// nie zdazy nazbierac dystansu/predkosci potrzebnej do wykrycia gestu
// (LV_EVENT_GESTURE). Stad zglaszany objaw: gest w jedna strone "w miare"
// dzialal (mniej pecha z szumem), w druga regularnie nie doliczal sie wcale.
//
// Rozwiazanie: NIE zglaszaj RELEASED po pojedynczym nieudanym odczycie -
// dopiero po kilku KOLEJNYCH z rzedu (PROG_PUSZCZENIA ponizej). W
// miedzyczasie trzymamy ostatnia znana dobra pozycje (palec "wciaz wcisniety"
// z punktu widzenia LVGL) - to standardowy wzorzec debounce puszczenia,
// analogiczny do debounce WCISNIECIA juz istniejacego w touchOdczytajSurowy().
// PROG_PUSZCZENIA=2 to 2 KOLEJNE nieudane odczyty (kazdy to az 4 probki SPI
// wewnatrz dotykOdczytaj() - juz spory margines), wiec prawdziwe, celowe
// puszczenie palca wciaz jest wykrywane bardzo szybko (bez zauwazalnego
// opoznienia w zwyklych tapnieciach w kafelki/przyciski).
static void lvgl_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  static int16_t ostX = 0, ostY = 0;
  static bool bylWczesniejWcisniety = false;
  static uint8_t licznikNieudanychZRzedu = 0;
  const uint8_t PROG_PUSZCZENIA = 2;

  int16_t px, py;
  if (dotykOdczytaj(px, py)) {
    ostX = px;
    ostY = py;
    bylWczesniejWcisniety = true;
    licznikNieudanychZRzedu = 0;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = ostX;
    data->point.y = ostY;
    return;
  }

  if (bylWczesniejWcisniety && licznikNieudanychZRzedu < PROG_PUSZCZENIA) {
    // Odczyt sie nie udal, ale jeszcze nie na tyle razy z rzedu, zeby uznac
    // to za prawdziwe puszczenie - "przytrzymaj" ostatnia dobra pozycje.
    licznikNieudanychZRzedu++;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = ostX;
    data->point.y = ostY;
    return;
  }

  bylWczesniejWcisniety = false;
  licznikNieudanychZRzedu = 0;
  data->state = LV_INDEV_STATE_RELEASED;
}

void lvgl_port_init() {
  // [DEBUG - diagnoza cichego zawieszenia] Jesli log urywa sie gdzies w tej
  // funkcji bez zadnego bledu, te printy pokaza DOKLADNIE ktory krok sie nie
  // konczy (podejrzany numer 1: assert LVGL - domyslny handler w LVGL 8 to
  // "while(1);" bez zadnego printa, wiec awaria wewnatrz lv_init() albo
  // rejestracji sterownikow zawiesza sie calkowicie po cichu).
  Serial.printf("[LVGL] start lvgl_port_init(), wolny heap=%u B\n", ESP.getFreeHeap());

  lv_init();
  Serial.println("[LVGL] lv_init() OK");

  // [FIX RAM] Alokacja bufora rysowania na stercie zamiast statycznej
  // tablicy - patrz komentarz przy deklaracji buf1 wyzej. Jesli malloc
  // sie nie powiedzie (skrajny brak RAM w danym momencie startu), lepiej
  // to wykryc od razu przez Serial + zawiesic, niz dac LVGL pisac pod
  // nullptr i miec tajemniczy crash pozniej.
  size_t bufBajtow = sizeof(lv_color_t) * 480 * LVGL_BUF_WIERSZE;
  buf1 = (lv_color_t *)malloc(bufBajtow);
  if (buf1 == nullptr) {
    Serial.println("[LVGL] BLAD: brak RAM na bufor rysowania LVGL (malloc zwrocil NULL)");
    while (true) { delay(1000); }
  }
  Serial.printf("[LVGL] bufor rysowania zaalokowany (%u B), wolny heap=%u B\n", (unsigned)bufBajtow, ESP.getFreeHeap());

  lv_disp_draw_buf_init(&drawBuf, buf1, NULL, 480 * LVGL_BUF_WIERSZE);

  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = 480;
  dispDrv.ver_res = 320;
  dispDrv.flush_cb = lvgl_disp_flush;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  Serial.println("[LVGL] disp driver zarejestrowany");

  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = lvgl_touch_read;
  lv_indev_drv_register(&indevDrv);
  Serial.println("[LVGL] indev (dotyk) driver zarejestrowany - koniec lvgl_port_init()");
}

// Wolane raz na obrot loop() w main.cpp. lv_timer_handler() to "silnik" LVGL -
// przelicza animacje, odswieza to co sie zmienilo, woła flush_cb gdy trzeba.
// lv_tick_inc mowi LVGL ile faktycznie czasu minelo (LV_TICK_CUSTOM=0 w
// lv_conf.h, wiec robimy to recznie zamiast przez sprzetowy timer).
void lvgl_port_tick() {
  static uint32_t ostatniMs = 0;
  uint32_t teraz = millis();
  lv_tick_inc(teraz - ostatniMs);
  ostatniMs = teraz;
  lv_timer_handler();
}
