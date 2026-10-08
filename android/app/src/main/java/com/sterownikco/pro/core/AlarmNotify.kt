package com.sterownikco.pro.core

import android.Manifest
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.net.Uri
import android.os.Build
import android.provider.Settings
import androidx.core.app.ActivityCompat
import androidx.core.app.NotificationCompat
import com.sterownikco.pro.AlarmActivity
import com.sterownikco.pro.MainActivity
import com.sterownikco.pro.R
import com.sterownikco.pro.receiver.AlarmReceiver

/* ══════════════════════════════════════════════════════════════════════════
   Powiadomienia alarmowe (APK-only).

   • Dźwięk kanału jest NIEMUTOWALNY po utworzeniu (reguła Androida 8+) —
     dlatego każdy dźwięk do wyboru ma WŁASNY kanał; wybór = zmiana kanału.
   • Okienko na wierzchu (także na zablokowanym ekranie) = full-screen intent.
     Android 14+ wymaga USE_FULL_SCREEN_INTENT + zgody użytkownika
     (`canUseFullScreenIntent()`); bez zgody degradujemy do heads-up + akcji.
   ══════════════════════════════════════════════════════════════════════════ */

object AlarmNotify {
    const val NOTIF_ID = 4201
    const val SVC_NOTIF_ID = 4202
    const val CH_SVC = "piec_svc"

    /** id dźwięku → podpis. Kolejność = kolejność w selekcie. */
    val SOUNDS = listOf(
        "syrena" to "Syrena",
        "dzwonek" to "Dzwonek",
        "pikanie" to "Pikanie",
        "dwutonowy" to "Dwutonowy",
        "impulsowy" to "Krótkie impulsy",
        "narastajacy" to "Narastający",
        "niski_puls" to "Niski puls",
        "system" to "Domyślny systemowy"
    )

    fun channelFor(sound: String): String = "piec_alarm_$sound"

    fun soundRes(sound: String): Int = when (sound) {
        "syrena" -> R.raw.alarm_syrena
        "dzwonek" -> R.raw.alarm_dzwonek
        "pikanie" -> R.raw.alarm_pikanie
        "dwutonowy" -> R.raw.alarm_dwutonowy
        "impulsowy" -> R.raw.alarm_impulsowy
        "narastajacy" -> R.raw.alarm_narastajacy
        "niski_puls" -> R.raw.alarm_niski_puls
        else -> 0 // "system" — kanał bez setSound = domyślny dźwięk powiadomień
    }

    private fun nm(ctx: Context): NotificationManager =
        ctx.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager

    fun hasPostNotifications(ctx: Context): Boolean =
        Build.VERSION.SDK_INT < 33 ||
            ActivityCompat.checkSelfPermission(ctx, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED

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
                description = "Ciche powiadomienie usługi czuwającej nad alarmami dymu i przegrzania."
            }
        )
        val attrs = AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_ALARM)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build()
        for ((id, label) in SOUNDS) {
            val ch = NotificationChannel(channelFor(id), "Alarm kotła — $label", NotificationManager.IMPORTANCE_HIGH)
            ch.description = "Dźwięk alarmu dymu / przegrzania: $label."
            ch.enableVibration(true)
            ch.vibrationPattern = longArrayOf(0, 450, 200, 450, 200, 800)
            ch.setShowBadge(true)
            ch.lockscreenVisibility = android.app.Notification.VISIBILITY_PUBLIC
            val res = soundRes(id)
            if (res != 0) ch.setSound(Uri.parse("android.resource://${ctx.packageName}/$res"), attrs)
            m.createNotificationChannel(ch)
        }
    }

    private fun actionPending(ctx: Context, action: String, req: Int): PendingIntent {
        val i = Intent(ctx, AlarmReceiver::class.java).setAction(action)
        return PendingIntent.getBroadcast(ctx, req, i, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE)
    }

    /** Odpalenie alarmu: dźwięk + wibracja + okienko (FSI) lub heads-up + akcje drzemki. */
    fun fire(ctx: Context, info: AlarmInfo) {
        ensureChannels(ctx)
        val sound = AlarmCenter.sound(Prefs(ctx), info.kind)
        val full = PendingIntent.getActivity(
            ctx, 11,
            Intent(ctx, AlarmActivity::class.java)
                .putExtra("kind", info.kind).putExtra("title", info.title)
                .putExtra("msg", info.msg).putExtra("sig", info.sig)
                .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val open = PendingIntent.getActivity(
            ctx, 12,
            Intent(ctx, MainActivity::class.java).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val b = NotificationCompat.Builder(ctx, channelFor(sound))
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle("🚨 ${info.title}")
            .setContentText(info.msg)
            .setStyle(NotificationCompat.BigTextStyle().bigText(info.msg))
            .setPriority(NotificationCompat.PRIORITY_MAX)
            .setCategory(NotificationCompat.CATEGORY_ALARM)
            .setVisibility(NotificationCompat.VISIBILITY_PUBLIC)
            .setAutoCancel(false)
            .setOngoing(true)
            .setOnlyAlertOnce(false)
            .setContentIntent(open)
            .addAction(0, "Drzemka 10 min", actionPending(ctx, AlarmReceiver.ACT_SNOOZE_10, 21))
            .addAction(0, "Drzemka 1 h", actionPending(ctx, AlarmReceiver.ACT_SNOOZE_60, 22))
            .addAction(0, "Wycisz do następnego", actionPending(ctx, AlarmReceiver.ACT_MUTE_NEXT, 23))
        if (canFullScreen(ctx)) b.setFullScreenIntent(full, true)
        try {
            nm(ctx).notify(NOTIF_ID, b.build())
        } catch (e: SecurityException) {
            // Brak POST_NOTIFICATIONS — użytkownik musi je włączyć w arkuszu Alarmy.
        }
    }

    fun cancel(ctx: Context) {
        try { nm(ctx).cancel(NOTIF_ID) } catch (e: Exception) { /* ignore */ }
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
            .setContentTitle("Sterownik CO — monitoring alarmów")
            .setContentText("Czuwam nad dymem i przegrzaniem kotła")
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setOngoing(true)
            .setContentIntent(open)
            .build()
    }
}
