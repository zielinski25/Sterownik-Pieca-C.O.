package com.sterownikco.pro.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.widget.Toast
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmMuteSync
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.service.AlarmMonitorService

/* ══════════════════════════════════════════════════════════════════════════
   Akcje z przycisków powiadomienia alarmowego + restart monitoringu po
   reboocie telefonu. Bez UI — tylko prefs + gaszenie powiadomienia.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmReceiver : BroadcastReceiver() {

    override fun onReceive(ctx: Context, intent: Intent?) {
        val received = intent ?: return
        val action = received.action
        val p = try { Prefs(ctx.applicationContext) } catch (e: Exception) { return }
        val isAlarmAction = action == ACT_SNOOZE_10 || action == ACT_SNOOZE_60 || action == ACT_MUTE_NEXT
        if (isAlarmAction && !AlarmNotify.hasCurrentAlarmNonce(ctx.applicationContext, received)) return
        when (action) {
            ACT_SNOOZE_10 -> {
                AlarmCenter.snooze(p, 10)
                AlarmMuteSync.request(
                    ctx, alarmSignature(received, p), seconds = 10 * 60
                )
                AlarmNotify.cancel(ctx)
                toast(ctx, "Alarm wyciszony na 10 minut; synchronizuję z centralą")
            }
            ACT_SNOOZE_60 -> {
                AlarmCenter.snooze(p, 60)
                AlarmMuteSync.request(
                    ctx, alarmSignature(received, p), seconds = 60 * 60
                )
                AlarmNotify.cancel(ctx)
                toast(ctx, "Alarm wyciszony na 1 godzinę; synchronizuję z centralą")
            }
            ACT_MUTE_NEXT -> {
                AlarmCenter.muteUntilNext(p)
                AlarmMuteSync.request(ctx, alarmSignature(received, p), seconds = 0)
                AlarmNotify.cancel(ctx)
                toast(ctx, "Wyciszono do następnego alarmu; synchronizuję z centralą")
            }
            Intent.ACTION_BOOT_COMPLETED, Intent.ACTION_LOCKED_BOOT_COMPLETED -> {
                // Po reboocie wznawiamy czuwanie tylko gdy użytkownik jest
                // zalogowany (mamy refresh_token) i monitoringu nie wyłączył.
                // AlarmMonitorService.start() performs the authoritative session check.
                // Avoid decrypting the Direct Boot session twice on this receiver thread.
                if (AlarmCenter.isMonitored(p)) {
                    AlarmMonitorService.start(ctx.applicationContext)
                }
            }
        }
    }

    private fun alarmSignature(intent: Intent, prefs: Prefs): String =
        intent.getStringExtra(AlarmNotify.EXTRA_ALARM_SIGNATURE)
            ?.takeIf { it.isNotBlank() }
            ?: prefs.get(AlarmCenter.K_LAST_SIG).orEmpty()

    private fun toast(ctx: Context, msg: String) {
        try { Toast.makeText(ctx.applicationContext, msg, Toast.LENGTH_LONG).show() } catch (e: Exception) { /* ignore */ }
    }

    companion object {
        const val ACT_SNOOZE_10 = "com.sterownikco.pro.ALARM_SNOOZE_10"
        const val ACT_SNOOZE_60 = "com.sterownikco.pro.ALARM_SNOOZE_60"
        const val ACT_MUTE_NEXT = "com.sterownikco.pro.ALARM_MUTE_NEXT"
    }
}
