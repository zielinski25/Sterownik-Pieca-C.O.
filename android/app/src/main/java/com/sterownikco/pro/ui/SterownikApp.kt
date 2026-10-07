package com.sterownikco.pro.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
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
import com.sterownikco.pro.ui.screens.demo.DemoDrawer
import com.sterownikco.pro.ui.theme.Dimens
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.SterownikTheme

/**
 * Korzeń aplikacji = `<div class="app">` z Piec.html: górny pasek, przewijana
 * strona, dolna nawigacja, a na to: scrim + arkusz (`#sheet`), toast,
 * przycisk DEMO i modal logowania.
 */
@Composable
fun SterownikApp(m: AppModel) {
    SterownikTheme {
        CompositionLocalProvider(LocalModel provides m) {
            BoxWithConstraints(
                Modifier.fillMaxSize().background(Pal.Stage),
                contentAlignment = Alignment.TopCenter
            ) {
                val wide = maxWidth > 560.dp
                LaunchedEffect(wide) { m.layoutMode = if (wide) "desktop" else "phone" }
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

                // `#demoBtn` — stały przycisk nad nawigacją, po prawej
                Box(
                    Modifier.align(Alignment.BottomEnd).padding(end = 14.dp, bottom = 86.dp)
                        .height(38.dp)
                        .background(Pal.Surface2, RoundedCornerShape(19.dp))
                        .border(1.dp, Pal.rgba(0, 212, 245, .6f), RoundedCornerShape(19.dp))
                        .clickable { m.demoOpen = !m.demoOpen }
                        .padding(horizontal = 16.dp),
                    contentAlignment = Alignment.Center
                ) {
                    Text("⚙ DEMO", fontSize = 12.sp, fontWeight = FontWeight.ExtraBold, color = Pal.Cyan)
                }

                // `#toast`
                m.toast?.let { t ->
                    Box(Modifier.align(Alignment.BottomCenter)) {
                        Toast(text = t.stage, stage = t.cmd, cls = t.cls)
                    }
                }

                // `#sheet` + `#scrim`
                if (m.sheet != null) SheetHost(m)
                if (m.demoOpen) DemoDrawer(m)
                if (m.authOpen) AuthModal(m)
            }
        }
    }
}
