#include <ArduinoJson.h>
#include "SiecIkony.h"
#include <cassert>
#include <cstdio>
#include <string>
struct lv_obj_t { int color = 0; bool hidden = false; std::string text; };
static lv_obj_t* wifiPasek[7];
static lv_obj_t* wifiX;
static lv_obj_t* netX;
static lv_obj_t* netGlobusSiatka;
static lv_obj_t* netGlobusLady;
static bool g_polaczono = false;
constexpr int KOL_TEKST = 1, KOL_TEKST_DIM = 2, KOL_WARN = 3, KOL_ERR = 4;
constexpr int KOL_GLOBUS_SIATKA = 5, KOL_GLOBUS_LADY = 6, LV_OBJ_FLAG_HIDDEN = 1;
static int rgb565(int x) { return x; }
static void lv_obj_set_style_bg_color(lv_obj_t* o, int color, int) { o->color = color; }
static void lv_obj_set_style_text_color(lv_obj_t* o, int color, int) { o->color = color; }
static void lv_obj_set_style_img_recolor(lv_obj_t* o, int color, int) { o->color = color; }
static void lv_obj_add_flag(lv_obj_t* o, int) { o->hidden = true; }
static void lv_obj_clear_flag(lv_obj_t* o, int) { o->hidden = false; }
static void lv_label_set_text(lv_obj_t* o, const char* text) { o->text = text; }
#include "render_fragment.inc"
static int parse(const char* json) {
  JsonDocument doc;
  assert(deserializeJson(doc, json) == DeserializationError::Ok);
  return SiecIkony::odczytaj(doc).internet;
}
static void check_bars(int n) {
  for (int i = 0; i < 7; ++i)
    assert(wifiPasek[i]->color == (i < n ? KOL_TEKST : KOL_TEKST_DIM));
}
int main() {
  lv_obj_t bars[7], wifi, globeX, grid, land;
  for (int i = 0; i < 7; ++i) wifiPasek[i] = &bars[i];
  wifiX = &wifi; netX = &globeX; netGlobusSiatka = &grid; netGlobusLady = &land;
  const int thresholds[] = {-95, -89, -83, -77, -71, -65, -55};
  for (int i = 0; i < 7; ++i) {
    assert(SiecIkony::oblicz(true, thresholds[i], 1).kreski == i + 1);
    assert(SiecIkony::oblicz(true, thresholds[i] - 1, 1).kreski == i);
  }
  assert(parse("{\"internet_wan\":true,\"internet\":false,\"rssi\":0}") == 1);
  assert(parse("{\"internet_wan\":false,\"internet\":true,\"rssi\":-50}") == 0);
  for (const char* json : {"{}", "{\"internet_wan\":null}", "{\"internet_wan\":1}",
                           "{\"internet_wan\":\"true\"}", "{\"internet\":true}"})
    assert(parse(json) == SiecIkony::INTERNET_NIEZNANY);
  lvgl_dashboard_poc_aktualizuj_siec(true, -65, 1);
  assert(g_polaczono && wifi.hidden && globeX.hidden); check_bars(6);
  assert(grid.color == KOL_GLOBUS_SIATKA && land.color == KOL_GLOBUS_LADY);
  // HTTP niedostepny, ale LCD nadal polaczone z AP: kreski nadal prawdziwe.
  lvgl_dashboard_poc_aktualizuj_siec(false, -75, 1);
  assert(!g_polaczono && wifi.hidden && !globeX.hidden && globeX.text == "?"); check_bars(4);
  lvgl_dashboard_poc_aktualizuj_siec(true, -75, 0);
  assert(wifi.hidden && globeX.text == "X" && !globeX.hidden); check_bars(4);
  // Fizyczne odlaczenie WiFi: X na paskach i nieznany WAN.
  lvgl_dashboard_poc_aktualizuj_siec(true, SiecIkony::ROZLACZONE, 1);
  assert(!wifi.hidden && wifi.text == "X" && wifi.color == KOL_ERR); check_bars(0);
  assert(!globeX.hidden && globeX.text == "?");
  lvgl_dashboard_poc_aktualizuj_siec(true, 0, 1);
  assert(!wifi.hidden && wifi.text == "?" && globeX.hidden); check_bars(0);
  lvgl_dashboard_poc_aktualizuj_siec(true, -50, SiecIkony::INTERNET_NIEZNANY);
  assert(wifi.hidden && !globeX.hidden && globeX.text == "?"); check_bars(7);
  lvgl_dashboard_poc_aktualizuj_siec(true, -65, 1);
  assert(wifi.hidden && globeX.hidden); check_bars(6);
  puts("OK LCD: lokalny RSSI, progi, WAN i utrata/odzyskanie");
}
