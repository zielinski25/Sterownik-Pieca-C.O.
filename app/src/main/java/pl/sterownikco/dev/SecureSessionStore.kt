package pl.sterownikco.dev

import android.content.Context
import android.util.Base64
import java.nio.ByteBuffer
import java.security.KeyStore
import javax.crypto.Cipher
import javax.crypto.KeyGenerator
import javax.crypto.SecretKey
import javax.crypto.spec.GCMParameterSpec

class SecureSessionStore(context: Context) {
    data class SavedSession(
        val email: String,
        val refreshToken: String,
        val idToken: String? = null,
        val expiresAtMs: Long = 0L,
        val localId: String? = null,
    )

    private val prefs = context.getSharedPreferences("secure_session", Context.MODE_PRIVATE)
    private val keyAlias = "sterownik_co_dev_session_key"

    init {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        if (!ks.containsAlias(keyAlias)) {
            val generator = KeyGenerator.getInstance("AES", "AndroidKeyStore")
            generator.init(android.security.keystore.KeyGenParameterSpec.Builder(
                keyAlias,
                android.security.keystore.KeyProperties.PURPOSE_ENCRYPT or
                    android.security.keystore.KeyProperties.PURPOSE_DECRYPT
            ).setBlockModes(android.security.keystore.KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(android.security.keystore.KeyProperties.ENCRYPTION_PADDING_NONE)
                .build())
            generator.generateKey()
        }
    }

    /**
     * New format stores the refresh token plus the latest ID-token metadata.
     * Keeping the ID token encrypted lets the app remain signed-in while a
     * temporary network outage prevents an immediate refresh.
     * The loader remains backwards-compatible with the original 2-field blob.
     */
    fun save(
        email: String,
        refreshToken: String,
        idToken: String? = null,
        expiresAtMs: Long = 0L,
        localId: String? = null,
    ) {
        val fields = listOf(
            email,
            refreshToken,
            idToken.orEmpty(),
            expiresAtMs.toString(),
            localId.orEmpty(),
        )
        val payload = fields.joinToString("\u0000").toByteArray(Charsets.UTF_8)
        prefs.edit().putString("blob", encrypt(payload)).apply()
    }

    fun load(): SavedSession? {
        val blob = prefs.getString("blob", null) ?: return null
        val plain = runCatching { decrypt(blob) }.getOrNull() ?: return null
        val split = plain.toString(Charsets.UTF_8).split('\u0000')
        if (split.size < 2 || split[0].isBlank() || split[1].isBlank()) return null
        val idToken = split.getOrNull(2).orEmpty().takeIf { it.isNotBlank() }
        val expiresAtMs = split.getOrNull(3)?.toLongOrNull() ?: 0L
        val localId = split.getOrNull(4).orEmpty().takeIf { it.isNotBlank() }
        return SavedSession(split[0], split[1], idToken, expiresAtMs, localId)
    }

    fun clear() { prefs.edit().clear().apply() }

    private fun key(): SecretKey {
        val ks = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }
        return (ks.getEntry(keyAlias, null) as KeyStore.SecretKeyEntry).secretKey
    }

    private fun encrypt(bytes: ByteArray): String {
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.ENCRYPT_MODE, key())
        val iv = cipher.iv
        val cipherText = cipher.doFinal(bytes)
        val packed = ByteBuffer.allocate(4 + iv.size + cipherText.size)
        packed.putInt(iv.size).put(iv).put(cipherText)
        return Base64.encodeToString(packed.array(), Base64.NO_WRAP)
    }

    private fun decrypt(value: String): ByteArray {
        val packed = Base64.decode(value, Base64.NO_WRAP)
        val buffer = ByteBuffer.wrap(packed)
        val ivSize = buffer.int
        require(ivSize in 12..32)
        val iv = ByteArray(ivSize).also(buffer::get)
        val cipherText = ByteArray(buffer.remaining()).also(buffer::get)
        val cipher = Cipher.getInstance("AES/GCM/NoPadding")
        cipher.init(Cipher.DECRYPT_MODE, key(), GCMParameterSpec(128, iv))
        return cipher.doFinal(cipherText)
    }
}
