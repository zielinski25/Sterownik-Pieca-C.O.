package com.sterownikco.pro

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.lifecycle.lifecycleScope
import com.sterownikco.pro.core.AppModel
import com.sterownikco.pro.ui.SterownikApp

/**
 * Jedyna aktywność panelu (odpowiednik `<body>` w Piec.html).
 * `onResume` / `onPause` włączają timery tak jak `setInterval` w oryginale:
 * 4 s odpytywanie statusu / 2 s tick symulacji / 15 min pogoda / 1 s arkusz.
 */
class MainActivity : ComponentActivity() {

    private lateinit var model: AppModel

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        model = AppModel(applicationContext, lifecycleScope)
        // APK: brak autologowania — modal logowania otwiera sam AppModel.
        setContent { SterownikApp(model) }
    }

    override fun onResume() {
        super.onResume()
        if (::model.isInitialized) model.onResume()
    }

    override fun onPause() {
        super.onPause()
        if (::model.isInitialized) model.onPause()
    }
}
