#!/usr/bin/env python3
"""Wycina z main_centrala.cpp kod [v3.58.7 START-TG] do sprawdzenia prawdziwymi
naglowkami ESP32 (sprawdz_naglowki_esp32.sh). Wycinane doslownie:
  - blok czystych funkcji [v3.58.7 START-TG-BEGIN]..[-END],
  - kod klejacy wyslijWiadomoscStartowa() wraz ze zmiennymi g_startTg*,
  - fragment TAGCHECK z potwierdzPartycjeJesliCzeka() (od komentarza
    "[v3.58.7 START-TG] Dwa rozne bledy" do konca galezi r == ESP_OK),
  - definicja OTA_CONFIRM_UPTIME_MS ze static_assert.
Otoczenie (config, tgEnabled, logBootCounter, zapiszLog, sendTelegramMessage,
porownajWersje, bylCrash, resetReasonNaTekst) jest tylko ZADEKLAROWANE - te same
typy i sygnatury co w pliku centrali (sprawdzane tu po tekscie).
Uzycie: wytnij_start_tg.py <main_centrala.cpp> <wyjscie.cpp>"""
import re, sys
src, dst = sys.argv[1], sys.argv[2]
t = open(src, encoding="utf-8").read()

def miedzy(a, b, od=0):
    i = t.index(a, od); j = t.index(b, i)
    return t[i:j], j

# Sygnatury otoczenia - musza istniec doslownie w pliku centrali.
for wzor in ["bool    tgEnabled  = false;", "Preferences config;", "uint32_t logBootCounter",
             "void zapiszLog(const String &tresc);", "void zapiszLog(const __FlashStringHelper *tresc);",
             "bool   sendTelegramMessage(const String& text, const String& replyMarkup = String());",
             "const char* resetReasonNaTekst(esp_reset_reason_t powod);",
             "bool bylCrash(esp_reset_reason_t powod);",
             "int porownajWersje(const String &a, const String &b) {", "RTC_NOINIT_ATTR uint32_t logBootCounter;",
             "const char* FIRMWARE_VERSION = \"v3.58.7\";"]:
    if wzor not in t:
        sys.exit("brak w pliku centrali: " + wzor)

blok, k = miedzy("// [v3.58.7 START-TG-BEGIN]", "// [v3.58.7 START-TG-END]")
klej, k = miedzy("// [v3.58.7 START-TG] Kod klejacy wiadomosci startowej.", "// ── SCHEDULER", k)
m = re.search(r"#define OTA_CONFIRM_UPTIME_MS[^\n]*\nstatic_assert\([^\n]*\n", t)
if not m: sys.exit("brak OTA_CONFIRM_UPTIME_MS ze static_assert")
define_otc = m.group(0)
tag_i = t.index("      // [v3.58.7 START-TG] Dwa rozne bledy")
tag_j = t.index("    } else {", tag_i)
tagcheck = t[tag_i:tag_j]

out = f'''// WYCINEK z main_centrala.cpp v3.58.7 ([v3.58.7 START-TG]) - wygenerowany przez wytnij_start_tg.py.
#include <Arduino.h>
#include <Preferences.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
// ---- otoczenie: deklaracje jak w pliku centrali ----
const char* FIRMWARE_VERSION = "v3.58.7";
bool    tgEnabled  = false;
Preferences config;
uint32_t logBootCounter = 0;
void zapiszLog(const String &tresc);
void zapiszLog(const __FlashStringHelper *tresc);
bool   sendTelegramMessage(const String& text, const String& replyMarkup = String());
const char* resetReasonNaTekst(esp_reset_reason_t powod);
int porownajWersje(const String &a, const String &b);
bool bylCrash(esp_reset_reason_t powod);
// ---- blok czystych funkcji ----
{blok}
// ---- kod klejacy ----
{klej}
// ---- OTA_CONFIRM_UPTIME_MS (dalej w pliku centrali) ----
{define_otc}
// ---- TAGCHECK w potwierdzPartycjeJesliCzeka(): fragment galezi r == ESP_OK ----
static void tagcheck_fragment(const String& selfTag) {{
  {{
{tagcheck}    }}
  }}
}}
// ---- wywolania w swoich miejscach (odtworzone) ----
static void tgTaskFn_fragment() {{
  wyslijWiadomoscStartowa();
}}
static void wyslijRaportNadzoru_fragment() {{
  g_startTgNadzor = true;
}}
void uzyj_wszystkiego() {{ tgTaskFn_fragment(); wyslijRaportNadzoru_fragment(); tagcheck_fragment(String("3.58.5")); }}
'''
open(dst, "w", encoding="utf-8").write(out)
print("zapisano %s (%d linii)" % (dst, out.count("\n")))
