package com.sterownikco.pro.core

import android.content.Context
import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import org.json.JSONObject
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/**
 * Encrypted, device-protected copy of the Firebase session needed only for
 * alarm monitoring before the first unlock after boot. Never store raw tokens
 * in device-protected preferences.
 *
 * Android 9+ is required for a Keystore key explicitly usable while locked.
 * Older supported Android versions can still start monitoring after first unlock.
 */
internal object DeviceBootCredentials {
    private const val PREFS_NAME = "piec_alarm_boot_session"
    private const val PREF_CIPHERTEXT = "session_v1"
    private const val KEY_ALIAS = "sterownikco.directboot.session.aes.v1"
    private const val IV_BYTES = 12
    private const val TAG_BITS = 128

    data class Session(val apiKey: String, val idToken: String, val refreshToken: String)

    fun supportsLockedBoot(): Boolean = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P

    fun read(context: Context): Session? = synchronized(this) {
        if (!supportsLockedBoot()) return@synchronized null
        val encoded = prefs(context).getString(PREF_CIPHERTEXT, null) ?: return@synchronized null
        try {
            val sealed = Base64.decode(encoded, Base64.NO_WRAP)
            if (sealed.size <= IV_BYTES) return@synchronized null
            val iv = sealed.copyOfRange(0, IV_BYTES)
            val ciphertext = sealed.copyOfRange(IV_BYTES, sealed.size)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.DECRYPT_MODE, keyStoreKey(), GCMParameterSpec(TAG_BITS, iv))
            cipher.updateAAD(context.applicationContext.packageName.toByteArray(Charsets.UTF_8))
            val json = JSONObject(String(cipher.doFinal(ciphertext), Charsets.UTF_8))
            val refreshToken = json.optString("refreshToken")
            if (refreshToken.isBlank()) return@synchronized null
            Session(
                apiKey = json.optString("apiKey").ifBlank { Prefs.DEFAULT_FB_API_KEY },
                idToken = json.optString("idToken"),
                refreshToken = refreshToken
            )
        } catch (e: Exception) {
            // Keep the ciphertext for post-unlock recovery; never log its contents.
            null
        }
    }

    fun write(context: Context, apiKey: String, idToken: String, refreshToken: String): Boolean = synchronized(this) {
        if (!supportsLockedBoot() || refreshToken.isBlank()) return@synchronized false
        try {
            val json = JSONObject()
                .put("apiKey", apiKey)
                .put("idToken", idToken)
                .put("refreshToken", refreshToken)
            val cipher = Cipher.getInstance("AES/GCM/NoPadding")
            cipher.init(Cipher.ENCRYPT_MODE, keyStoreKey())
            cipher.updateAAD(context.applicationContext.packageName.toByteArray(Charsets.UTF_8))
            val sealed = cipher.iv + cipher.doFinal(json.toString().toByteArray(Charsets.UTF_8))
            // commit() makes a successful login/token rotation durable before the app can be rebooted.
            prefs(context).edit()
                .putString(PREF_CIPHERTEXT, Base64.encodeToString(sealed, Base64.NO_WRAP))
                .commit()
        } catch (e: Exception) {
            false
        }
    }

    fun clear(context: Context, invalidateKey: Boolean = false) {
        synchronized(this) {
            val removed = try { prefs(context).edit().remove(PREF_CIPHERTEXT).commit() } catch (e: Exception) { false }
            if (invalidateKey || !removed) {
                // If storage removal fails, delete the wrapping key so stale ciphertext is unusable.
                try {
                    KeyStore.getInstance("AndroidKeyStore").apply { load(null) }.deleteEntry(KEY_ALIAS)
                } catch (e: Exception) { /* ignore */ }
            }
        }
    }

    private fun prefs(context: Context) = context.applicationContext
        .createDeviceProtectedStorageContext()
        .getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    private fun keyStoreKey(): SecretKey {
        val keyStore = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        (keyStore.getKey(KEY_ALIAS, null) as? SecretKey)?.let { return it }

        val builder = KeyGenParameterSpec.Builder(
            KEY_ALIAS,
            KeyProperties.PURPOSE_ENCRYPT or KeyProperties.PURPOSE_DECRYPT
        ).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true)
            .setKeySize(256)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            builder.setUnlockedDeviceRequired(false)
        }
        return KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore")
            .apply { init(builder.build()) }
            .generateKey()
    }
}
