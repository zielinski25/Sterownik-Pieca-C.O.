#pragma once
// ============================================================================
// HISTORIA ZMIAN (ten plik) — najnowsze na gorze, kategorie jak w CHANGELOG.md
// ============================================================================
// [2026-09-05] Dodano: lv_ikona_alarm_duza (opakowanie IKONA_ALARM_DUZA z
//   ikony.h w lv_img_dsc_t) dla nowego EKRANU ALARMOWEGO - patrz
//   PLAN_EKRAN_ALARMOWY.md oraz CHANGELOG.md [Unreleased].
// ============================================================================
// [LVGL - KROK 4] Te same bitmapy co w ikony.h (wygenerowane offline z 8x
// nadprobkowaniem + LANCZOS), tylko opakowane w lv_img_dsc_t zeby LVGL je
// zrozumial. LV_IMG_CF_ALPHA_1BIT = "1 bit na piksel, ustawiony bit = kropka
// widoczna" - format uzywa DOKLADNIE tego samego pakowania bajtow (MSB
// pierwszy, wiersz padowany do pelnego bajtu) co nasze recznie spakowane
// dane, wiec zero konwersji danych, tylko naglowek.
//
// Kluczowe: obraz w tym formacie jest PRZEBARWIALNY w runtime przez
// lv_obj_set_style_img_recolor() + lv_obj_set_style_img_recolor_opa(...,
// LV_OPA_COVER) - dokladnie ten sam mechanizm "kolor jako stan", ktory masz
// teraz w tresciKafelka()/kolorIkony. Nie trzeba trzymac osobnej bitmapy per
// kolor.
//
// Makro przyjmuje "rozmiar" jako parametr zamiast sztywnego IKONA_ROZMIAR -
// dlon (30px) i ikona pompy (32px) maja rozne wymiary od reszty (40px),
// patrz komentarze w ikony.h.
#include <lvgl.h>
#include "ikony.h"

#define LVGL_IKONA_DSC(nazwa, dane, rozmiar) \
  const lv_img_dsc_t nazwa = { \
    .header = { \
      .cf = LV_IMG_CF_ALPHA_1BIT, \
      .always_zero = 0, \
      .reserved = 0, \
      .w = rozmiar, \
      .h = rozmiar, \
    }, \
    .data_size = sizeof(dane), \
    .data = dane, \
  };

LVGL_IKONA_DSC(lv_ikona_plomien, IKONA_PLOMIEN, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_kropla, IKONA_KROPLA, IKONA_ROZMIAR)
// [Partia 3, ta sesja] Wariant 32px dla kafelka WILGOTNOSC - patrz obszerny
// komentarz przy IKONA_KROPLA_MALA w ikony.h (m.in. dlaczego NIE zoom).
LVGL_IKONA_DSC(lv_ikona_kropla_mala, IKONA_KROPLA_MALA, IKONA_KROPLA_MALA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_suwak, IKONA_SUWAK, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_alarm, IKONA_ALARM, IKONA_ROZMIAR)
// [EKRAN ALARMOWY, ta sesja] Duzy wariant do pelnoekranowej nakladki alarmu
// (main ikona, animowana panel_anim_recolor_opa_cb) - patrz
// PLAN_EKRAN_ALARMOWY.md i obszerny komentarz przy IKONA_ALARM_DUZA w
// ikony.h.
LVGL_IKONA_DSC(lv_ikona_alarm_duza, IKONA_ALARM_DUZA, IKONA_ALARM_DUZA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_recznie, IKONA_RECZNIE, IKONA_RECZNIE_ROZMIAR)

// [UI-POMPA] Wybrany wariant ("wentylator", 4 lopatki) - 8 klatek pelnego
// obrotu co 45 st., cyklicznie podmienianych przez pompa_wirnik_timer_cb()
// w dashboard_lvgl_poc.cpp. Zastepuje POPRZEDNIE lv_ikona_pompa_obudowa +
// lv_ikona_pompa_wirnik_0..3 (dwuwarstwowe podejscie obudowa+wirnik) -
// patrz komentarz w ikony.h co do tego, dlaczego wybrany ksztalt nie
// potrzebuje osobnej warstwy obudowy.
LVGL_IKONA_DSC(lv_ikona_pompa_0, IKONA_POMPA_0, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_1, IKONA_POMPA_1, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_2, IKONA_POMPA_2, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_3, IKONA_POMPA_3, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_4, IKONA_POMPA_4, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_5, IKONA_POMPA_5, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_6, IKONA_POMPA_6, IKONA_POMPA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pompa_7, IKONA_POMPA_7, IKONA_POMPA_ROZMIAR)

