package com.sterownikco.pro.receiver

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.widget.Toast
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.service.AlarmMonitorService

/* ══════════════════════════════════════════════════════════════════════════
   Akcje z przycisków powiadomienia alarmowego + restart monitoringu po
   reboocie telefonu. Bez UI — tylko prefs + gaszenie powiadomienia.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmReceiver : BroadcastReceiver() {

    override fun onReceive(ctx: Context, intent: Intent?) {
        val p = try { Prefs(ctx.applicationContext) } catch (e: Exception) { return }
        when (intent?.action) {
            ACT_SNOOZE_10 -> {
                AlarmCenter.snooze(p, 10)
                AlarmNotify.cancel(ctx)
                toast(ctx, "Alarm wyciszony na 10 minut")
            }
            ACT_SNOOZE_60 -> {
                AlarmCenter.snooze(p, 60)
                AlarmNotify.cancel(ctx)
                toast(ctx, "Alarm wyciszony na 1 godzinę")
            }
            ACT_MUTE_NEXT -> {
                AlarmCenter.muteUntilNext(p)
                AlarmNotify.cancel(ctx)
                toast(ctx, "Wyciszono do następnego alarmu")
            }
            Intent.ACTION_BOOT_COMPLETED, Intent.ACTION_LOCKED_BOOT_COMPLETED -> {
                // Po reboocie wznawiamy czuwanie tylko gdy użytkownik jest
                // zalogowany (mamy refresh_token) i monitoringu nie wyłączył.
                if (AlarmCenter.isMonitored(p) && (p.get(Prefs.K_REF_TOKEN) ?: "").isNotEmpty()) {
                    AlarmMonitorService.start(ctx.applicationContext)
                }
            }
        }
    }

    private fun toast(ctx: Context, msg: String) {
        try { Toast.makeText(ctx.applicationContext, msg, Toast.LENGTH_LONG).show() } catch (e: Exception) { /* ignore */ }
    }

    companion object {
        const val ACT_SNOOZE_10 = "com.sterownikco.pro.ALARM_SNOOZE_10"
        const val ACT_SNOOZE_60 = "com.sterownikco.pro.ALARM_SNOOZE_60"
        const val ACT_MUTE_NEXT = "com.sterownikco.pro.ALARM_MUTE_NEXT"
    }
}
