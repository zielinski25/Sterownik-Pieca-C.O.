package com.sterownikco.pro.ui

import androidx.compose.runtime.compositionLocalOf
import com.sterownikco.pro.core.AppModel

/** Model aplikacji udostępniany wszystkim widokom (odpowiednik globali w Piec.html). */
val LocalModel = compositionLocalOf<AppModel> { error("Brak LocalModel — owidź zawartość w SterownikApp {}") }