// [UI-ZEWN] 3 warianty przelaczane w runtime wg. temperatury - patrz
// obszerny komentarz przy danych w ikony.h (progi 0/15 st.C). IKONA_ZEWN_
// ROZMIAR = 40 (jak IKONA_ROZMIAR standardowych ikon) - ZEWN uzywa tego
// samego rozmiaru co reszta 2-wierszowych kafelkow, makro ponizej samo to
// odzwierciedla bez zadnych zmian w tym pliku.
LVGL_IKONA_DSC(lv_ikona_zewn_neutralny, IKONA_ZEWN_NEUTRALNY, IKONA_ZEWN_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_zewn_mroz, IKONA_ZEWN_MROZ, IKONA_ZEWN_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_zewn_cieplo, IKONA_ZEWN_CIEPLO, IKONA_ZEWN_ROZMIAR)
// [DESZCZ-ETAP4, PLAN_DESZCZ_ZEWN.md] Stan "nadciaga" (deszcz_stan==1) na
// kafelku ZEWN - patrz obszerny komentarz przy IKONA_ZEWN_DESZCZ w ikony.h.
LVGL_IKONA_DSC(lv_ikona_zewn_deszcz, IKONA_ZEWN_DESZCZ, IKONA_ZEWN_ROZMIAR)

// [UI-PIEC CO/SR] Grzejnik - kafelek polaczonej temperatury pieca C.O. +
// srednia, patrz obszerny komentarz przy danych w ikony.h. [FIX tej sesji]
// IKONA_ROZMIAR urosl z 32 na 40 (patrz ikony.h), ale GRZEJNIK zostal
// CELOWO przy 24px - wlasny IKONA_GRZEJNIK_ROZMIAR, odlaczony od
// IKONA_ROZMIAR - bo kafelek_ogrz_polaczony() ma 3 wiersze (etykieta+ikona /
// glowna wartosc / SR) w tej samej wysokosci karty (kh=90) co 2-wierszowe
// kafelki, wiec nie ma pionowego zapasu na wiekszej ikonie bez kolizji
// glownej wartosci z wierszem SR - przeliczone, nie zgadywane.
LVGL_IKONA_DSC(lv_ikona_grzejnik, IKONA_GRZEJNIK, IKONA_GRZEJNIK_ROZMIAR)

// [UI-BOJLER, v2] Dwie warstwy 40x40 (tyle co standardowe ikony, bo kh=90
// daje dosc miejsca) - kontur zbiornika + woda, oddzielnie przebarwiane w
// kafelek() (patrz ikona2/kolorIkony2 w dashboard_lvgl_poc.cpp). Zastepuje
// poprzednia jednowarstwowa lv_ikona_bojler.
LVGL_IKONA_DSC(lv_ikona_bojler_kontur, IKONA_BOJLER_KONTUR, IKONA_BOJLER_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_bojler_woda, IKONA_BOJLER_WODA, IKONA_BOJLER_ROZMIAR)

// [UI-CISNIENIE/POKOJ] Nowe ikony pod kafelki CISNIENIE i POKOJ w docelowej
// siatce 4x3. Rozmiar IKONA_ROZMIAR (40, standard).
LVGL_IKONA_DSC(lv_ikona_cisnienie, IKONA_CISNIENIE, IKONA_ROZMIAR)
// [FIX, ta sesja - kolorowe logo POKOJ] Dwie warstwy zamiast jednej -
// ten sam wzorzec co lv_ikona_bojler_kontur/lv_ikona_bojler_woda ponizej.
LVGL_IKONA_DSC(lv_ikona_pokoj_dach, IKONA_POKOJ_DACH, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_pokoj_termometr, IKONA_POKOJ_TERMOMETR, IKONA_ROZMIAR)

// [UI-WYKRESY, ta sesja] Ikona kafelka "Wykresy" (wejscie na ekran historii
// pomiarow) - patrz obszerny komentarz przy danych w ikony.h.
LVGL_IKONA_DSC(lv_ikona_wykresy, IKONA_WYKRESY, IKONA_ROZMIAR)

// [UI-PANEL, ta sesja] Kolektor sloneczny (slonce nad przechylonym panelem
// grzewczym 3-segmentowym) - patrz obszerny komentarz przy danych w
// ikony.h co do tego, dlaczego PANEL przestal uzywac ikony plomienia.
// [UI-PANEL-KOLOR, ta sesja] Rozbite na dwie rozlaczne warstwy (jak BOJLER
// kontur+woda) zeby slonce moglo byc na stale zolte (KOL_SLONCE) niezaleznie
// od koloru/alarmu kolektora (KOL_ZAR/KOL_ERR) - patrz obszerny komentarz
// przy danych IKONA_PANEL_SLONCE/IKONA_PANEL_KOLEKTOR w ikony.h oraz
// wywolanie kafelek() dla PANEL w dashboard_lvgl_poc.cpp.
LVGL_IKONA_DSC(lv_ikona_panel_slonce, IKONA_PANEL_SLONCE, IKONA_PANEL_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_panel_kolektor, IKONA_PANEL_KOLEKTOR, IKONA_PANEL_ROZMIAR)

