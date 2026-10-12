package com.sterownikco.pro.core

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import com.sterownikco.pro.AlarmActivity
import com.sterownikco.pro.MainActivity
import com.sterownikco.pro.R
import com.sterownikco.pro.receiver.AlarmReceiver
import com.sterownikco.pro.service.AlarmMonitorService
import java.util.UUID

/* ══════════════════════════════════════════════════════════════════════════
   Powiadomienia alarmowe (APK-only).

   • Kanały powiadomień są ciche i wersjonowane; alarmowy dźwięk wybrany w UI
     odtwarza w pętli AlarmMonitorService aż do skasowania alarmu / drzemki.
   • Okienko na wierzchu (także na zablokowanym ekranie) = full-screen intent.
     Android 14+ wymaga USE_FULL_SCREEN_INTENT + zgody użytkownika
     (`canUseFullScreenIntent()`); bez zgody degradujemy do heads-up + akcji.
   ══════════════════════════════════════════════════════════════════════════ */

object AlarmNotify {
    private const val TAG = "AlarmNotify"
    const val NOTIF_ID = 4201
    const val SVC_NOTIF_ID = 4202
    const val CH_SVC = "piec_svc"
    const val ACTION_ALARM_UPDATED = "com.sterownikco.pro.ALARM_UPDATED"
    const val ACTION_ALARM_CLEARED = "com.sterownikco.pro.ALARM_CLEARED"
    const val EXTRA_ALARM_TITLE = "alarm_title"
    const val EXTRA_ALARM_MESSAGE = "alarm_message"
    const val EXTRA_ALARM_KIND = "kind"
    const val EXTRA_ALARM_SIGNATURE = "sig"
    const val EXTRA_ALARM_OPEN_NONCE = "alarm_open_nonce"
    private const val K_ALARM_OPEN_NONCE = Prefs.K_ALARM_OPEN_NONCE

    /** id dźwięku → podpis. Kolejność = kolejność w selekcie. */
    val SOUNDS = listOf(
        "syrena" to "Syrena",
        "dzwonek" to "Dzwonek",
        "pikanie" to "Pikanie",
        "dwutonowy" to "Dwutonowy",
        "impulsowy" to "Krótkie impulsy",
        "narastajacy" to "Narastający",
        "niski_puls" to "Niski puls",
        "syrena_falujaca" to "Syrena falująca",
        "syrena_ratunkowa" to "Syrena ratunkowa",
        "klakson_pulsacyjny" to "Klakson pulsacyjny",
        "gong_ostrzegawczy" to "Gong ostrzegawczy",
        "system" to "Domyślny systemowy"
    )

    // Nowy, cichy kanał omija jednokrotne odtwarzanie starego kanału; dźwięk zapętla usługa.
    fun channelFor(sound: String): String = "piec_alarm_loop_v1_$sound"

    fun soundRes(sound: String): Int = when (sound) {
        "syrena" -> R.raw.alarm_syrena
        "dzwonek" -> R.raw.alarm_dzwonek
        "pikanie" -> R.raw.alarm_pikanie
        "dwutonowy" -> R.raw.alarm_dwutonowy
        "impulsowy" -> R.raw.alarm_impulsowy
        "narastajacy" -> R.raw.alarm_narastajacy
        "niski_puls" -> R.raw.alarm_niski_puls
        "syrena_falujaca" -> R.raw.alarm_syrena_falujaca
        "syrena_ratunkowa" -> R.raw.alarm_syrena_ratunkowa
        "klakson_pulsacyjny" -> R.raw.alarm_klakson_pulsacyjny
        "gong_ostrzegawczy" -> R.raw.alarm_gong_ostrzegawczy
        else -> 0 // "system" — AlarmMonitorService używa domyślnego dźwięku systemowego
    }

