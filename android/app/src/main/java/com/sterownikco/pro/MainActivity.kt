package com.sterownikco.pro

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.sterownikco.pro.core.AlarmInfo
import com.sterownikco.pro.core.AlarmNotify
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.SterownikApp

/**
 * Jedyna aktywność panelu (odpowiednik `<body>` w Piec.html).
 * `onResume` / `onPause` włączają timery tak jak `setInterval` w oryginale:
 * 2 s odpytywanie statusu / 2 s tick zegara / 15 min pogoda / 1 s arkusz.
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