// [UI-POMPA-PANEL, ta sesja] Ikony trybu w panelu wyboru POMPA - patrz
// obszerny komentarz przy danych w ikony.h. AUTO ma 8 klatek (pelny obrot,
// ten sam wzorzec co lv_ikona_pompa_0..7 wyzej, podmieniane cyklicznie przez
// wlasny timer w dashboard_lvgl_poc.cpp, uruchamiany/zatrzymywany razem z
// panelem zeby nie krecic w tle, gdy panel jest ukryty). WLACZ RECZNIE
// swiadomie NIE ma tu wlasnego wpisu - uzywa wprost lv_ikona_recznie
// (zdefiniowanej wyzej).
LVGL_IKONA_DSC(lv_ikona_auto_0, IKONA_AUTO_0, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_1, IKONA_AUTO_1, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_2, IKONA_AUTO_2, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_3, IKONA_AUTO_3, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_4, IKONA_AUTO_4, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_5, IKONA_AUTO_5, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_6, IKONA_AUTO_6, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_auto_7, IKONA_AUTO_7, IKONA_AUTO_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_recznie_przekreslona, IKONA_RECZNIE_PRZEKRESLONA, IKONA_RECZNIE_ROZMIAR)

// [UI-POMPA-STRONA, ta sesja] Ikony wyboru strategii pracy pompy (Wybor 1/2/3
// na centrali) + strzalka wstecz - patrz obszerny komentarz przy danych w
// ikony.h co do znaczenia kazdego trybu.
LVGL_IKONA_DSC(lv_ikona_trociniak, IKONA_TROCINIAK, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_kopciuch, IKONA_KOPCIUCH, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_automatyczny, IKONA_AUTOMATYCZNY, IKONA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_wstecz, IKONA_WSTECZ, IKONA_ROZMIAR)

// [UI-SERWO-PANEL, Czesc 1, ta sesja] Tarcza z checkmarkiem - ikona trybu
// BEZPIECZNA w panelu wyboru trybu SERWO (KLAPA/SYBERKA) - patrz obszerny
// komentarz przy danych w ikony.h.
LVGL_IKONA_DSC(lv_ikona_bezpieczna, IKONA_BEZPIECZNA, IKONA_ROZMIAR)

// [UI-SERWO-USTAWIENIA, ta sesja] Zebatka - wejscie na ekran USTAWIENIA
// SERWA (6 parametrow automatyki klapy/syberka) - patrz obszerny komentarz
// przy danych w ikony.h.
LVGL_IKONA_DSC(lv_ikona_zebatka, IKONA_ZEBATKA, IKONA_ROZMIAR)

// [UI-INTERNET, ta sesja] Dwie rozlaczne warstwy globusu (siatka+ladu) -
// patrz obszerny komentarz przy danych IKONA_GLOBUS_SIATKA/IKONA_GLOBUS_LADY
// w ikony.h. Wlasny rozmiar 16px (IKONA_GLOBUS_ROZMIAR), NIE standardowe 40px.
LVGL_IKONA_DSC(lv_ikona_globus_siatka, IKONA_GLOBUS_SIATKA, IKONA_GLOBUS_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_globus_lady, IKONA_GLOBUS_LADY, IKONA_GLOBUS_ROZMIAR)

// [UI-SERWO-GLOWNY-KAFELEK, ta sesja] Male (20px) warianty ikon trybu
// SERWO - patrz obszerny komentarz przy danych IKONA_*_MALA w ikony.h co
// do tego, dlaczego to osobne bitmapy a nie lv_img_set_zoom().
LVGL_IKONA_DSC(lv_ikona_auto_0_mala, IKONA_AUTO_0_MALA, IKONA_TRYB_MALA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_recznie_mala, IKONA_RECZNIE_MALA, IKONA_TRYB_MALA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_bezpieczna_mala, IKONA_BEZPIECZNA_MALA, IKONA_TRYB_MALA_ROZMIAR)

// [UI-POMPA-GLOWNY-KAFELEK, ta sesja] Male (20px) warianty 3 ikon strategii
// pracy pompy (g_pompaWybor) - patrz obszerny komentarz przy danych
// IKONA_TROCINIAK_MALA/IKONA_KOPCIUCH_MALA/IKONA_AUTOMATYCZNY_MALA w
// ikony.h. Uzyte na kafelku glownym POMPA jako staly znaczek strategii,
// patrz pompa_tile_odswiez() w dashboard_lvgl_poc.cpp.
LVGL_IKONA_DSC(lv_ikona_trociniak_mala, IKONA_TROCINIAK_MALA, IKONA_TRYB_MALA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_kopciuch_mala, IKONA_KOPCIUCH_MALA, IKONA_TRYB_MALA_ROZMIAR)
LVGL_IKONA_DSC(lv_ikona_automatyczny_mala, IKONA_AUTOMATYCZNY_MALA, IKONA_TRYB_MALA_ROZMIAR)

#undef LVGL_IKONA_DSC