    private fun nm(ctx: Context): NotificationManager =
        ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    fun hasPostNotifications(ctx: Context): Boolean =
        Build.VERSION.SDK_INT < 33 ||
            ActivityCompat.checkSelfPermission(ctx, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED

    /** App-generated alarm intents are trusted only while their app-private nonce is current. */
    fun hasCurrentAlarmNonce(ctx: Context, intent: Intent): Boolean {
        val supplied = intent.getStringExtra(EXTRA_ALARM_OPEN_NONCE) ?: return false
        return AlarmIntentNonce.matches(supplied, Prefs(ctx).get(K_ALARM_OPEN_NONCE))
    }

    /** Android 14+: czy wolno nam odpalać full-screen intent? */
    fun canFullScreen(ctx: Context): Boolean =
        Build.VERSION.SDK_INT < 34 || nm(ctx).canUseFullScreenIntent()

    /** Ustawienia systemowe „Pełny ekran" dla naszej aplikacji (Android 14+). */
    fun fullScreenSettingsIntent(ctx: Context): Intent =
        if (Build.VERSION.SDK_INT >= 34)
            Intent(Settings.ACTION_MANAGE_APP_USE_FULL_SCREEN_INTENT).apply {
                data = Uri.parse("package:" + ctx.packageName)
            }
        else Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS).apply {
            putExtra(Settings.EXTRA_APP_PACKAGE, ctx.packageName)
        }

    fun ensureChannels(ctx: Context) {
        if (Build.VERSION.SDK_INT < 26) return
        val m = nm(ctx)
        m.createNotificationChannel(
            NotificationChannel(CH_SVC, "Monitoring kotła w tle", NotificationManager.IMPORTANCE_LOW).apply {
                description = "Ciche powiadomienie usługi alarmowej; nie potwierdza łączności ani aktualności danych RTDB."
            }
        )
        for ((id, label) in SOUNDS) {
            val ch = NotificationChannel(channelFor(id), "Alarm kotła — $label", NotificationManager.IMPORTANCE_HIGH)
            ch.description = "Alarm dymu / przegrzania: $label, odtwarzany w pętli do skasowania alarmu."
            ch.enableVibration(true)
            ch.vibrationPattern = longArrayOf(0, 450, 200, 450, 200, 800)
            ch.setShowBadge(true)
            ch.lockscreenVisibility = android.app.Notification.VISIBILITY_PUBLIC
            // Dźwięk kanału Androida jest jednorazowy; właściwą pętlę odtwarza MediaPlayer usługi.
            ch.setSound(null, null)
            m.createNotificationChannel(ch)
        }
    }

