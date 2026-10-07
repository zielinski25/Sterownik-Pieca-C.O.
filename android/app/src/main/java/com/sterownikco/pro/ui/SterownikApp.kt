package com.sterownikco.pro.ui

import android.Manifest
import android.os.Build
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.components.NavBar
import com.sterownikco.pro.ui.components.Toast
import com.sterownikco.pro.ui.components.TopBar
import com.sterownikco.pro.ui.screens.AuthModal
import com.sterownikco.pro.ui.screens.ChartsPage
import com.sterownikco.pro.ui.screens.Dashboard
import com.sterownikco.pro.ui.screens.MorePage
import com.sterownikco.pro.ui.screens.SheetHost
import com.sterownikco.pro.ui.screens.SettingsPage
import com.sterownikco.pro.ui.screens.WeatherPage
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.SterownikTheme

/**
 * Korzeń aplikacji = `<div class="app">` z Piec.html: górny pasek, przewijana
 * strona, dolna nawigacja, a na to: scrim + arkusz (`#sheet`), toast,
 * modal logowania.
 */
@Composable
fun SterownikApp(m: AppModel) {
    SterownikTheme {
        CompositionLocalProvider(LocalModel provides m) {
            BoxWithConstraints(
                modifier = Modifier.fillMaxSize().background(Pal.Stage)
            ) {
                val wide = maxWidth > 560.dp
                Column(
                    Modifier
                        .then(if (wide) Modifier.width(m.frameWidth.dp).fillMaxHeight(0.98f) else Modifier.fillMaxSize())
                        .background(Pal.Bg)
                        .then(
                            if (wide) Modifier.border(
                                1.dp, Pal.BorderStrong, RoundedCornerShape(22.dp)
                            ) else Modifier
                        )
                ) {
                    TopBar(m)
                    Box(Modifier.weight(1f).fillMaxWidth()) {
                        when (m.page) {
                            0 -> Dashboard(m)
                            1 -> ChartsPage(m)
                            2 -> WeatherPage(m)
                            3 -> SettingsPage(m)
                            else -> MorePage(m)
                        }
                    }
                    NavBar(page = m.page) { m.navigate(it) }
                }

                // `#toast`
                m.toast?.let { t ->
                    Box(Modifier.align(Alignment.BottomCenter)) {
                        Toast(text = t.cmd, stage = t.stage, cls = t.cls)
                    }
                }

                // `#sheet` + `#scrim`
                if (m.sheet != null) SheetHost(m)
                if (m.authOpen) AuthModal(m)
                // Okienko alarmu w aplikacji (system nie odpala FSI na 1. planie).
                if (m.alarmPopup != null) AlarmPopup(m)
                // Jednorazowa prośba o zgodę na powiadomienia (Android 13+).
                val notifLauncher = rememberLauncherForActivityResult(
                    ActivityResultContracts.RequestPermission()
                ) { m.askNotifPerm = false }
                LaunchedEffect(m.askNotifPerm) {
                    if (m.askNotifPerm && Build.VERSION.SDK_INT >= 33 &&
                        !AlarmNotify.hasPostNotifications(m.ctx)
                    ) notifLauncher.launch(Manifest.permission.POST_NOTIFICATIONS)
                    else if (m.askNotifPerm) m.askNotifPerm = false
                }
            }
        }
    }
}

/**
 * Okienko alarmu dymu / przegrzania W APLIKACJI (APK-only).
 * System nie odpala full-screen intent, gdy aplikacja jest na wierzchu —
 * wtedy dźwięk gra z powiadomienia, a wybór drzemki jest tutaj.
 */
@Composable
private fun AlarmPopup(m: AppModel) {
    val info = m.alarmPopup ?: return
    fun done(op: () -> Unit) {
        try { op() } catch (e: Exception) { /* ignore */ }
        AlarmNotify.cancel(m.ctx)
        m.alarmPopup = null
    }
    Box(
        Modifier.fillMaxSize().background(Color(0xB0000000))
            .clickable(onClick = {}), // scrim zjada dotyki — wybór tylko przyciskiem
        contentAlignment = Alignment.Center
    ) {
        Column(
            Modifier.fillMaxWidth().padding(horizontal = 26.dp)
                .background(Pal.Surface, RoundedCornerShape(18.dp))
                .border(2.dp, Pal.Err, RoundedCornerShape(18.dp))
                .padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(10.dp)
        ) {
            Text("🚨", fontSize = 40.sp)
            Text(info.title, fontSize = 17.sp, fontWeight = FontWeight.Black,
                color = Pal.Err, textAlign = TextAlign.Center)
            Text(info.msg, fontSize = 13.sp, color = Pal.Text, textAlign = TextAlign.Center)
            PopupBtn("⏸ Wycisz na 10 minut", Pal.Cyan) { done { AlarmCenter.snooze(m.prefs, 10) } }
            PopupBtn("⏸ Wycisz na 1 godzinę", Pal.Cyan) { done { AlarmCenter.snooze(m.prefs, 60) } }
            PopupBtn("🔕 Wycisz do następnego alarmu", Pal.Violet) { done { AlarmCenter.muteUntilNext(m.prefs) } }
            PopupBtn("Zamknij podgląd", Pal.TextDim) { m.alarmPopup = null }
        }
    }
}

@Composable
private fun PopupBtn(label: String, tint: Color, onClick: () -> Unit) {
    Box(
        Modifier.fillMaxWidth().height(46.dp)
            .background(Pal.rgba(255, 255, 255, .06f), RoundedCornerShape(12.dp))
            .clickable(onClick = onClick).padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 13.sp, fontWeight = FontWeight.Bold, color = tint, textAlign = TextAlign.Center)
    }
}
