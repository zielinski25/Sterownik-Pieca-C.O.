package com.sterownikco.pro

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Bundle
import android.view.MotionEvent
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.lifecycle.lifecycleScope
import com.sterownikco.pro.core.AlarmInfo
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.SterownikApp

/**
 * Jedyna aktywność panelu (odpowiednik `<body>` w Piec.html).
 * `onResume` / `onPause` włączają timery tak jak `setInterval` w oryginale:
 * 8 s odpytywanie statusu / 2 s tick zegara / 15 min pogoda / 1 s arkusz.
 */
class MainActivity : ComponentActivity() {

    private lateinit var model: AppModel
    private var alarmReceiverRegistered = false
    private val alarmStateReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent?) {
            if (!::model.isInitialized) return
            val received = intent ?: return
            when (received.action) {
                AlarmNotify.ACTION_ALARM_UPDATED -> {
                    val info = alarmInfoFrom(received) ?: return
                    model.showAlarmPopup(info)
                }
                AlarmNotify.ACTION_ALARM_CLEARED -> model.clearAlarmPopup()
            }
        }
    }

    private fun alarmInfoFrom(intent: Intent): AlarmInfo? {
        val kind = intent.getStringExtra(AlarmNotify.EXTRA_ALARM_KIND) ?: return null
        val title = intent.getStringExtra(AlarmNotify.EXTRA_ALARM_TITLE) ?: return null
        val message = intent.getStringExtra(AlarmNotify.EXTRA_ALARM_MESSAGE) ?: return null
        val signature = intent.getStringExtra(AlarmNotify.EXTRA_ALARM_SIGNATURE) ?: return null
        return AlarmInfo(kind, title, message, signature)
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // [0.31.70] Ten sam efekt co w motywie (values-v27/themes.xml), ale
        // ustawiony w kodzie: czesc urzadzen (m.in. Samsung) ignoruje sam
        // atrybut z motywu i dalej zamalowuje pas przy wycieciu kamery na
        // czarno w poziomie. Ustawienie na oknie przed pierwszym rysowaniem
        // dziala na wszystkich. Stala SHORT_EDGES istnieje od API 28.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        // [0.31.72] TRYB edge-to-edge. Bez tego system nie wpuszcza tresci w obszar
        // wyciecia kamery: okno rozciaga sie na caly ekran, ale WIDOK tresci (Compose)
        // dostaje od systemu 37 dp marginesu z lewej. Dlatego 0.31.71 nie mogla nic
        // zmienic — pas tla nie byl juz naszym paddingiem, tylko wcieciem systemowym.
        // Teraz tresc rysuje pod paskami, a odstepy ustawiamy sami (Insets w UI).
        WindowCompat.setDecorFitsSystemWindows(window, false)
        model = AppModel(applicationContext, lifecycleScope)
        applyAlarmIntent(intent)
        // AppModel przywraca zapamiętane konto albo otwiera modal logowania.
        setContent { SterownikApp(model) }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        applyAlarmIntent(intent)
    }

    private fun applyAlarmIntent(intent: Intent) {
        if (!AlarmNotify.hasCurrentAlarmNonce(applicationContext, intent)) return
        val info = alarmInfoFrom(intent) ?: return
        model.showAlarmPopup(info)
    }

    override fun onStart() {
        super.onStart()
        val filter = IntentFilter().apply {
            addAction(AlarmNotify.ACTION_ALARM_UPDATED)
            addAction(AlarmNotify.ACTION_ALARM_CLEARED)
        }
        ContextCompat.registerReceiver(this, alarmStateReceiver, filter, ContextCompat.RECEIVER_NOT_EXPORTED)
        alarmReceiverRegistered = true
    }

    override fun onStop() {
        if (alarmReceiverRegistered) {
            try { unregisterReceiver(alarmStateReceiver) } catch (e: Exception) { /* ignore */ }
            alarmReceiverRegistered = false
        }
        super.onStop()
    }

    /**
     * [0.31.58] Każde dotknięcie ekranu to „dowód obecności": heartbeat podglądu
     * (`podglad_wl`, dzięki któremu centrala czyta komendy co 2 s) gaśnie po
     * 10 min ciszy i wznawia się natychmiast przy pierwszym dotknięciu.
     * Celowo `dispatchTouchEvent`, a nie nasłuch na pojedynczych kontrolkach —
     * łapie też przewijanie i kliknięcia w tło.
     */
    override fun dispatchTouchEvent(ev: MotionEvent?): Boolean {
        if (::model.isInitialized && ev?.actionMasked == MotionEvent.ACTION_DOWN) {
            model.noteInteraction()
        }
        return super.dispatchTouchEvent(ev)
    }

    override fun onResume() {
        super.onResume()
        if (::model.isInitialized) {
            model.onResume()
            model.alarmUiTick++ // odczytaj ponownie uprawnienia i wyjątki OEM po powrocie do aplikacji
        }
    }

    override fun onPause() {
        super.onPause()
        if (::model.isInitialized) model.onPause()
    }
}
