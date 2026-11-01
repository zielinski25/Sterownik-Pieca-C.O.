package com.sterownikco.pro.ui.screens.sheets

import android.content.Context
import android.content.Intent
import android.media.MediaPlayer
import android.net.Uri
import android.os.PowerManager
import android.os.SystemClock
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
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmMuteCommands
import com.sterownikco.pro.core.AlarmMuteSync
import com.sterownikco.pro.core.AlarmMonitorHealthText
import com.sterownikco.pro.core.AlarmMonitorPhase
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AlarmSoundSync
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.service.AlarmMonitorService
import com.sterownikco.pro.ui.components.Seg
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
    m.sheetTick // odświeżaj wiek ostatniego odczytu RTDB
    val ctx = m.ctx
    val p = m.prefs
    val alarmSignature = AlarmMuteCommands.signature(
        m.S.dym_alarm.takeIf { m.S.hasData("dym_alarm") },
        m.S.alarm_ogrzewanie.takeIf { m.S.hasData("alarm_ogrzewanie") },
        p.get(AlarmCenter.K_LAST_SIG).orEmpty()
    )
    val controllerMutedSignature = listOfNotNull(
        if (m.S.hasData("alarm_dym_wyciszony") && m.S.alarm_dym_wyciszony) "D" else null,
        if (m.S.hasData("alarm_ogrz_wyciszony") && m.S.alarm_ogrz_wyciszony) "O" else null
    ).joinToString("+")
    val monitored = AlarmCenter.isMonitored(p)
    val monitorHealth by AlarmMonitorService.monitorHealth.collectAsState()
    val nowElapsedMs = SystemClock.elapsedRealtime()
    var soundKind by remember { mutableStateOf("dym") }
    val sound = AlarmCenter.sound(p, soundKind)
    val soundTarget = AlarmSoundSync.targetFor(soundKind)
    val controllerSound = when (soundTarget?.statusField) {
        "alarm_sound_dym" -> m.S.alarm_sound_dym
        "alarm_sound_ogrz" -> m.S.alarm_sound_ogrz
        "alarm_sound_both" -> m.S.alarm_sound_both
        else -> ""
    }
    val controllerSoundReported = soundTarget?.let { m.S.hasData(it.statusField) } == true

    Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
        // 1. Status — brak flag nie jest równoznaczny ze stanem bez alarmu.
        AlarmCard {
            AlarmRow("●", AlarmCenter.statusText(p),
                if (monitored) Pal.Live else Pal.Err)
            val smokeKnown = m.S.hasData("dym_alarm")
            val boilerKnown = m.S.hasData("alarm_ogrzewanie")
            val active = (smokeKnown && m.S.dym_alarm) || (boilerKnown && m.S.alarm_ogrzewanie)
            val smoke = when {
                !smokeKnown -> "brak danych"
                m.S.dym_alarm -> "ALARM"
                else -> "spokój"
            }
            val boiler = when {
                !boilerKnown -> "brak danych"
                m.S.alarm_ogrzewanie -> "PRZEGRZANIE"
                else -> "OK"
            }
            AlarmRow(if (active) "🚨" else if (smokeKnown && boilerKnown) "○" else "⚠",
                "Dym MQ-2: $smoke · Piec: $boiler",
                if (active) Pal.Err else if (smokeKnown && boilerKnown) Pal.TextDim else Pal.Warn)
        }

        // 2. Monitoring w tle
        AlarmCard {
            Text("CZUWANIE W TLE", style = Txt.tiny, color = Pal.Cyan)
            val hasRefreshToken = !p.get(Prefs.K_REF_TOKEN).isNullOrBlank()
            val healthAge = monitorHealth.lastStatusElapsedMs?.let {
                (nowElapsedMs - it).coerceAtLeast(0L)
            }
            val monitorHint = AlarmMonitorHealthText.describe(
                monitored = monitored,
                hasSession = hasRefreshToken,
                health = monitorHealth,
                nowElapsedMs = nowElapsedMs
            )
            val healthColor = when {
                !monitored -> Pal.TextDim
                !hasRefreshToken -> Pal.Warn
                monitorHealth.phase == AlarmMonitorPhase.STATUS_OK && healthAge != null && healthAge < 60_000L -> Pal.Live
                monitorHealth.phase == AlarmMonitorPhase.STARTING || monitorHealth.phase == AlarmMonitorPhase.CONNECTING -> Pal.Warn
                monitorHealth.phase == AlarmMonitorPhase.RETRYING && healthAge != null && healthAge < 60_000L -> Pal.Warn
                else -> Pal.Err
            }
            Text(monitorHint, fontSize = 11.sp, color = healthColor)
            val directBootReady = p.directBootReady()
            val directBootHint = when {
                !monitored -> "Po restarcie: czuwanie przed odblokowaniem jest wyłączone."
                !hasRefreshToken -> "Przed odblokowaniem: brak zapisanej sesji Firebase."
                !p.directBootSupported() -> "Czuwanie przed odblokowaniem wymaga Androida 9 (API 28) lub nowszego."
                directBootReady -> "Zaszyfrowana sesja umożliwia wznowienie czuwania przed pierwszym odblokowaniem."
                else -> "Brak dostępnej zaszyfrowanej sesji Direct Boot; sprawdź logowanie i otwórz ponownie ten arkusz."
            }
            Text(
                directBootHint,
                fontSize = 11.sp,
                color = when {
                    !monitored -> Pal.TextDim
                    directBootReady -> Pal.Live
                    else -> Pal.Warn
                }
            )
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                AlarmBtn("Włączone", monitored, Modifier.weight(1f)) {
                    AlarmCenter.setMonitored(p, true)
                    if (!p.get(Prefs.K_REF_TOKEN).isNullOrBlank() && !AlarmNotify.hasPostNotifications(ctx)) {
                        m.askNotifPerm = true
                    }
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

        // 3. Osobny dźwięk dla każdego rodzaju alarmu
        AlarmCard {
            Text("DŹWIĘK WG RODZAJU ALARMU", style = Txt.tiny, color = Pal.Cyan)
            Seg(
                listOf("dym" to "Dym", "ogrz" to "Piec", "dym+ogrz" to "Oba"),
                current = soundKind,
                onPick = { soundKind = it }
            )
            Text(
                when (soundKind) {
                    "dym" -> "Alarm czujnika dymu MQ-2"
                    "ogrz" -> "Alarm przegrzania pieca"
                    else -> "Jednoczesny alarm dymu i przegrzania"
                },
                fontSize = 11.sp, fontWeight = FontWeight.Bold, color = Pal.Text
            )
            AlarmNotify.SOUNDS.forEach { (id, label) ->
                val selected = sound == id
                Row(Modifier.fillMaxWidth().clickable {
                    m.selectAlarmSound(soundKind, id)
                    playTest(ctx, id)
                }.padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                    Text(if (selected) "◉" else "○", fontSize = 15.sp,
                        color = if (selected) Pal.Cyan else Pal.TextDim)
                    Text("  $label", fontSize = 12.5.sp, color = Pal.Text, modifier = Modifier.weight(1f))
                    if (selected) Text("♪", fontSize = 11.sp, color = Pal.Cyan)
                }
            }
            Text("Każdy rodzaj alarmu zapamiętuje własny wybór. Dotknięcie zapisuje go w telefonie, wysyła do centrali i odtwarza próbkę.",
                fontSize = 11.sp, color = Pal.TextDim)
            Text(
                if (controllerSoundReported) {
                    val label = AlarmNotify.SOUNDS.firstOrNull { it.first == controllerSound }?.second ?: controllerSound
                    "Ustawienie centrali: $label"
                } else {
                    "Centrala nie raportuje jeszcze wyboru dźwięku — synchronizacja wymaga firmware v3.50.0 lub nowszego."
                },
                fontSize = 11.sp, color = if (controllerSoundReported) Pal.Live else Pal.Warn
            )
            Text("W centrali bieżący brzęczyk odtwarza odpowiadający wzór tonów (nie plik WAV); zmiana działa także przy aktywnym alarmie.",
                fontSize = 11.sp, color = Pal.TextDim)
            Text(
                "Podczas aktywnego alarmu dźwięk jest zapętlony; zatrzyma go skasowanie alarmu, drzemka lub wyciszenie.",
                fontSize = 11.sp, color = Pal.TextDim
            )
        }

        // 4. Drzemka / wyciszenie
        AlarmCard {
            Text("WYCISZENIE", style = Txt.tiny, color = Pal.Cyan)
            val muteStatusKnown = m.S.hasAllData(
                "alarm_dym_wyciszony", "alarm_dym_wyciszenie_ms",
                "alarm_ogrz_wyciszony", "alarm_ogrz_wyciszenie_ms"
            )
            val mutedClasses = buildList {
                if (m.S.hasData("alarm_dym_wyciszony") && m.S.alarm_dym_wyciszony) add("dym")
                if (m.S.hasData("alarm_ogrz_wyciszony") && m.S.alarm_ogrz_wyciszony) add("przegrzanie")
            }
            val sharedMuteText = when {
                !muteStatusKnown -> "Centrala nie raportuje wspólnego wyciszenia. Wymagany firmware z synchronizacją alarmów."
                mutedClasses.isEmpty() -> "Centrala potwierdza: brak wspólnego wyciszenia."
                else -> {
                    fun remainingLabel(ms: Long): String = when {
                        ms < 0L -> "do ustąpienia alarmu"
                        ms == 0L -> "wznowienie po upływie timera"
                        else -> "około ${((ms + 59_999L) / 60_000L).coerceAtLeast(1L)} min"
                    }
                    val details = buildList {
                        if (m.S.alarm_dym_wyciszony) {
                            add("dym ${remainingLabel(m.S.alarm_dym_wyciszenie_ms)}")
                        }
                        if (m.S.alarm_ogrz_wyciszony) {
                            add("przegrzanie ${remainingLabel(m.S.alarm_ogrz_wyciszenie_ms)}")
                        }
                    }
                    "Centrala i aplikacje wyciszone: ${details.joinToString("; ")}."
                }
            }
            Text(
                sharedMuteText,
                fontSize = 11.sp,
                color = when {
                    !muteStatusKnown -> Pal.Warn
                    mutedClasses.isNotEmpty() -> Pal.Cyan
                    else -> Pal.TextDim
                }
            )
            Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                AlarmBtn("10 min", false, Modifier.weight(1f)) {
                    AlarmCenter.snooze(p, 10)
                    AlarmMuteSync.request(ctx, alarmSignature, 10 * 60)
                    AlarmNotify.cancel(ctx); m.alarmUiTick++
                }
                AlarmBtn("1 h", false, Modifier.weight(1f)) {
                    AlarmCenter.snooze(p, 60)
                    AlarmMuteSync.request(ctx, alarmSignature, 60 * 60)
                    AlarmNotify.cancel(ctx); m.alarmUiTick++
                }
            }
            AlarmBtn("🔕 Wycisz do następnego alarmu", false, Modifier.fillMaxWidth()) {
                AlarmCenter.muteUntilNext(p)
                AlarmMuteSync.request(ctx, alarmSignature, 0)
                AlarmNotify.cancel(ctx); m.alarmUiTick++
            }
            if (AlarmCenter.snoozeLeftMin(p) > 0 || AlarmCenter.isMutedNow(p) || controllerMutedSignature.isNotEmpty()) {
                AlarmBtn("🔔 Zakończ wyciszenie na wszystkich urządzeniach", true, Modifier.fillMaxWidth()) {
                    AlarmCenter.unsilence(p)
                    AlarmMuteSync.request(
                        ctx,
                        controllerMutedSignature.ifEmpty { alarmSignature },
                        seconds = null
                    )
                    m.alarmUiTick++
                }
            }
        }

        // 5. Uprawnienia systemowe
        AlarmCard {
            Text("UPRAWNIENIA SYSTEMOWE", style = Txt.tiny, color = Pal.Cyan)
            val notifOk = AlarmNotify.hasPostNotifications(ctx)
            PermRow("Powiadomienia", notifOk,
                if (notifOk) "Zgoda jest włączona; widoczność nadal zależy od systemu i kanału."
                else "Powiadomienia alarmowe są zablokowane; dźwięk i działanie usługi też nie są gwarantowane.",
                btn = "Poproś o zgodę") { m.askNotifPerm = true }
            val fsiOk = AlarmNotify.canFullScreen(ctx)
            PermRow("Okienko na wierzchu", fsiOk,
                if (fsiOk) "Zezwolono na pełny ekran; system nadal może ograniczyć wyświetlenie."
                else "Bez tej zgody aplikacja nie może żądać alarmowego okna pełnoekranowego.",
                btn = if (fsiOk) null else "Otwórz ustawienia") {
                openSafely(ctx, AlarmNotify.fullScreenSettingsIntent(ctx)); m.alarmUiTick++
            }
            val batOk = remember(m.alarmUiTick) { isIgnoringBattery(ctx) }
            PermRow("Praca w tle (bateria)", batOk,
                if (batOk) "Wyjątek włączony — może zwiększać zużycie baterii; system/OEM nadal może ograniczyć usługę."
                else "Optymalizacja pozostaje włączona (zalecane dla baterii). Wyjątek jest opcjonalny i może zmniejszyć ryzyko opóźnień na części telefonów.",
                btn = if (batOk) null else "Ustawienia baterii") {
                openBatterySettings(ctx); m.alarmUiTick++
            }
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
