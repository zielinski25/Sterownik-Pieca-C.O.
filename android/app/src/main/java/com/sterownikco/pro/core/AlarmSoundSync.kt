package com.sterownikco.pro.core

import org.json.JSONObject
import java.util.Locale

/** Command/status contract shared by the Android selector and the ESP32 firmware. */
internal object AlarmSoundSync {
    val SOUND_IDS: Set<String> = setOf(
        "syrena", "dzwonek", "pikanie", "dwutonowy", "impulsowy", "narastajacy", "niski_puls",
        "syrena_falujaca", "syrena_ratunkowa", "klakson_pulsacyjny", "gong_ostrzegawczy", "system"
    )

    data class Target(
        val commandTarget: String,
        val preferenceKind: String,
        val statusField: String
    )

    private data class PendingSelection(
        val soundId: String,
        val requestId: Long,
        val expiresAtElapsedMs: Long
    )

    const val STATUS_TIMEOUT_MS = 45_000L
    private val pendingLock = Any()
    private val pendingSelections = mutableMapOf<String, PendingSelection>()
    private var nextRequestId = 0L

    val TARGETS: List<Target> = listOf(
        Target(commandTarget = "dym", preferenceKind = "dym", statusField = "alarm_sound_dym"),
        Target(commandTarget = "ogrz", preferenceKind = "ogrz", statusField = "alarm_sound_ogrz"),
        Target(commandTarget = "oba", preferenceKind = "dym+ogrz", statusField = "alarm_sound_both")
    )

    private val targetByKind = mapOf(
        "dym" to TARGETS[0],
        "ogrz" to TARGETS[1],
        "oba" to TARGETS[2],
        "dym+ogrz" to TARGETS[2],
        "ogrz+dym" to TARGETS[2]
    )

    fun targetFor(kind: String): Target? =
        targetByKind[kind.trim().lowercase(Locale.ROOT)]

    fun normalizeSoundId(soundId: String): String? =
        soundId.trim().lowercase(Locale.ROOT).takeIf { it in SOUND_IDS }

    fun command(kind: String, soundId: String): String? {
        val target = targetFor(kind) ?: return null
        val sound = normalizeSoundId(soundId) ?: return null
        return "alarm_sound ${target.commandTarget} $sound"
    }

    fun rememberSelection(target: Target, soundId: String, nowElapsedMs: Long): Long? {
        val normalized = normalizeSoundId(soundId) ?: return null
        return synchronized(pendingLock) {
            nextRequestId += 1L
            pendingSelections[target.statusField] = PendingSelection(
                soundId = normalized,
                requestId = nextRequestId,
                expiresAtElapsedMs = nowElapsedMs + STATUS_TIMEOUT_MS
            )
            nextRequestId
        }
    }

    fun clearPendingSelection(target: Target, requestId: Long) {
        synchronized(pendingLock) {
            if (pendingSelections[target.statusField]?.requestId == requestId) {
                pendingSelections.remove(target.statusField)
            }
        }
    }

    private fun shouldApplyStatus(target: Target, soundId: String, nowElapsedMs: Long): Boolean =
        synchronized(pendingLock) {
            val pending = pendingSelections[target.statusField] ?: return@synchronized true
            when {
                soundId == pending.soundId -> {
                    if (pendingSelections[target.statusField]?.requestId == pending.requestId) {
                        pendingSelections.remove(target.statusField)
                    }
                    true
                }
                nowElapsedMs < pending.expiresAtElapsedMs -> false
                else -> {
                    if (pendingSelections[target.statusField]?.requestId == pending.requestId) {
                        pendingSelections.remove(target.statusField)
                    }
                    true
                }
            }
        }

    /** Applies only valid sound fields; older firmware leaves all of them absent. */
    fun applyStatus(prefs: AlarmPrefsStore, status: JSONObject, nowElapsedMs: Long) {
        val availableFields = mutableSetOf<String>()
        for (target in TARGETS) {
            val raw = if (status.has(target.statusField) && !status.isNull(target.statusField)) {
                status.opt(target.statusField) as? String
            } else null
            val sound = raw?.let(::normalizeSoundId)
            if (sound != null) {
                availableFields.add(target.statusField)
                if (shouldApplyStatus(target, sound, nowElapsedMs) &&
                    AlarmCenter.sound(prefs, target.preferenceKind) != sound) {
                    AlarmCenter.setSound(prefs, target.preferenceKind, sound)
                }
            }
        }
        synchronized(pendingLock) {
            pendingSelections.entries.toList().forEach { (field, pending) ->
                if (nowElapsedMs >= pending.expiresAtElapsedMs && field !in availableFields) {
                    pendingSelections.remove(field)
                }
            }
        }
    }
}
