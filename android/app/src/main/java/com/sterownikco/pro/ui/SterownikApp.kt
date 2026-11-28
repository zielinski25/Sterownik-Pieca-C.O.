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
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.asPaddingValues
import androidx.compose.foundation.layout.calculateStartPadding
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.displayCutoutPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.calculateEndPadding
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.padding
import androidx.compose.ui.platform.LocalLayoutDirection
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
import com.sterownikco.pro.core.AlarmMuteSync
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.core.ScreenLayout
import com.sterownikco.pro.ui.components.NavBar
import com.sterownikco.pro.ui.components.NavRail
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
            // Rozmiar okna (dp) decyduje o układzie — nigdy orientacja urządzenia.
            // 0.31.69: bez sztywnej ramki 412 dp; w niskim i szerokim oknie (telefon
            // w poziomie) nawigacja idzie na bok, żeby treść miała pełną wysokość.
            BoxWithConstraints(
                modifier = Modifier.fillMaxSize().background(Pal.Bg)
            ) {
                // [0.31.73] Pas pod paskiem statusu maluja teraz SAME paski: TopBar
                // i NavRail (tlo przed wcieciem). Osobny Box byl tu zbedny i malowal
                // drugi gradient na tym samym obszarze.
                val rail = ScreenLayout.useNavRail(maxWidth.value.toInt(), maxHeight.value.toInt())
                // [0.31.70/0.31.71] Motyw dopuszcza rysowanie pod wycieciem kamery
                // (w poziomie system zamalowywal ten pas na czarno). Interfejs musi byc
                // od niego odsuniety, ale tak, zeby przy krawedzi nie powstal pas tla.
                // Dlatego w trybie szyny wciecie pochlania SAMA szyna (jej tlo dochodzi
                // do krawedzi ekranu), a tresc obok nie dostaje juz zadnego paddingu.
                val cutoutStart = WindowInsets.displayCutout.asPaddingValues()
                    .calculateStartPadding(LocalLayoutDirection.current)
                // [0.31.74] Wciecie paska nawigacji liczymy sami — i bierzemy
                // z niego wylacznie koniec i dol.
                val navVals = WindowInsets.navigationBars.asPaddingValues()
                val navEnd = navVals.calculateEndPadding(LocalLayoutDirection.current)
                val navBottom = navVals.calculateBottomPadding()
                Row(Modifier.fillMaxSize()) {
                    if (rail) NavRail(page = m.page, startInset = cutoutStart) { m.navigate(it) }
                    Column(
                        Modifier.weight(1f).fillMaxHeight()
                            .then(if (rail) Modifier else Modifier.displayCutoutPadding())
                            // [0.31.74] Tylko KONIEC i DOL paska nawigacji.
                            // Gotowy `navigationBarsPadding()` bral wszystkie cztery
                            // strony, a po lewej doliczal wyciecie kamery (39 dp):
                            // szyna 119 dp + drugie 39 dp pustki obok = 158 dp
                            // z 813 dp szerokosci ekranu zjadalo 1/5 ekranu.
                            .padding(end = navEnd, bottom = navBottom)
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
                        if (!rail) NavBar(page = m.page) { m.navigate(it) }
                    }
                }

                // `#sheet` + `#scrim`
                if (m.sheet != null) SheetHost(m)

                // HTML-owy #toast ma wyższy z-index niż arkusz; Compose rysuje późniejsze dzieci na wierzchu.
                m.toast?.let { t ->
                    Box(Modifier.align(Alignment.BottomCenter)) {
                        Toast(text = t.cmd, stage = t.stage, cls = t.cls)
                    }
                }

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
        m.clearAlarmPopup()
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
            PopupBtn("⏸ Wycisz na 10 minut", Pal.Cyan) {
                done {
                    AlarmCenter.snooze(m.prefs, 10)
                    AlarmMuteSync.request(m.ctx, info.sig, 10 * 60)
                }
            }
            PopupBtn("⏸ Wycisz na 1 godzinę", Pal.Cyan) {
                done {
                    AlarmCenter.snooze(m.prefs, 60)
                    AlarmMuteSync.request(m.ctx, info.sig, 60 * 60)
                }
            }
            PopupBtn("🔕 Wycisz do następnego alarmu", Pal.Violet) {
                done {
                    AlarmCenter.muteUntilNext(m.prefs)
                    AlarmMuteSync.request(m.ctx, info.sig, 0)
                }
            }
            PopupBtn("Zamknij podgląd", Pal.TextDim) { m.dismissAlarmPopup() }
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