    private fun actionPending(ctx: Context, action: String, req: Int, signature: String): PendingIntent {
        val nonce = Prefs(ctx).get(K_ALARM_OPEN_NONCE).orEmpty()
        val i = Intent(ctx, AlarmReceiver::class.java).setAction(action)
            .setData(Uri.parse("sterownikco://alarm-action/$req/$nonce"))
            .putExtra(EXTRA_ALARM_OPEN_NONCE, nonce)
            .putExtra(EXTRA_ALARM_SIGNATURE, signature)
        return PendingIntent.getBroadcast(ctx, req, i, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    private val missingNotificationLock = Any()

    private fun hasActiveAlarmNotification(ctx: Context): Boolean = try {
        nm(ctx).activeNotifications.any { it.id == NOTIF_ID }
    } catch (e: Exception) {
        false
    }

    /** Cancels a pre-nonce notification whose old action PendingIntents must not be trusted. */
    fun cancelLegacyNotification(ctx: Context) = synchronized(missingNotificationLock) {
        if (!Prefs(ctx).get(K_ALARM_OPEN_NONCE).isNullOrBlank()) return@synchronized
        if (hasActiveAlarmNotification(ctx)) cancel(ctx)
    }

    /** Restores a missing notification, or replaces a legacy copy without a current action nonce. */
    fun postIfMissing(ctx: Context, info: AlarmInfo): Boolean = synchronized(missingNotificationLock) {
        val hasCurrentNonce = !Prefs(ctx).get(K_ALARM_OPEN_NONCE).isNullOrBlank()
        val isActive = hasActiveAlarmNotification(ctx)
        if (isActive && hasCurrentNonce) return@synchronized false
        if (isActive) cancel(ctx)
        if (!hasPostNotifications(ctx)) return@synchronized false
        fire(ctx, info)
        true
    }

    /** Odpalenie alarmu: dźwięk + wibracja + okienko (FSI) lub heads-up + akcje drzemki. */
    fun fire(ctx: Context, info: AlarmInfo) {
        ensureChannels(ctx)
        val prefs = Prefs(ctx)
        val sound = AlarmCenter.sound(prefs, info.kind)
        val openNonce = UUID.randomUUID().toString()
        prefs.set(K_ALARM_OPEN_NONCE, openNonce)
        // Kanał jest cichy; usługa utrzymuje wybraną melodię w pętli, dopóki alarm trwa.
        AlarmMonitorService.startAlarmLoop(ctx, sound)
        val full = PendingIntent.getActivity(
            ctx, 11,
            Intent(ctx, AlarmActivity::class.java)
                .putExtra(EXTRA_ALARM_KIND, info.kind).putExtra(EXTRA_ALARM_TITLE, info.title)
                .putExtra(EXTRA_ALARM_MESSAGE, info.msg).putExtra(EXTRA_ALARM_SIGNATURE, info.sig)
                .putExtra(EXTRA_ALARM_OPEN_NONCE, openNonce)
                .setData(Uri.parse("sterownikco://alarm-fullscreen/$openNonce"))
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val open = PendingIntent.getActivity(
            ctx, 13,
            Intent(ctx, MainActivity::class.java)
                .putExtra(EXTRA_ALARM_KIND, info.kind)
                .putExtra(EXTRA_ALARM_TITLE, info.title)
                .putExtra(EXTRA_ALARM_MESSAGE, info.msg)
                .putExtra(EXTRA_ALARM_SIGNATURE, info.sig)
                .putExtra(EXTRA_ALARM_OPEN_NONCE, openNonce)
                .setData(Uri.parse("sterownikco://alarm-panel/$openNonce"))
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val visibleTitle = "🚨 ${info.title}"
        val b = NotificationCompat.Builder(ctx, channelFor(sound))
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle(visibleTitle)
            .setContentText(info.msg)
            .setStyle(NotificationCompat.BigTextStyle().bigText(info.msg))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setCategory(NotificationCompat.CATEGORY_ALARM)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setAutoCancel(false)
            .setOngoing(true)
            .setOnlyAlertOnce(false)
            .setContentIntent(open)
            .addAction(0, "Drzemka 10 min", actionPending(ctx, AlarmReceiver.ACT_SNOOZE_10, 21, info.sig))
            .addAction(0, "Drzemka 1 h", actionPending(ctx, AlarmReceiver.ACT_SNOOZE_60, 22, info.sig))
            .addAction(0, "Wycisz do następnego", actionPending(ctx, AlarmReceiver.ACT_MUTE_NEXT, 23, info.sig))
        if (canFullScreen(ctx)) b.setFullScreenIntent(full, true)
        try {
            nm(ctx).notify(NOTIF_ID, b.build())
        } catch (e: SecurityException) {
            Log.e(TAG, "Brak uprawnienia POST_NOTIFICATIONS; powiadomienie alarmowe nie zostało opublikowane", e)
        }
        publishActivityState(ctx, ACTION_ALARM_UPDATED, info, openNonce)
    }

    fun cancel(ctx: Context) {
        try { nm(ctx).cancel(NOTIF_ID) } catch (e: Exception) { /* ignore */ }
        AlarmMonitorService.stopAlarmLoop()
        try { Prefs(ctx).remove(K_ALARM_OPEN_NONCE) } catch (e: Exception) { /* ignore */ }
        publishActivityState(ctx, ACTION_ALARM_CLEARED)
    }

    private fun publishActivityState(
        ctx: Context,
        action: String,
        info: AlarmInfo? = null,
        openNonce: String? = null
    ) {
        try {
            val intent = Intent(action).setPackage(ctx.packageName)
            if (info != null) {
                intent.putExtra(EXTRA_ALARM_KIND, info.kind)
                intent.putExtra(EXTRA_ALARM_TITLE, info.title)
                intent.putExtra(EXTRA_ALARM_MESSAGE, info.msg)
                intent.putExtra(EXTRA_ALARM_SIGNATURE, info.sig)
            }
            openNonce?.let { intent.putExtra(EXTRA_ALARM_OPEN_NONCE, it) }
            ctx.sendBroadcast(intent)
        } catch (e: Exception) { /* ekran alarmu może nie być uruchomiony */ }
    }

    /** Ciche, stałe powiadomienie usługi monitorującej (kanał LOW = bez dźwięku). */
    fun serviceNotification(ctx: Context): android.app.Notification {
        ensureChannels(ctx)
        val open = PendingIntent.getActivity(
            ctx, 12,
            Intent(ctx, MainActivity::class.java).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(ctx, CH_SVC)
            .setSmallIcon(android.R.drawable.stat_notify_sync)
            .setContentTitle("${ctx.getString(R.string.app_name)} — usługa alarmów")
            .setContentText("Usługa alarmów działa w tle.")
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setOngoing(true)
            .setContentIntent(open)
            .build()
    }
}
