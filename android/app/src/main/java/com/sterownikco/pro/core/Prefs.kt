package com.sterownikco.pro.core

import android.content.Context
import android.content.SharedPreferences

/* ══════════════════════════════════════════════════════════════════════════
   Odpowiednik `safeStorage` z Piec.html (localStorage + sessionStorage).
   Na Androidzie: jedna SharedPreferences; klucze bez zmian (piec_*).
   ══════════════════════════════════════════════════════════════════════════ */
class Prefs(ctx: Context) {
    private val sp: SharedPreferences = ctx.getSharedPreferences("piec_hmi", Context.MODE_PRIVATE)

    fun get(key: String): String? = sp.getString(key, null)?.ifEmpty { null }
    fun set(key: String, value: String) = sp.edit().putString(key, value).apply()
    fun remove(key: String) = sp.edit().remove(key).apply()

    fun getInt(key: String, def: Int): Int = try { sp.getString(key, null)?.toIntOrNull() ?: def } catch (e: Exception) { def }
    fun setInt(key: String, v: Int) = sp.edit().putString(key, v.toString()).apply()

    fun getLong(key: String, def: Long): Long = try { sp.getString(key, null)?.toLongOrNull() ?: def } catch (e: Exception) { def }
    fun setLong(key: String, v: Long) = sp.edit().putString(key, v.toString()).apply()

    fun getBool(key: String, def: Boolean): Boolean = try { sp.getString(key, null)?.toBooleanStrictOrNull() ?: def } catch (e: Exception) { def }
    fun setBool(key: String, v: Boolean) = sp.edit().putString(key, v.toString()).apply()

    companion object {
        // poświadczenia wbudowane w firmware (main_centrala.cpp) — jak w Piec.html
        const val DEFAULT_FB_API_KEY = "AIzaSyDcheuRNcNo4mzNpaTzn-19Ntw62djkfVU"
        const val DEFAULT_FB_EMAIL = "piec_co_boot@akwarium.local"
        const val DEFAULT_FB_PASS = "PcTg8plOcvRrMSojv79X"
        const val DEFAULT_CMD_TOKEN = "sterownikco-cmd-2026"

        const val K_API_KEY = "piec_fb_api_key"
        const val K_EMAIL = "piec_fb_email"
        const val K_PASS = "piec_fb_password"
        const val K_CMD_TOKEN = "piec_cmd_token"
        const val K_ID_TOKEN = "piec_fb_token"
        const val K_REF_TOKEN = "piec_fb_ref_token"
        const val K_TG_TOKEN = "piec_tg_token"
        const val K_TG_CHAT = "piec_tg_chat_id"
        const val K_CHART_PREFS = "piec_chart_prefs_v2"
        const val K_LAT = "piec_weather_lat"
        const val K_LON = "piec_weather_lon"
        const val K_SOLAR_ARCHIVE = "solar_archive_data"
    }
}
