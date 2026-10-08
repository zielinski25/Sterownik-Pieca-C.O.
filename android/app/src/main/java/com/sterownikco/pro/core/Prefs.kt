package com.sterownikco.pro.core

import android.content.Context
import android.content.SharedPreferences
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/* ══════════════════════════════════════════════════════════════════════════
   Odpowiednik `safeStorage` z Piec.html (localStorage + sessionStorage).
   Na Androidzie: jedna SharedPreferences; klucze bez zmian (piec_*).
   ══════════════════════════════════════════════════════════════════════════ */
class Prefs(ctx: Context) {
    private val sp: SharedPreferences = ctx.getSharedPreferences("piec_hmi", Context.MODE_PRIVATE)

    fun get(key: String): String? = sp.getString(key, null)?.ifEmpty { null }
    fun set(key: String, value: String) = sp.edit().putString(key, value).apply()
    fun remove(key: String) = sp.edit().remove(key).apply()

    /** Store passwords using an AES-GCM key held by Android Keystore. */
    fun setSecret(key: String, value: String): Boolean {
        if (value.isEmpty()) { remove(key); return true }
        return try {
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, credentialKey())
            val sealed = cipher.iv + cipher.doFinal(value.toByteArray(Charsets.UTF_8))
            this.set(key, SECRET_PREFIX + Base64.encodeToString(sealed, Base64.NO_WRAP))
            true
        } catch (e: Exception) {
            false
        }
    }

    /** Transparently migrates any password written as plain text by older APK builds. */
    fun getSecret(key: String): String? {
        val stored = get(key) ?: return null
        if (!stored.startsWith(SECRET_PREFIX)) {
            return if (setSecret(key, stored)) stored else {
                remove(key)
                null
            }
        }
        return try {
            val sealed = Base64.decode(stored.removePrefix(SECRET_PREFIX), Base64.NO_WRAP)
            if (sealed.size <= GCM_IV_BYTES) throw IllegalArgumentException("Invalid encrypted secret")
            val iv = sealed.copyOfRange(0, GCM_IV_BYTES)
            val ciphertext = sealed.copyOfRange(GCM_IV_BYTES, sealed.size)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, credentialKey(), GCMParameterSpec(GCM_TAG_BITS, iv))
            String(cipher.doFinal(ciphertext), Charsets.UTF_8)
        } catch (e: Exception) {
            // A lost/invalidated Keystore key must never fall back to plaintext.
            remove(key)
            null
        }
    }

    private fun credentialKey(): SecretKey {
        val keyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (keyStore.getKey(CREDENTIAL_KEY_ALIAS, null) as? SecretKey)?.let { return it }
        val generator = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
        generator.init(
            KeyGenParameterSpec.Builder(
                CREDENTIAL_KEY_ALIAS,
                KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
            ).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setRandomizedEncryptionRequired(true)
                .setKeySize(256)
                .build()
        )
        return generator.generateKey()
    }

    fun getInt(key: String, def: Int): Int = try { sp.getString(key, null)?.toIntOrNull() ?: def } catch (e: Exception) { def }
    fun setInt(key: String, v: Int) = sp.edit().putString(key, v.toString()).apply()

    fun getLong(key: String, def: Long): Long = try { sp.getString(key, null)?.toLongOrNull() ?: def } catch (e: Exception) { def }
    fun setLong(key: String, v: Long) = sp.edit().putString(key, v.toString()).apply()

    fun getBool(key: String, def: Boolean): Boolean = try { sp.getString(key, null)?.toBooleanStrictOrNull() ?: def } catch (e: Exception) { def }
    fun setBool(key: String, v: Boolean) = sp.edit().putString(key, v.toString()).apply()

    companion object {
        private const val SECRET_PREFIX = "enc:v1:"
        private const val CREDENTIAL_KEY_ALIAS = "sterownikco.credentials.aes.v1"
        private const val GCM_IV_BYTES = 12
        private const val GCM_TAG_BITS = 128

        // poświadczenia wbudowane w firmware (main_centrala.cpp) — jak w Piec.html
        const val DEFAULT_FB_API_KEY = "AIzaSyDcheuRNcNo4mzNpaTzn-19Ntw62djkfVU"
        const val DEFAULT_FB_EMAIL = "piec_co_boot@akwarium.local"
        const val DEFAULT_FB_PASS = "PcTg8plOcvRrMSojv79X"
        const val DEFAULT_CMD_TOKEN = "sterownikco-cmd-2026"

        const val K_API_KEY = "piec_fb_api_key"
        const val K_EMAIL = "piec_fb_email"
        const val K_PASS = "piec_fb_password"
        const val K_REMEMBER_CREDS = "piec_fb_remember_credentials"
        const val K_CMD_TOKEN = "piec_cmd_token"
        const val K_DASH_WIDGETS = "piec_dashboard_widgets_v1"
        const val K_ID_TOKEN = "piec_fb_token"
        const val K_REF_TOKEN = "piec_fb_ref_token"
        const val K_TG_TOKEN = "piec_tg_token"
        const val K_TG_CHAT = "piec_tg_chat_id"
        const val K_CHART_PREFS = "piec_chart_prefs_v2"
        const val K_LAT = "piec_weather_lat"
        const val K_LON = "piec_weather_lon"
        const val K_SOLAR_ARCHIVE = "solar_archive_data" // legacy/unverified; never loaded as real telemetry
        const val K_SOLAR_REAL_ARCHIVE = "solar_real_archive_v1"
    }
}
