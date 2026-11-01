#!/usr/bin/env python3
# [ETAP 221] Mutanty: celowo psujemy JEDNO miejsce w kodzie i sprawdzamy, czy
# test to wykryje. Mutant przechodzi tylko wtedy, gdy zrobi sie CZERWONO na
# kontroli, ktora tego miejsca pilnuje.
#
# Psujemy kopie w katalogu tymczasowym; pliki w repo zostaja nietkniete.
import os
import shutil

# (nazwa, plik, szukane, zamiana, kontrole_ktore_musza_siasc)
MUTANTY = [
    (
        "M1_trasa_history_goly_napis",
        "trasy221.inc",
        'wifiWebServer.on(AsyncURIMatcher::exact("/history"), HTTP_ANY,',
        'wifiWebServer.on("/history", HTTP_ANY,',
        ["A01_dluga_200"],
    ),
    (
        "M2_link_metoda_post",
        "trasy221.inc",
        'wifiWebServer.on(AsyncURIMatcher::exact("/history/link"), HTTP_GET,',
        'wifiWebServer.on(AsyncURIMatcher::exact("/history/link"), HTTP_POST,',
        ["C01_link_200_hex32"],
    ),
    (
        "M3_trasa_dluga_podaje_krotki",
        "trasy221.inc",
        "    historiaPobranie.obsluz(request, historia212::Strumien::Dluga);",
        "    historiaPobranie.obsluz(request, historia212::Strumien::Krotki);",
        ["A01_dluga_200"],
    ),
    (
        "M4_bramka_zawsze_krotka_sciezka",
        "bramka221.inc",
        "    return (s == historia212::Strumien::Krotki) ? historia211::kShortVfsPath",
        "    return (s == s) ? historia211::kShortVfsPath",
        ["A01_dluga_200"],
    ),
    (
        "M5_bez_content_disposition",
        "HistoriaPobranie.h",
        '    odp->addHeader("Content-Disposition",',
        '    if (false) odp->addHeader("Content-Disposition",',
        ["A03_naglowki_csv"],
    ),
    (
        "M6_bez_content_length",
        "HistoriaPobranie.h",
        "    _sendContentLength = true;",
        "    _sendContentLength = false;",
        ["A03_naglowki_csv"],
    ),
    (
        "M7_odpowiedz_bez_zrodla",
        "HistoriaPobranie.h",
        "bool _sourceValid() const override { return !awaria_ && pin_ != nullptr && pin_->active(); }",
        "bool _sourceValid() const override { return false; }",
        ["A02_krotka_200"],
    ),
    (
        "M8_token_zawsze_zgodny",
        "HistoriaPobranie.h",
        "  return roznica == 0;",
        "  return true;",
        ["B02_zly_token_401"],
    ),
    (
        "M9_bez_kontroli_zajetosci",
        "HistoriaPobranie.h",
        "    if (zajeta_) {",
        "    if (false && zajeta_) {",
        ["D01_drugie_pobranie_503"],
    ),
    (
        "M10_zuzycie_biletu_zawsze_krotki",
        "HistoriaPobranie.h",
        "strumien == Strumien::Dluga ? historia213::kDluga : historia213::kKrotki",
        "strumien == Strumien::Dluga ? historia213::kKrotki : historia213::kKrotki",
        ["C06_bilet_dlugi_na_dluga_200"],
    ),
    (
        "M11_bez_cache_control",
        "HistoriaPobranie.h",
        '    if (r != nullptr) r->addHeader("Cache-Control", "no-store");',
        '    if (false) r->addHeader("Cache-Control", "no-store");',
        ["C02_link_no_store"],
    ),
    (
        "M12_pusty_bufor_odpowiedzi",
        "HistoriaPobranie.h",
        "    if (r.code == historia211::ReadCode::Data) return r.bytes;",
        "    if (r.code == historia211::ReadCode::Data) return 0;",
        ["A02_krotka_200"],
    ),
]


def przygotuj(katalog_zrodlowy, katalog_docelowy, mutant):
    """Kopia drzewa testu i podmiana JEDNEGO napisu w JEDNYM pliku."""
    nazwa, plik, szukane, zamiana, kontrole = mutant
    if os.path.exists(katalog_docelowy):
        shutil.rmtree(katalog_docelowy)
    shutil.copytree(katalog_zrodlowy, katalog_docelowy)
    cel = os.path.join(katalog_docelowy, "nasz", plik) if plik != "Test.cpp" else os.path.join(katalog_docelowy, "Test.cpp")
    with open(cel, encoding="utf-8") as f:
        tekst = f.read()
    ile = tekst.count(szukane)
    if ile != 1:
        raise ValueError(f"{nazwa}: napis do podmiany wystepuje {ile} razy (musi byc dokladnie 1)")
    with open(cel, "w", encoding="utf-8") as f:
        f.write(tekst.replace(szukane, zamiana, 1))
    return nazwa, kontrole
