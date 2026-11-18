/* ===========================================================================
 * PROTOTYP POMIAROWY - to NIE jest czesc firmware'u i nie trafia na plytke.
 * ===========================================================================
 * Pytanie: ile rysowania zniknie, jesli dashboard bedzie ustawial napis,
 * kolor, ikone i widocznosc TYLKO wtedy, gdy nowa wartosc rozni sie od tej,
 * ktora juz jest na ekranie?
 *
 * LVGL 8.3.11 przy kazdym wywolaniu ponizszych czterech funkcji oznacza
 * obiekt "do przerysowania" - takze wtedy, gdy wartosc jest ta sama co przed
 * chwila (lv_label.c: lv_label_set_text(); lv_obj_style.c:
 * lv_obj_set_local_style_prop() -> lv_obj_refresh_style(); lv_img.c:
 * lv_img_set_src(); lv_obj.c: lv_obj_clear_flag()).
 *
 * Tutaj te cztery funkcje sa podmienione przy linkowaniu (--wrap) na wersje,
 * ktore najpierw porownuja. Firmware (src/) jest ten sam, bez zadnej zmiany.
 * Uzywa tego tylko:  ./pomiar_obciazenia.sh --prototyp
 *
 * Wynik to SZACUNEK tego, co da poprawka w dashboard_lvgl_poc.cpp - nie sama
 * poprawka. W firmware trzeba to zrobic w kodzie dashboardu (porownanie przed
 * ustawieniem), a nie przez podmiane funkcji biblioteki.
 * =========================================================================== */
#include <string.h>
#include <lvgl.h>

unsigned long obciazenie_prototyp_pominiete[4] = {0, 0, 0, 0};
unsigned long obciazenie_prototyp_wykonane[4] = {0, 0, 0, 0};

void __real_lv_label_set_text(lv_obj_t * obj, const char * text);
void __wrap_lv_label_set_text(lv_obj_t * obj, const char * text)
{
    if(text != NULL) {
        const char * teraz = lv_label_get_text(obj);
        if(teraz != NULL && teraz != text && strcmp(teraz, text) == 0) {
            obciazenie_prototyp_pominiete[0]++;
            return;
        }
    }
    obciazenie_prototyp_wykonane[0]++;
    __real_lv_label_set_text(obj, text);
}

void __real_lv_obj_set_local_style_prop(lv_obj_t * obj, lv_style_prop_t prop, lv_style_value_t value, lv_style_selector_t selector);
void __wrap_lv_obj_set_local_style_prop(lv_obj_t * obj, lv_style_prop_t prop, lv_style_value_t value, lv_style_selector_t selector)
{
    /* Pomijane tylko wtedy, gdy WSZYSTKIE bajty wartosci sa takie same - w razie
     * watpliwosci (np. smieci w nieuzywanej czesci unii) wywolanie idzie dalej. */
    lv_style_value_t teraz;
    memset(&teraz, 0, sizeof(teraz));
    if(lv_obj_get_local_style_prop(obj, prop, &teraz, selector) == LV_RES_OK && memcmp(&teraz, &value, sizeof(teraz)) == 0) {
        obciazenie_prototyp_pominiete[1]++;
        return;
    }
    obciazenie_prototyp_wykonane[1]++;
    __real_lv_obj_set_local_style_prop(obj, prop, value, selector);
}

void __real_lv_img_set_src(lv_obj_t * obj, const void * src);
void __wrap_lv_img_set_src(lv_obj_t * obj, const void * src)
{
    if(src != NULL && lv_img_get_src(obj) == src) {
        obciazenie_prototyp_pominiete[2]++;
        return;
    }
    obciazenie_prototyp_wykonane[2]++;
    __real_lv_img_set_src(obj, src);
}

void __real_lv_obj_clear_flag(lv_obj_t * obj, lv_obj_flag_t f);
void __wrap_lv_obj_clear_flag(lv_obj_t * obj, lv_obj_flag_t f)
{
    if(!lv_obj_has_flag_any(obj, f)) {
        obciazenie_prototyp_pominiete[3]++;
        return;
    }
    obciazenie_prototyp_wykonane[3]++;
    __real_lv_obj_clear_flag(obj, f);
}
