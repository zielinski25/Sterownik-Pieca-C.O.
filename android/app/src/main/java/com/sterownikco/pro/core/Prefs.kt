package com.sterownikco.pro.core

import android.content.Context
import android.content.SharedPreferences
import android.os.UserManager
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.util.Base64
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

/* ══════════════════════════════════════════════════════════════════════════
   Credential-encrypted preferences hold normal UI data. Alarm decisions use
   a small device-protected allowlist so the boot monitor can run before unlock.
   Firebase session copies in device-protected storage are encrypted separately.
   ══════════════════════════════════════════════════════════════════════════ */
class Prefs(ctx: Context) : AlarmPrefsStore {
    private val appContext = ctx.applicationContext
    private val deviceContext = appContext.createDeviceProtectedStorageContext()
    private val devicePrefs: SharedPreferences =
        deviceContext.getSharedPreferences(DEVICE_PREFS_NAME, Context.MODE_PRIVATE)
    private val userManager = appContext.getSystemService(Context.USER_SERVICE) as UserManager
    private val credentialPrefs: SharedPreferences by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        appContext.getSharedPreferences(CREDENTIAL_PREFS_NAME, Context.MODE_PRIVATE)
    }

    init {
        migrateAlarmPreferencesIfUnlocked()
    }

    private fun isUserUnlocked(): Boolean = userManager.isUserUnlocked

    private fun firebaseSessionLoggedOut(): Boolean =
        devicePrefs.getBoolean(FIREBASE_SESSION_LOGGED_OUT, false)

    private fun migrateAlarmPreferencesIfUnlocked() {
        if (!isUserUnlocked()) return
        synchronized(BOOT_PREFS_MIGRATION_LOCK) {
            if (devicePrefs.getBoolean(BOOT_PREFS_MIGRATED, false)) return
            val oldValues = credentialPrefs.all
            val editor = devicePrefs.edit()
            for (key in DEVICE_PROTECTED_KEYS) {
                if (!devicePrefs.contains(key)) {
                    (oldValues[key] as? String)?.let { editor.putString(key, it) }
                }
            }
            editor.putBoolean(BOOT_PREFS_MIGRATED, true).commit()
        }
    }

    override fun get(key: String): String? {
        if (key in FIREBASE_SESSION_KEYS) {
            val session = firebaseSession()
            val value = when (key) {
                K_API_KEY -> session.apiKey
                K_ID_TOKEN -> session.idToken
                else -> session.refreshToken
            }
            return value.ifEmpty { null }
        }
        val store = when {
            key in DEVICE_PROTECTED_KEYS -> devicePrefs
            isUserUnlocked() -> credentialPrefs
            else -> return null
        }
        return store.getString(key, null)?.ifEmpty { null }
    }

    override fun set(key: String, value: String) {
        when {
            key in DEVICE_PROTECTED_KEYS -> synchronized(BOOT_PREFS_MIGRATION_LOCK) {
                // Serialize first-unlock migration against alarm writes; DE state must survive reboot.
                devicePrefs.edit().putString(key, value).commit()
                if (key == AlarmCenter.K_MONITOR) {
                    if (value == "0") DeviceBootCredentials.clear(appContext)
                    else if (isUserUnlocked()) firebaseSession()
                }
            }
            key in FIREBASE_SESSION_KEYS -> synchronized(BOOT_PREFS_MIGRATION_LOCK) {
                if (firebaseSessionLoggedOut()) return@synchronized
                val current = firebaseSession()
                val updated = when (key) {
                    K_API_KEY -> current.copy(apiKey = value)
                    K_ID_TOKEN -> current.copy(idToken = value)
                    else -> current.copy(refreshToken = value)
                }
                saveFirebaseSession(updated.apiKey, updated.idToken, updated.refreshToken)
            }
            isUserUnlocked() -> credentialPrefs.edit().putString(key, value).apply()
        }
    }

    override fun remove(key: String) {
        when {
            key in DEVICE_PROTECTED_KEYS -> synchronized(BOOT_PREFS_MIGRATION_LOCK) {
                devicePrefs.edit().remove(key).commit()
            }
            key in FIREBASE_SESSION_KEYS -> clearFirebaseTokens()
            isUserUnlocked() -> {
                val editor = credentialPrefs.edit().remove(key)
                if (key == K_PASS) {
                    editor.commit()
                } else {
                    editor.apply()
                }
            }
        }
    }

    /** Current Firebase session, preferring the encrypted boot copy when monitoring is enabled. */
    internal fun firebaseSession(): DeviceBootCredentials.Session {
        // A DE tombstone is written first on logout, so a crash cannot restore
        // stale tokens from either storage domain on the next boot/unlock.
        if (firebaseSessionLoggedOut()) {
            return DeviceBootCredentials.Session(Prefs.DEFAULT_FB_API_KEY, "", "")
        }
        val monitoring = AlarmCenter.isMonitored(this)
        if (monitoring) {
            val bootSession = DeviceBootCredentials.read(appContext)
            if (bootSession != null) {
                if (isUserUnlocked()) writeCredentialSession(bootSession)
                return bootSession
            }
        } else {
            DeviceBootCredentials.clear(appContext)
        }

        if (!isUserUnlocked()) {
            return DeviceBootCredentials.Session(Prefs.DEFAULT_FB_API_KEY, "", "")
        }
        val session = DeviceBootCredentials.Session(
            apiKey = credentialPrefs.getString(K_API_KEY, null) ?: Prefs.DEFAULT_FB_API_KEY,
            idToken = credentialPrefs.getString(K_ID_TOKEN, null).orEmpty(),
            refreshToken = credentialPrefs.getString(K_REF_TOKEN, null).orEmpty()
        )
        if (monitoring && session.refreshToken.isNotBlank()) {
            if (!DeviceBootCredentials.write(appContext, session.apiKey, session.idToken, session.refreshToken)) {
                DeviceBootCredentials.clear(appContext, invalidateKey = true)
            }
        }
        return session
    }

    /** True only when an encrypted session exists in storage available during Direct Boot. */
    internal fun directBootSupported(): Boolean = DeviceBootCredentials.supportsLockedBoot()

    internal fun directBootReady(): Boolean =
        !firebaseSessionLoggedOut() && AlarmCenter.isMonitored(this) && DeviceBootCredentials.read(appContext) != null

    /** Persist a successful login, then release a previous logout tombstone. */
    internal fun saveFirebaseSession(apiKey: String, idToken: String, refreshToken: String) {
        synchronized(BOOT_PREFS_MIGRATION_LOCK) {
            val normalizedApiKey = apiKey.ifBlank { Prefs.DEFAULT_FB_API_KEY }
            val credentialSaved = if (isUserUnlocked()) {
                credentialPrefs.edit()
                    .putString(K_API_KEY, normalizedApiKey)
                    .putString(K_ID_TOKEN, idToken)
                    .putString(K_REF_TOKEN, refreshToken)
                    .commit()
            } else false

            val monitoring = AlarmCenter.isMonitored(this)
            val bootSaved = if (monitoring && refreshToken.isNotBlank()) {
                DeviceBootCredentials.write(appContext, normalizedApiKey, idToken, refreshToken).also { saved ->
                    if (!saved) DeviceBootCredentials.clear(appContext, invalidateKey = true)
                }
            } else {
                DeviceBootCredentials.clear(appContext)
                false
            }

            // Do not expose any CE/DE copy again until at least one complete new session is durable.
            if (refreshToken.isNotBlank() && (credentialSaved || bootSaved)) {
                devicePrefs.edit().remove(FIREBASE_SESSION_LOGGED_OUT).commit()
            }
        }
    }

    /** Persist a rotated pair only for a live session; a late refresh must not undo logout. */
    internal fun saveRefreshedTokens(apiKey: String, idToken: String, refreshToken: String) {
        synchronized(BOOT_PREFS_MIGRATION_LOCK) {
            if (firebaseSessionLoggedOut()) return
            saveFirebaseSession(apiKey, idToken, refreshToken)
        }
    }

    /** Logout tombstone suppresses both session copies before their storage is cleared. */
    fun clearFirebaseTokens() {
        synchronized(BOOT_PREFS_MIGRATION_LOCK) {
            devicePrefs.edit().putBoolean(FIREBASE_SESSION_LOGGED_OUT, true).commit()
            if (isUserUnlocked()) {
                credentialPrefs.edit().remove(K_ID_TOKEN).remove(K_REF_TOKEN).commit()
            }
            DeviceBootCredentials.clear(appContext, invalidateKey = true)
        }
    }

    /** Forget the saved password and the auto-login preference as one durable CE update. */
    internal fun clearRememberedCredentials() {
        if (!isUserUnlocked()) return
        credentialPrefs.edit()
            .remove(K_PASS)
            .putString(K_REMEMBER_CREDS, false.toString())
            .commit()
    }

    private fun writeCredentialSession(session: DeviceBootCredentials.Session) {
        credentialPrefs.edit()
            .putString(K_API_KEY, session.apiKey)
            .putString(K_ID_TOKEN, session.idToken)
            .putString(K_REF_TOKEN, session.refreshToken)
            .apply()
    }

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
        // If logout's secure delete could not complete, never let a stored password auto-login.
        if (key == K_PASS && firebaseSessionLoggedOut()) return null
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

    fun getInt(key: String, def: Int): Int = try { get(key)?.toIntOrNull() ?: def } catch (e: Exception) { def }
    fun setInt(key: String, v: Int) = set(key, v.toString())

    override fun getLong(key: String, def: Long): Long = try { get(key)?.toLongOrNull() ?: def } catch (e: Exception) { def }
    override fun setLong(key: String, v: Long) = set(key, v.toString())

    fun getBool(key: String, def: Boolean): Boolean = try { get(key)?.toBooleanStrictOrNull() ?: def } catch (e: Exception) { def }
    fun setBool(key: String, v: Boolean) = set(key, v.toString())

    companion object {
        private const val SECRET_PREFIX = "enc:v1:"
        private const val CREDENTIAL_KEY_ALIAS = "sterownikco.credentials.aes.v1"
        private const val GCM_IV_BYTES = 12
        private const val GCM_TAG_BITS = 128
        private const val CREDENTIAL_PREFS_NAME = "piec_hmi"
        private const val DEVICE_PREFS_NAME = "piec_hmi_direct_boot"
        private const val BOOT_PREFS_MIGRATED = "direct_boot_alarm_prefs_migrated_v1"
        private const val FIREBASE_SESSION_LOGGED_OUT = "direct_boot_firebase_session_logged_out_v1"
        private val BOOT_PREFS_MIGRATION_LOCK = Any()

        const val K_ALARM_OPEN_NONCE = "piec_alarm_open_nonce"
        private val DEVICE_PROTECTED_KEYS = setOf(
            AlarmCenter.K_MONITOR,
            AlarmCenter.K_SOUND,
            AlarmCenter.K_SOUND_DYM,
            AlarmCenter.K_SOUND_OGRZ,
            AlarmCenter.K_SOUND_BOTH,
            AlarmCenter.K_SNOOZE_UNTIL,
            AlarmCenter.K_MUTED_SIG,
            AlarmCenter.K_LAST_SIG,
            AlarmCenter.K_LAST_FIRE,
            AlarmCenter.K_CONTROLLER_DYM_MUTE_UNTIL,
            AlarmCenter.K_CONTROLLER_OGRZ_MUTE_UNTIL,
            AlarmCenter.K_CONTROLLER_STATUS_MARKER,
            K_ALARM_OPEN_NONCE
        )
        private val FIREBASE_SESSION_KEYS = setOf(K_API_KEY, K_ID_TOKEN, K_REF_TOKEN)

        // poświadczenia wbudowane w firmware (main_centrala.cpp) — jak w Piec.html
        const val DEFAULT_FB_API_KEY = "AIzaSyDcheuRNcNo4mzNpaTzn-19Ntw62djkfVU"
        const val DEFAULT_FB_EMAIL = "piec_co_boot@akwarium.local"
        const val DEFAULT_CMD_TOKEN = "sterownikco-cmd-2026"

        const val K_API_KEY = "piec_fb_api_key"
        const val K_EMAIL = "piec_fb_email"
        const val K_PASS = "piec_fb_password"
        const val K_REMEMBER_CREDS = "piec_fb_remember_credentials"
        const val K_CMD_TOKEN = "piec_cmd_token"
        const val K_DASH_WIDGETS = "piec_dashboard_widgets_v1"
        const val K_DASH_WIDGET_ORDER = "piec_dashboard_widget_order_v1"
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
