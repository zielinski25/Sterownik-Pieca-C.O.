package com.sterownikco.pro.ui.screens.sheets

import android.content.Context
import android.content.Intent
import android.media.MediaPlayer
import android.net.Uri
import android.os.PowerManager
import android.provider.Settings
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.service.AlarmMonitorService
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.Txt

/* ══════════════════════════════════════════════════════════════════════════
   Arkusz „Alarmy dymu i przegrzania" (APK-only, brak w Piec.html):
   monitoring w tle, wybór dźwięku + odsłuch, drzemka, uprawnienia
   (powiadomienia / pełny ekran / bateria) i podpowiedź o widgecie.
   ══════════════════════════════════════════════════════════════════════════ */

@Composable
fun AlarmSheet(m: AppModel) {
    m.alarmUiTick // licznik odświeżeń (uprawnienia zmieniają się poza aplikacją)
    val ctx = m.ctx
    val p = m.prefs
    val monitored = AlarmCenter.isMonitored(p)
    val sound = AlarmCenter.sound(p)

    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        // 1. Status
        AlarmCard {
            AlarmRow("●", AlarmCenter.statusText(p),
                if (monitored) Pal.Live else Pal.Err)
            AlarmRow(if (m.S.dym_alarm) "🚨" else "○",
                "Dym MQ-2: " + (if (m.S.dym_alarm) "ALARM" else "spokój") +
                    " · Piec: " + (if (m.S.alarm_ogrzewanie) "PRZEGRZANIE" else "OK"),
                if (m.S.dym_alarm || m.S.alarm_ogrzewanie) Pal.Err else Pal.TextDim)
        }

        // 2. Monitoring w tle
        AlarmCard {
            Text("CZUWANIE W TLE", style = Txt.tiny, color = Pal.Cyan)
            Text("Usługa czyta strumień Firebase także przy zminimalizowanej aplikacji. Wyłączenie = brak alarmów w tle.",
                fontSize = 11.sp, color = Pal.TextDim)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                AlarmBtn("Włączone", monitored, Modifier.weight(1f)) {
                    AlarmCenter.setMonitored(p, true)
                    try { AlarmMonitorService.start(ctx) } catch (e: Exception) { /* ignore */ }
                    m.alarmUiTick++
                }
                AlarmBtn("Wyłączone", !monitored, Modifier.weight(1f)) {
                    AlarmCenter.setMonitored(p, false)
                    try { AlarmMonitorService.stop(ctx) } catch (e: Exception) { /* ignore */ }
                    m.alarmUiTick++
                }
            }
        }

        // 3. Dźwięk alarmu
        AlarmCard {
            Text("DŹWIĘK ALARMU", style = Txt.tiny, color = Pal.Cyan)
            AlarmNotify.SOUNDS.forEach { (id, label) ->
                Row(Modifier.fillMaxWidth().clickable {
                    AlarmCenter.setSound(p, id)
                    playTest(ctx, id)
                    m.alarmUiTick++
                }.padding(vertical = 7.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(if (sound == id) "◉" else "○", fontSize = 15.sp,
                        color = if (sound == id) Pal.Cyan else Pal.TextDim)
                    Text("  $label", fontSize = 13.sp, color = Pal.Text, modifier = Modifier.weight(1f))
                    if (sound == id) Text("♪ gra", fontSize = 11.sp, color = Pal.Cyan)
                }
            }
            Text("Tapnięcie zapisuje wybór i od razu go odsłuchuje.",
                fontSize = 11.sp, color = Pal.TextDim)
        }

        // 4. Drzemka / wyciszenie
        AlarmCard {
            Text("WYCISZENIE", style = Txt.tiny, color = Pal.Cyan)
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                AlarmBtn("10 min", false, Modifier.weight(1f)) {
                    AlarmCenter.snooze(p, 10); AlarmNotify.cancel(ctx); m.alarmUiTick++
                }
                AlarmBtn("1 h", false, Modifier.weight(1f)) {
                    AlarmCenter.snooze(p, 60); AlarmNotify.cancel(ctx); m.alarmUiTick++
                }
            }
            AlarmBtn("🔕 Wycisz do następnego alarmu", false, Modifier.fillMaxWidth()) {
                AlarmCenter.muteUntilNext(p); AlarmNotify.cancel(ctx); m.alarmUiTick++
            }
            if (AlarmCenter.snoozeLeftMin(p) > 0 || AlarmCenter.isMutedNow(p)) {
                AlarmBtn("🔔 Zakończ wyciszenie", true, Modifier.fillMaxWidth()) {
                    AlarmCenter.unsilence(p); m.alarmUiTick++
                }
            }
        }

        // 5. Uprawnienia systemowe
        AlarmCard {
            Text("UPRAWNIENIA SYSTEMOWE", style = Txt.tiny, color = Pal.Cyan)
            val notifOk = AlarmNotify.hasPostNotifications(ctx)
            PermRow("Powiadomienia", notifOk, "Bez tego alarm nie zadzwoni.",
                btn = "Poproś o zgodę") { m.askNotifPerm = true }
            val fsiOk = AlarmNotify.canFullScreen(ctx)
            PermRow("Okienko na wierzchu", fsiOk,
                if (fsiOk) "Alarm wyskoczy nawet na blokadzie." else "Bez tego tylko pasek + dźwięk.",
                btn = if (fsiOk) null else "Otwórz ustawienia") {
                openSafely(ctx, AlarmNotify.fullScreenSettingsIntent(ctx)); m.alarmUiTick++
            }
            val batOk = remember(m.alarmUiTick) { isIgnoringBattery(ctx) }
            PermRow("Praca w tle (bateria)", batOk,
                if (batOk) "System nie ubije czuwania." else "realme/Oppo lubią ubijać usługi — wyłącz optymalizację.",
                btn = if (batOk) null else "Wyłącz optymalizację") {
                openBatterySettings(ctx); m.alarmUiTick++
            }
        }

        // 6. Widget + autostart
        AlarmCard {
            Text("WIDGET I AUTOSTART", style = Txt.tiny, color = Pal.Cyan)
            Text("• Widget: przytrzymaj pulpit → Widgety → Sterownik CO (4 temperatury + status).\n" +
                "• Po restarcie telefonu czuwanie wraca samo (gdy jesteś zalogowany).\n" +
                "• realme: Ustawienia → Aplikacje → Sterownik CO → Autostart + „Zezwól na działanie w tle”.",
                fontSize = 11.sp, color = Pal.TextDim, lineHeight = 16.sp)
        }
    }
}

