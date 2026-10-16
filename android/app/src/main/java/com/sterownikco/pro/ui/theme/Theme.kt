package com.sterownikco.pro.ui.theme

import android.app.Activity
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Typography
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** Wymiary 1:1 z arkusza CSS (px → dp). */
object Dimens {
    val radiusTile = 18.dp
    val radiusCard = 16.dp
    val radiusSheet = 24.dp
    val topBarH = 62.dp
    val navH = 74.dp
    val navBtnH = 56.dp
    val tileH = 126.dp
    val quickH = 64.dp
    val analysisH = 74.dp
    val trendH = 75.dp
    val chartH = 220.dp
    val weatherChartH = 180.dp
    val sunArcH = 68.dp
    val pagePad = 12.dp
    val pagePadH = 14.dp
    val gap = 8.dp

    /** Szerokość ramki symulatora telefonu (tryb PC/tablet). */
    val frameW = 420.dp
    val frameH = 920.dp
}

/**
 * Style typografii odpowiadające rozmiarom z CSS panelu.
 * `px` z Piec.html oddają rozmiary w dp/na phones, więc mapujemy 1:1 na sp.
 */
object Txt {
    val mono = FontFamily.Monospace
    val sans = FontFamily.SansSerif

    val topTitle = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 18.sp, letterSpacing = 0.4.sp, color = Pal.White, lineHeight = 20.sp)
    val topSub = TextStyle(fontFamily = sans, fontWeight = FontWeight.SemiBold, fontSize = 9.2.sp, color = Pal.TextDim, lineHeight = 11.sp)
    val chip = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 9.sp, letterSpacing = 0.36.sp, lineHeight = 11.sp)
    val h2 = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 18.sp, letterSpacing = 0.4.sp, color = Pal.White)
    val lead = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 10.sp, color = Pal.TextDim)
    val heroTemp = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 38.sp, letterSpacing = (-0.8).sp, color = Pal.White, lineHeight = 40.sp)
    val heroK = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 8.5.sp, letterSpacing = 0.7.sp, color = Pal.Cyan)
    val heroD = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 8.8.sp, color = Pal.TextDim)
    val tileTitle = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 8.5.sp, letterSpacing = 0.5.sp, color = Pal.TileTitle)
    val tileValue = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 21.sp, letterSpacing = (-0.2).sp, color = Pal.TileValue)
    val tileValueSm = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 15.sp, letterSpacing = (-0.1).sp, color = Pal.TileValue)
    val tileUnit = TextStyle(fontFamily = sans, fontWeight = FontWeight.SemiBold, fontSize = 10.sp, color = Pal.TextDim)
    val tileDesc = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 8.sp, color = Pal.TextDim)
    val badge = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 7.2.sp, letterSpacing = 0.2.sp, lineHeight = 9.sp)
    val sect = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 9.5.sp, letterSpacing = 0.8.sp, color = Pal.TextDim)
    val pill = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 8.5.sp, letterSpacing = 0.5.sp)
    val quickTitle = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 7.5.sp, letterSpacing = 0.5.sp, color = Pal.TextDim2)
    val quickVal = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 11.sp, color = Pal.White, lineHeight = 13.sp)
    val quickSub = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 7.5.sp, color = Pal.TextDim)
    val hStatLbl = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 7.sp, letterSpacing = 0.35.sp, color = Pal.TextDim2)
    val hStatVal = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 9.5.sp, color = Pal.White, lineHeight = 11.sp)
    val navLabel = TextStyle(fontFamily = sans, fontWeight = FontWeight.Bold, fontSize = 9.sp)
    val cardTitle = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 11.5.sp, color = Pal.White, lineHeight = 14.sp)
    val cardDesc = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 8.8.sp, color = Pal.TextDim, lineHeight = 11.sp)
    val sheetTitle = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 16.sp, color = Pal.White)
    val note = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 10.sp, color = Pal.TextDim, lineHeight = 14.sp)
    val rowLabel = TextStyle(fontFamily = sans, fontWeight = FontWeight.Bold, fontSize = 11.5.sp, color = Pal.White, lineHeight = 14.sp)
    val rowDesc = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 8.5.sp, color = Pal.TextDim, lineHeight = 11.sp)
    val secHead = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 9.sp, letterSpacing = 0.7.sp, color = Pal.TextDim)
    val btn = TextStyle(fontFamily = sans, fontWeight = FontWeight.Bold, fontSize = 11.5.sp)
    val seg = TextStyle(fontFamily = sans, fontWeight = FontWeight.Bold, fontSize = 11.5.sp)
    val inp = TextStyle(fontFamily = mono, fontWeight = FontWeight.Medium, fontSize = 12.sp, color = Pal.White)
    val diagLbl = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 8.5.sp, letterSpacing = 0.5.sp, color = Pal.Cyan)
    val diagVal = TextStyle(fontFamily = mono, fontWeight = FontWeight.ExtraBold, fontSize = 20.sp, color = Pal.White)
    val diagSub = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 9.sp, color = Pal.TextDim, lineHeight = 12.sp)
    val monoSm = TextStyle(fontFamily = mono, fontWeight = FontWeight.Medium, fontSize = 9.sp, color = Pal.TextDim)
    val monoVal = TextStyle(fontFamily = mono, fontWeight = FontWeight.ExtraBold, fontSize = 12.sp, color = Pal.White)
    val tiny = TextStyle(fontFamily = sans, fontWeight = FontWeight.Medium, fontSize = 8.sp, color = Pal.TextDim)
    val mcardLbl = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 8.sp, letterSpacing = 0.4.sp, color = Pal.TextDim)
    val mcardVal = TextStyle(fontFamily = sans, fontWeight = FontWeight.ExtraBold, fontSize = 13.sp, color = Pal.White, lineHeight = 15.sp)
    val mcardSub = TextStyle(fontFamily = sans, fontWeight = FontWeight.SemiBold, fontSize = 7.5.sp, color = Pal.TextDim2)
    val term = TextStyle(fontFamily = mono, fontWeight = FontWeight.Normal, fontSize = 9.5.sp, lineHeight = 13.sp, color = Pal.TermInfo)
}

@Composable
fun SterownikTheme(
    @Suppress("UNUSED_PARAMETER") darkTheme: Boolean = isSystemInDarkTheme(),
    content: @Composable () -> Unit
) {
    val scheme = darkColorScheme(
        primary = Pal.Cyan,
        onPrimary = Color(0xFF00131F),
        secondary = Pal.Accent,
        background = Pal.Bg,
        onBackground = Pal.Text,
        surface = Pal.Surface,
        onSurface = Pal.Text,
        surfaceVariant = Pal.Surface2,
        onSurfaceVariant = Pal.TextDim,
        outline = Pal.Border,
        outlineVariant = Pal.Border,
        error = Pal.Err,
        onError = Color(0xFF2B0A11)
    )
    val type = Typography(
        bodyMedium = TextStyle(fontFamily = Txt.sans, fontSize = 12.sp, color = Pal.Text),
        labelMedium = TextStyle(fontFamily = Txt.sans, fontSize = 10.sp, color = Pal.TextDim)
    )
    val view = LocalView.current
    if (!view.isInEditMode) {
        SideEffect {
            val w = view.context as? Activity
            w?.window?.apply {
                statusBarColor = Pal.Top.toArgb()
                navigationBarColor = Pal.Nav.toArgb()
            }
        }
    }
    MaterialTheme(colorScheme = scheme, typography = type, content = content)
}
