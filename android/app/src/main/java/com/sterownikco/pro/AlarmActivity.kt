package com.sterownikco.pro

import android.app.KeyguardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.core.AlarmCenter
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.Prefs
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.SterownikTheme

/* ══════════════════════════════════════════════════════════════════════════
   Pełnoekranowe OKIENKO ALARMU (dym / przegrzanie) — cel full-screen intent.
   Wyskakuje NAWET na zablokowanym ekranie i przy zminimalizowanej aplikacji:
   `showWhenLocked + turnScreenOn` (manifest + runtime) i prośba o zdjęcie
   klawiatury blokady. Nie znika samo — tylko świadomy wybór przycisku.
   ══════════════════════════════════════════════════════════════════════════ */

class AlarmActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(true)
            setTurnScreenOn(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                    WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
            )
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        try {
            (getSystemService(Context.KEYGUARD_SERVICE) as? KeyguardManager)
                ?.requestDismissKeyguard(this, null)
        } catch (e: Exception) { /* część nakładek odmawia — okienko i tak widać */ }

        val title = intent.getStringExtra("title") ?: "ALARM KOTŁA"
        val msg = intent.getStringExtra("msg") ?: "Wykryto zagrożenie — sprawdź kotłownię!"
        setContent {
            SterownikTheme {
                AlarmScreen(title = title, msg = msg,
                    onSnooze10 = { act { p -> AlarmCenter.snooze(p, 10) } },
                    onSnooze60 = { act { p -> AlarmCenter.snooze(p, 60) } },
                    onMuteNext = { act { p -> AlarmCenter.muteUntilNext(p) } },
                    onOpen = {
                        AlarmNotify.cancel(this)
                        startActivity(Intent(this, MainActivity::class.java).setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP))
                        finish()
                    })
            }
        }
    }

    private fun act(op: (Prefs) -> Unit) {
        try {
            op(Prefs(applicationContext))
        } catch (e: Exception) { /* ignore */ }
        AlarmNotify.cancel(this)
        finish()
    }

    @Deprecated("Powrót = schowanie, nie wyciszenie — alarm ma być świadomie obsłużony")
    override fun onBackPressed() {
        // Celowo: back tylko gasi ekran okienka (do HOME), powiadomienie zostaje.
        moveTaskToBack(true)
    }
}

@Composable
private fun AlarmScreen(
    title: String,
    msg: String,
    onSnooze10: () -> Unit,
    onSnooze60: () -> Unit,
    onMuteNext: () -> Unit,
    onOpen: () -> Unit
) {
    Box(
        Modifier.fillMaxSize().background(Pal.Bg).padding(20.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(
            Modifier.fillMaxWidth().background(Pal.Surface, RoundedCornerShape(18.dp)).padding(22.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text("🚨", fontSize = 52.sp)
            Text(title, fontSize = 20.sp, fontWeight = FontWeight.Black, color = Pal.Err, textAlign = TextAlign.Center)
            Text(msg, fontSize = 14.sp, color = Pal.Text, textAlign = TextAlign.Center, lineHeight = 20.sp)
            Text("Wybierz, jak wyciszyć ten alarm:", fontSize = 12.sp, color = Pal.TextDim, textAlign = TextAlign.Center)
            AlarmBtn("⏸ Wycisz na 10 minut", Pal.Cyan, onSnooze10)
            AlarmBtn("⏸ Wycisz na 1 godzinę", Pal.Cyan, onSnooze60)
            AlarmBtn("🔕 Wycisz do następnego alarmu", Pal.Violet, onMuteNext)
            AlarmBtn("🔥 Otwórz panel sterownika", Pal.Live, onOpen)
            Spacer(Modifier.height(2.dp))
            Text("Drzemka wygaśnie sama i alarm zadzwoni ponownie, gdy zagrożenie trwa.",
                fontSize = 11.sp, color = Pal.TextDim, textAlign = TextAlign.Center)
        }
    }
}

@Composable
private fun AlarmBtn(label: String, tint: androidx.compose.ui.graphics.Color, onClick: () -> Unit) {
    Box(
        Modifier.fillMaxWidth().height(48.dp)
            .background(Pal.rgba(255, 255, 255, .06f), RoundedCornerShape(12.dp))
            .clickable(onClick = onClick)
            .padding(horizontal = 12.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(label, fontSize = 13.5.sp, fontWeight = FontWeight.Bold, color = tint, textAlign = TextAlign.Center)
    }
}