@Composable
private fun AlarmCard(body: @Composable () -> Unit) {
    Column(Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(14.dp))
        .border(1.dp, Pal.Border, RoundedCornerShape(14.dp)).padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)) { body() }
}

@Composable
private fun AlarmRow(dot: String, text: String, color: Color) {
    Text("$dot  $text", fontSize = 12.5.sp, fontWeight = FontWeight.Bold, color = color)
}

@Composable
private fun AlarmBtn(label: String, on: Boolean, modifier: Modifier = Modifier, onClick: () -> Unit) {
    Box(modifier.height(44.dp)
        .background(if (on) Pal.rgba(0, 212, 245, .16f) else Pal.Surface2, RoundedCornerShape(10.dp))
        .border(1.dp, if (on) Pal.Cyan else Pal.BorderStrong, RoundedCornerShape(10.dp))
        .clickable(onClick = onClick).padding(horizontal = 10.dp),
        contentAlignment = Alignment.Center) {
        Text(label, fontSize = 12.5.sp, fontWeight = FontWeight.Bold,
            color = if (on) Pal.Cyan else Pal.Text)
    }
}

@Composable
private fun PermRow(title: String, ok: Boolean, desc: String, btn: String?, onBtn: () -> Unit) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(4.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text(if (ok) "✅" else "⚠️", fontSize = 13.sp)
            Text("  $title", fontSize = 12.5.sp, fontWeight = FontWeight.Bold, color = Pal.Text,
                modifier = Modifier.weight(1f))
            if (btn != null) {
                Box(Modifier.background(Pal.Surface2, RoundedCornerShape(8.dp))
                    .border(1.dp, Pal.Cyan, RoundedCornerShape(8.dp))
                    .clickable(onClick = onBtn).padding(horizontal = 10.dp, vertical = 6.dp)) {
                    Text(btn, fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Pal.Cyan)
                }
            }
        }
        Text(desc, fontSize = 11.sp, color = Pal.TextDim)
    }
}

private fun playTest(ctx: Context, sound: String) {
    try {
        val res = AlarmNotify.soundRes(sound)
        val mp = if (res != 0) MediaPlayer.create(ctx, res)
        else MediaPlayer.create(ctx, Settings.System.DEFAULT_NOTIFICATION_URI)
        mp?.setOnCompletionListener { try { it.release() } catch (e: Exception) { /* ignore */ } }
        mp?.start()
    } catch (e: Exception) { /* brak dźwięku — nie blokujemy UI */ }
}

private fun isIgnoringBattery(ctx: Context): Boolean = try {
    (ctx.getSystemService(Context.POWER_SERVICE) as PowerManager)
        .isIgnoringBatteryOptimizations(ctx.packageName)
} catch (e: Exception) { false }

private fun openSafely(ctx: Context, i: Intent) {
    try { ctx.startActivity(i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK)) } catch (e: Exception) { /* ignore */ }
}

private fun openBatterySettings(ctx: Context) {
    try {
        ctx.startActivity(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS,
            Uri.parse("package:" + ctx.packageName)).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
    } catch (e: Exception) {
        openSafely(ctx, Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
    }
}
