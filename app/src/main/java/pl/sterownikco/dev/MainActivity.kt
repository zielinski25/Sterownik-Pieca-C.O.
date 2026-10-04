package pl.sterownikco.dev

import android.app.Activity
import android.app.Dialog
import android.content.Context
import android.content.Intent
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.graphics.drawable.StateListDrawable
import android.os.Build
import android.os.Bundle
import android.view.WindowInsets
import android.text.TextUtils
import android.view.Gravity
import android.view.HapticFeedbackConstants
import android.view.View
import android.view.Window
import android.view.WindowManager
import android.widget.*
import android.util.Log
import org.json.JSONObject
import java.text.SimpleDateFormat
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.util.*
import java.util.concurrent.*
import kotlin.math.roundToInt

/**
 * STEROWNIK CO — v0.30.2 native control room. Visual cleanup + stable Chart Studio.
 * UI is native Android Views. Firebase/Auth/telemetry contracts remain unchanged.
 * The WWW panel is used only as functional/visual reference; Android UI remains native.
 */
class MainActivity : Activity() {
    private val firebase = FirebaseAuthRest()
    private val commands = FirebaseCommandRest()
    private lateinit var authStore: SecureSessionStore
    private val executor = Executors.newSingleThreadExecutor()
    // Separate lanes: a 30 s ACK wait or a 30-day history download must not block each other.
    private val historyExecutor = Executors.newSingleThreadExecutor()
    private val commandExecutor = Executors.newSingleThreadExecutor()
    private val scheduler = Executors.newSingleThreadScheduledExecutor()
    private val historyRequestId = java.util.concurrent.atomic.AtomicInteger(0)
    @Volatile private var appVisible = true

    @Volatile private var currentIdToken: String? = null
    @Volatile private var tokenExpiresAtMs = 0L
    @Volatile private var lastStatusData: JSONObject? = null
    @Volatile private var sessionNeedsLogin = false
    @Volatile private var pollingStarted = false
    @Volatile private var lastStatusAtMs = 0L
    private var maintenanceStarted = false
    @Volatile private var lastStatusOnline = false

    // Advanced sensor filters — thresholds tuned from 1504 real measurements (1-min intervals)
    // maxChangePerMinute = 3× observed max legitimate rate (data shows: t_zewn=0.5, rest=0.3 °C/min)
    private val filterOutside = AdvancedSensorFilter("t_zewn", bufferSize = 8, maxDeviationFromMedian = 3f, maxChangePerMinute = 1.5f, absoluteMin = -30f, absoluteMax = 60f)
    private val filterHeating = AdvancedSensorFilter("t_ogrz", bufferSize = 8, maxDeviationFromMedian = 5f, maxChangePerMinute = 1.0f, absoluteMin = 0f, absoluteMax = 160f)
    private val filterBoiler = AdvancedSensorFilter("t_bojler", bufferSize = 8, maxDeviationFromMedian = 5f, maxChangePerMinute = 1.0f, absoluteMin = 0f, absoluteMax = 160f)
    private val filterPanel = AdvancedSensorFilter("t_panel", bufferSize = 8, maxDeviationFromMedian = 8f, maxChangePerMinute = 1.5f, absoluteMin = -30f, absoluteMax = 160f)
    private val filterRoom = AdvancedSensorFilter("t_pokoj", bufferSize = 8, maxDeviationFromMedian = 3f, maxChangePerMinute = 0.5f, absoluteMin = -10f, absoluteMax = 50f)
    private val filterPressure = AdvancedSensorFilter("cisnienie", bufferSize = 8, maxDeviationFromMedian = 3f, maxChangePerMinute = 1.5f, absoluteMin = 900f, absoluteMax = 1100f)
    private val filterHumidity = AdvancedSensorFilter("wilgotnosc", bufferSize = 8, maxDeviationFromMedian = 5f, maxChangePerMinute = 5f, absoluteMin = 0f, absoluteMax = 100f)
    
    // Sensor health monitors — thresholds based on thermal physics + real usage:
    // t_zewn: changes with weather, BUT can be in stimulation/testing — allow 4h
    // t_ogrz: thermal mass of water+iron, can stabilize 3h when furnace off
    // t_bojler: large thermal mass, 6h stable when not heating CWU — NORMAL
    // t_panel: near furnace, 3h stable OK
    // t_pokoj: thermally insulated room, 6h stable — NORMAL (observed 363 min!)
    // cisnienie: barely changes, 12h of same value is normal
    // wilgotnosc: sensor works (user confirmed today), was offline in old CSV
    private val healthOutside = SensorHealthMonitor("t_zewn", maxStagnationMinutes = 240, tolerance = 0.05f)
    private val healthHeating = SensorHealthMonitor("t_ogrz", maxStagnationMinutes = 180, tolerance = 0.05f)
    private val healthBoiler = SensorHealthMonitor("t_bojler", maxStagnationMinutes = 360, tolerance = 0.05f)
    private val healthPanel = SensorHealthMonitor("t_panel", maxStagnationMinutes = 180, tolerance = 0.05f)
    private val healthRoom = SensorHealthMonitor("t_pokoj", maxStagnationMinutes = 360, tolerance = 0.05f)
    private val healthPressure = SensorHealthMonitor("cisnienie", maxStagnationMinutes = 720, tolerance = 0.05f)
    private val healthHumidity = SensorHealthMonitor("wilgotnosc", maxStagnationMinutes = 360, tolerance = 0.5f)

    private lateinit var root: FrameLayout
    private lateinit var content: FrameLayout
    private lateinit var title: TextView
    private lateinit var subtitle: TextView
    private lateinit var connectionChip: TextView
    private lateinit var logout: FrameLayout
    private val navButtons = mutableListOf<LinearLayout>()
    private lateinit var heroTemp: TextView
    private lateinit var heroMode: TextView
    private lateinit var heroPump: TextView
    private lateinit var heroAlarm: TextView
    private lateinit var heroState: TextView
    private lateinit var alarmBanner: LinearLayout
    private lateinit var alarmBannerText: TextView
    private lateinit var heroBoiler: BoilerHeroView
    private lateinit var heroOverview: DashboardOverviewView
    private lateinit var dashboardView: ScrollView
    private lateinit var chartsView: ScrollView
    private lateinit var settingsView: ScrollView
    private lateinit var moreView: ScrollView
    private lateinit var weatherView: ScrollView
    private lateinit var loginView: LinearLayout
    private lateinit var tempChart: ProfessionalTelemetryChartView
    private lateinit var servoChart: ProfessionalTelemetryChartView
    private lateinit var historyInfo: TextView
    private lateinit var quickPumpView: View
    private lateinit var quickServoView: View
    private lateinit var quickMixerView: View
    private val tileViews = LinkedHashMap<TileId, DashboardTileView>()

    private enum class TileId {
        OUTSIDE, HEATING, BOILER, PANEL, ROOM, PRESSURE, HUMIDITY, PUMP, SERVO, MIXER, SMOKE, CHARTS, CLOCK
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        authStore = SecureSessionStore(this)
        buildUi()
        executor.execute { restoreSession() }
    }

    private fun buildUi() {
        root = FrameLayout(this).apply { setBackgroundColor(C.bg) }
        setContentView(root)
        buildMainShell()
        buildLogin()
        showChecking()
    }

    private fun buildMainShell() {
        val shell = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(C.bg)
        }

        val top = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(15), dp(8), dp(11), dp(8))
            background = roundedGradient(C.top, 0xFF0A1928.toInt(), 0x553A5E79, 0x2A334A5D, 18)
        }
        val brand = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_VERTICAL }
        title = label("STEROWNIK CO", 18.8f, Color.WHITE, true).apply { isSingleLine = true }
        subtitle = label("CENTRALA • piec_co", 9.2f, C.textDim, false, dp(2)).apply {
            isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        }
        brand.addView(title); brand.addView(subtitle)
        top.addView(brand, LinearLayout.LayoutParams(0, -2, 1f))

        connectionChip = pill("● LIVE", C.liveBg, C.live, 0).apply { isSingleLine = true }
        top.addView(connectionChip, lp(88, 38))
        logout = iconButton(NativeIconView.Icon.MORE, "Menu sesji") { showSessionMenu() }
        top.addView(logout, lpS(40, 38, 6))
        shell.addView(top, LinearLayout.LayoutParams(-1, dp(62)))

        content = FrameLayout(this)
        shell.addView(content, LinearLayout.LayoutParams(-1, 0, 1f))

        val nav = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(9), dp(6), dp(9), dp(9))
            background = roundedGradient(C.nav, 0xFF0B1725.toInt(), 0x463B566C, 0x243A5268, 20)
        }
        val defs = listOf(
            NativeIconView.Icon.DASHBOARD to "Dashboard",
            NativeIconView.Icon.CHART to "Wykresy",
            NativeIconView.Icon.OUTSIDE to "Pogoda",
            NativeIconView.Icon.SETTINGS to "Ustawienia",
            NativeIconView.Icon.MORE to "Więcej"
        )
        defs.forEachIndexed { idx, (icon, name) ->
            val b = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                background = navBg(idx == 0)
                isClickable = true; isFocusable = true
                setPadding(dp(3), dp(4), dp(3), dp(4))
                foreground = RippleDrawable(ColorStateList.valueOf(0x35FFFFFF), null, null)
                setOnClickListener { navigate(idx) }
            }
            val iv = NativeIconView(this).apply { this.icon = icon; tint = if (idx == 0) C.cyan else C.textDim2; active = idx == 0 }
            val tv = label(name, 9.0f, if (idx == 0) C.cyan else C.textDim2, true).apply { gravity = Gravity.CENTER; includeFontPadding = false; isSingleLine = true }
            b.addView(iv, lp(29, 29)); b.addView(tv, lp(-1, 19, 2))
            b.tag = tv
            navButtons += b
            nav.addView(b, LinearLayout.LayoutParams(0, dp(56), 1f).apply { setMargins(dp(3), 0, dp(3), 0) })
        }
        shell.addView(nav, LinearLayout.LayoutParams(-1, dp(74)))
        root.addView(shell, FrameLayout.LayoutParams(-1, -1))
        installInsets(shell)

        dashboardView = buildDashboard()
        chartsView = buildCharts()
        weatherView = buildWeather()
        settingsView = buildSettings()
        moreView = buildMore()
        content.addView(dashboardView, FrameLayout.LayoutParams(-1, -1))
        content.addView(chartsView, FrameLayout.LayoutParams(-1, -1))
        content.addView(weatherView, FrameLayout.LayoutParams(-1, -1))
        content.addView(settingsView, FrameLayout.LayoutParams(-1, -1))
        content.addView(moreView, FrameLayout.LayoutParams(-1, -1))
        chartsView.visibility = View.GONE
        weatherView.visibility = View.GONE
        settingsView.visibility = View.GONE
        moreView.visibility = View.GONE
    }

    private fun buildLogin() {
        loginView = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            gravity = Gravity.CENTER
            setPadding(dp(24), dp(24), dp(24), dp(24))
            visibility = View.GONE
        }
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(26), dp(26), dp(26), dp(26))
            background = rounded(C.surface, 24, C.border)
        }
        card.addView(label("STEROWNIK CO", 26f, Color.WHITE, true))
        card.addView(label("Panel sterowania instalacją", 12.5f, C.textDim, false, dp(5)))
        val email = edit("E-mail operatora", false)
        val pass = edit("Hasło", true)
        card.addView(email, lp(-1, 54, 15))
        card.addView(pass, lp(-1, 54, 11))
        val msg = label("Sesja zostanie zapamiętana bez zapisywania hasła.", 10.8f, C.textDim, false, dp(3))
        card.addView(msg)
        val button = action("ZALOGUJ", C.accent, C.bg)
        card.addView(button, lp(-1, 54, 15))
        button.setOnClickListener { doLogin(email.text.toString().trim(), pass.text.toString(), msg, button) }
        loginView.addView(card, LinearLayout.LayoutParams(minOf(dp(390), resources.displayMetrics.widthPixels - dp(32)), -2))
        root.addView(loginView, FrameLayout.LayoutParams(-1, -1))
    }


    private fun buildDashboard(): ScrollView {
        val scroll = ScrollView(this).apply {
            overScrollMode = View.OVER_SCROLL_IF_CONTENT_SCROLLS
            clipToPadding = false
            setPadding(0, 0, 0, dp(24))
            isFillViewport = false
        }
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(15), dp(10), dp(15), dp(20))
        }

        // --- SYSTEM STRIP: compact, always useful, visually light ---
        val systemStrip = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(10), dp(7), dp(10), dp(7))
            background = rounded(0x18111F35, 16, C.border)
        }
        val brandDot = NativeIconView(this).apply {
            icon = NativeIconView.Icon.SHIELD
            tint = C.cyan
            active = true
            background = rounded(0x1A2FD7F5, 12, 0x663CCCE8)
        }
        systemStrip.addView(brandDot, lp(32, 32))
        val brandCopy = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        brandCopy.addView(label("CENTRALA CO", 9.4f, Color.WHITE, true))
        brandCopy.addView(label("stan instalacji w czasie rzeczywistym", 7.5f, C.textDim2, false, dp(1)))
        systemStrip.addView(brandCopy, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(8) })
        heroState = pill("CZUWANIE", C.liveBg, C.live, 0).apply { textSize = 8.2f }
        systemStrip.addView(heroState, lpS(94, 30, 6))
        box.addView(systemStrip, lp(-1, -2, 0))

        alarmBanner = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(10), dp(7), dp(10), dp(7))
            background = pressableBackground(0x3B4B1624, 0x885B2E40.toInt(), 14)
            visibility = View.GONE
        }
        alarmBanner.addView(NativeIconView(this).apply { icon = NativeIconView.Icon.SHIELD; tint = C.err; active = true }, lp(22,22))
        alarmBannerText = label("ALARM — sprawdź instalację", 8.8f, 0xFFFFB4C1.toInt(), true).apply { isSingleLine = true; ellipsize = TextUtils.TruncateAt.END }
        alarmBanner.addView(alarmBannerText, LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(6)})
        alarmBanner.addView(NativeIconView(this).apply{icon=NativeIconView.Icon.NEXT;tint=0xFFFF9DAE.toInt()},lp(20,20))
        alarmBanner.setOnClickListener { showSmokeMenu() }
        box.addView(alarmBanner, lp(-1,-2,8))

        // --- HERO: one strong visual focus, deliberately compact ---
        val hero = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(16), dp(14), dp(12), dp(14))
            background = roundedGradient(0xFF0B2032.toInt(), 0xFF07121E.toInt(), 0x66436F87, 0x2A32495E, 22)
        }
        val heroLeft = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        heroLeft.addView(label("PIEC C.O.", 8.6f, C.cyan, true))
        heroTemp = label("--.- °C", 46f, Color.WHITE, true).apply { includeFontPadding=false; isSingleLine=true }
        heroLeft.addView(heroTemp, lp(-1, -2, 2))
        heroLeft.addView(label("temperatura główna instalacji", 8.6f, C.textDim, false))

        val heroMini = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, dp(10), 0, 0)
        }
        fun miniStatus(labelText:String): Pair<LinearLayout,TextView> {
            val card=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(8),dp(6),dp(9),dp(6));background=rounded(0x1CFFFFFF, 11, 0x2E5D7890)}
            card.addView(label(labelText, 7.0f, C.textDim2, true))
            val v=label("—", 10.0f, Color.WHITE, true, dp(1)); card.addView(v); return card to v
        }
        val m1=miniStatus("TRYB"); heroMode=m1.second
        val m2=miniStatus("POMPA"); heroPump=m2.second
        val m3=miniStatus("BEZPIECZEŃSTWO"); heroAlarm=m3.second
        heroMini.addView(m1.first, LinearLayout.LayoutParams(0, dp(45), 1f))
        heroMini.addView(m2.first, LinearLayout.LayoutParams(0, dp(45), 1f).apply { marginStart = dp(5) })
        heroMini.addView(m3.first, LinearLayout.LayoutParams(0, dp(45), 1f).apply { marginStart = dp(5) })
        heroLeft.addView(heroMini)
        hero.addView(heroLeft, LinearLayout.LayoutParams(0, -2, 1f))
        heroBoiler = BoilerHeroView(this)
        hero.addView(heroBoiler, lpS(122, 122, 10))
        box.addView(hero, lp(-1, -2, 10))

        // --- Quick controls: action cards, not giant buttons ---
        val quick=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        fun quickCard(titleText:String, icon:NativeIconView.Icon):Pair<View,TextView>{
            val card=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(9),dp(8),dp(8),dp(8));background=pressableBackground(C.surface, C.border, 16);isClickable=true;isFocusable=true}
            card.addView(NativeIconView(this@MainActivity).apply{this.icon=icon;tint=C.cyan;active=true;background=rounded(0x1A2A5062, 12, 0x385B92AB)}, lp(36, 36))
            val cp=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.VERTICAL}
            cp.addView(label(titleText, 8.0f, C.textDim2, true))
            val v=label("—", 9.5f, Color.WHITE, true, dp(1)); v.tag="quickStatus"; cp.addView(v)
            card.addView(cp, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(7) })
            return card to v
        }
        val q1=quickCard("POMPA", NativeIconView.Icon.PUMP); val q2=quickCard("SERWO", NativeIconView.Icon.SERVO); val q3=quickCard("MIESZADŁO", NativeIconView.Icon.MIXER)
        quickPumpView=q1.first; quickServoView=q2.first; quickMixerView=q3.first
        q1.first.setOnClickListener{showPumpMenu()}; q2.first.setOnClickListener{showServoMenu()}; q3.first.setOnClickListener{showMixerMenu()}
        quick.addView(q1.first, LinearLayout.LayoutParams(0, dp(56), 1f))
        quick.addView(q2.first, LinearLayout.LayoutParams(0, dp(56), 1f).apply { marginStart = dp(5) })
        quick.addView(q3.first, LinearLayout.LayoutParams(0, dp(56), 1f).apply { marginStart = dp(5) })
        box.addView(quick, lp(-1, 56, 0))

        sectionTitle(box,"POMIARY")
        addTileGrid(box,listOf(
            TileId.OUTSIDE to "Zewnętrzna",TileId.HEATING to "Piec C.O.",TileId.BOILER to "Bojler",TileId.PANEL to "Panel słon.",
            TileId.ROOM to "Pomieszczenie",TileId.PRESSURE to "Ciśnienie",TileId.HUMIDITY to "Wilgotność"
        ))

        sectionTitle(box,"STEROWANIE · BEZPIECZEŃSTWO")
        addTileGrid(box,listOf(TileId.PUMP to "Pompa",TileId.SERVO to "Serwo",TileId.MIXER to "Mieszadło",TileId.SMOKE to "Czujnik dymu"))

        sectionTitle(box,"ANALIZA I SYSTEM")
        val analysis=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        val chartCard=analysisCard("WYKRESY","historia telemetryczna",C.cyan).apply{setOnClickListener{navigate(1)}}
        val clockCard=analysisCard("DATA I CZAS","synchronizacja telefonu",0xFFB7C4D4.toInt()).apply{setOnClickListener{showClockMenu()}}
        analysis.addView(chartCard,LinearLayout.LayoutParams(0,dp(78),1f))
        analysis.addView(clockCard,LinearLayout.LayoutParams(0,dp(78),1f).apply{marginStart=dp(6)})
        box.addView(analysis,lp(-1,78,0))

        // Compact trend footer – uses the same real data as the HERO.
        val trend=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(13),dp(11),dp(13),dp(9));background=rounded(0x16152234,16,C.border)}
        val th=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val tc=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        tc.addView(label("TREND OSTATNICH POMIARÓW", 8.2f, C.textDim2, true))
        tc.addView(label("szybki podgląd • źródło: telemetria", 7.5f, C.textDim, false, dp(1)))
        th.addView(tc,LinearLayout.LayoutParams(0,-2,1f))
        th.addView(label("LIVE", 8.0f, C.live, true))
        trend.addView(th)
        heroOverview=DashboardOverviewView(this)
        trend.addView(heroOverview,lp(-1,80,6))
        box.addView(trend,lp(-1,-2,10))

        scroll.addView(box)
        return scroll
    }

    private fun premiumStatusCell(name: String, value: String): Pair<View, TextView> {
        val cell = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(10), dp(8), dp(8), dp(7))
            background = rounded(0x26152538, 14, 0x354F7186)
        }
        cell.addView(label(name, 7.8f, C.textDim2, true))
        val v = label(value, 13.5f, Color.WHITE, true, dp(2)).apply { maxLines = 2; ellipsize = null; includeFontPadding = false }
        cell.addView(v)
        return cell to v
    }

    private fun quickAction(titleText: String, desc: String): View {
        val c = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(9), dp(8), dp(8), dp(8))
            background = pressableBackground(C.surface2, C.border, 16)
            isClickable = true; isFocusable = true
        }
        val iconView = NativeIconView(this).apply {
            this.icon = when (titleText) { "POMPA" -> NativeIconView.Icon.PUMP; "SERWO" -> NativeIconView.Icon.SERVO; else -> NativeIconView.Icon.MIXER }
            tint = C.cyan; active = true
            background = rounded(0x1A2C5268, 14, 0x5539BFD8)
        }
        c.addView(iconView, lp(44, 44))
        val copy = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        copy.addView(label(titleText, 9.8f, Color.WHITE, true))
        copy.addView(label(desc, 8.3f, C.textDim2, false, dp(2)).apply { tag = "quickStatus" })
        c.addView(copy, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(8) })
        c.addView(NativeIconView(this).apply { this.icon = NativeIconView.Icon.NEXT; tint = C.textDim2; active = false }, lp(26, 26))
        return c
    }

    private fun analysisCard(titleText: String, desc: String, color: Int): View {
        val c = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(11), dp(10), dp(10), dp(10))
            background = rounded(C.surface, 18, C.border)
            isClickable = true; isFocusable = true
        }
        val iconView = NativeIconView(this).apply {
            this.icon = if (titleText == "WYKRESY") NativeIconView.Icon.CHART else NativeIconView.Icon.CLOCK
            tint = color; active = false; background = rounded(Color.argb(36, Color.red(color), Color.green(color), Color.blue(color)), 14, Color.argb(100, Color.red(color), Color.green(color), Color.blue(color)))
        }
        c.addView(iconView, lp(42, 42))
        val t = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        t.addView(label(titleText, 10.0f, Color.WHITE, true))
        t.addView(label(desc, 8.4f, C.textDim2, false, dp(2)))
        c.addView(t, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(8) })
        c.addView(NativeIconView(this).apply { this.icon = NativeIconView.Icon.NEXT; tint = color; active = false }, lp(24, 24))
        return c
    }

    private fun addTileGrid(box: LinearLayout, defs: List<Pair<TileId, String>>) {
        val widthDp = resources.displayMetrics.widthPixels / resources.displayMetrics.density
        val columns = if (widthDp >= 720f) 4 else if (widthDp >= 520f) 3 else 2
        val tileHeight = if (columns >= 4) 108 else if (columns == 3) 116 else 122
        val grid = GridLayout(this).apply {
            columnCount = columns
            useDefaultMargins = false
            alignmentMode = GridLayout.ALIGN_MARGINS
        }
        defs.forEach { (id, name) ->
            val tile = DashboardTileView(this)
            tile.contentDescription = name
            tile.bind(kindFor(id), name, "—", "LIVE", accentFor(id))
            tile.setOnClickListener { tile.pulse(); onTileClicked(id) }
            grid.addView(tile, GridLayout.LayoutParams().apply {
                width = 0
                height = dp(tileHeight)
                columnSpec = GridLayout.spec(GridLayout.UNDEFINED, 1f)
                setMargins(dp(4), dp(4), dp(4), dp(4))
            })
            tileViews[id] = tile
        }
        box.addView(grid, lp(-1, -2, 1))
    }

    private fun sectionTitle(parent: LinearLayout, text: String) {
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(14), 0, dp(8)) }
        row.addView(View(this).apply { background = rounded(C.cyan, 4, 0) }, lp(3, 20))
        row.addView(label(text, 9.4f, C.textDim, true).apply { gravity = Gravity.CENTER_VERTICAL }, LinearLayout.LayoutParams(-2, dp(20)).apply { marginStart = dp(9) })
        row.addView(View(this).apply { background = rounded(0x263E5A70, 3, 0) }, LinearLayout.LayoutParams(0, dp(1), 1f).apply { marginStart = dp(11) })
        parent.addView(row)
    }

    private var chartFocus = 0
    private var activeRangeSeconds = 24L * 3600L
    private lateinit var chartWorkspace: FrameLayout
    private lateinit var chartFocusTemp: TextView
    private lateinit var chartFocusServo: TextView
    private lateinit var chartRange6: TextView
    private lateinit var chartRange24: TextView
    private lateinit var chartRange7: TextView
    private lateinit var chartRange30: TextView
    private lateinit var chartSeriesHost: LinearLayout
    private lateinit var chartCurrentTitle: TextView
    private lateinit var chartCurrentValue: TextView
    private lateinit var chartCurrentMeta: TextView
    private lateinit var chartNowStat: TextView
    private lateinit var chartRangeStat: TextView
    private lateinit var chartPointsStat: TextView
    private lateinit var chartZoomStat: TextView

    private val chartHeight: Int
        get() {
            val hDp = resources.displayMetrics.heightPixels / resources.displayMetrics.density
            return dp(when { hDp < 700f -> 310f; hDp < 840f -> 348f; else -> 382f }).toInt()
        }

    private fun buildCharts(): ScrollView {
        val scroll=ScrollView(this).apply{
            overScrollMode=View.OVER_SCROLL_IF_CONTENT_SCROLLS
            clipToPadding=false
            setPadding(0,0,0,dp(96))
        }
        val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(14),dp(8),dp(14),dp(18))}

        // Header = one line of identity + two real actions.
        val header=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        header.addView(NativeIconView(this).apply{icon=NativeIconView.Icon.CHART;tint=C.cyan;active=true;background=rounded(0x1621B9D6, 13, 0x553CCCE8)}, lp(38, 38))
        val hc=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        hc.addView(label("WYKRESY", 19f, Color.WHITE, true))
        hc.addView(label("telemetria • analiza • historia", 8.6f, C.textDim2, false, dp(1)))
        header.addView(hc, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(9) })
        header.addView(iconButton(NativeIconView.Icon.REFRESH, "Odśwież historię") { loadHistory(activeRangeSeconds) }, lp(42, 40))
        box.addView(header, lp(-1, 42, 0))

        // Status rail, styled like instrumentation rather than a generic card.
        val status=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(10),dp(7),dp(10),dp(7));background=rounded(0x14111F35,12,C.border)}
        status.addView(NativeIconView(this).apply{icon=NativeIconView.Icon.CLOCK;tint=C.textDim2},lp(20,20))
        historyInfo=label("Ładowanie historii…", 8.4f, C.textDim, false).apply { isSingleLine = true; ellipsize = TextUtils.TruncateAt.END }
        status.addView(historyInfo, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(7) })
        status.addView(pill("REAL", C.liveBg, C.live, 0).apply { tag = "chartReality" }, lpS(54, 28, 6))
        box.addView(status, lp(-1, -2, 7))

        // Focus + range: one control rail, not four competing panels.
        val control=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(9),dp(8),dp(9),dp(8));background=rounded(C.surface,16,C.border)}
        val focusRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        focusRow.addView(label("DANE", 7.4f, C.textDim2, true), lp(36, -2))
        chartFocusTemp=chartChip("TEMPERATURY",true)
        chartFocusServo=chartChip("SERWA",false)
        focusRow.addView(chartFocusTemp,LinearLayout.LayoutParams(0,dp(35),1f).apply{marginStart=dp(4)})
        focusRow.addView(chartFocusServo,LinearLayout.LayoutParams(0,dp(35),1f).apply{marginStart=dp(4)})
        control.addView(focusRow)
        val rangeRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(0,dp(8),0,0)}
        rangeRow.addView(label("ZAKRES", 7.4f, C.textDim2, true), lp(44, -2))
        chartRange6=chartChip("6 H",false);chartRange24=chartChip("24 H",true);chartRange7=chartChip("7 DNI",false);chartRange30=chartChip("30 DNI",false)
        listOf(chartRange6,chartRange24,chartRange7,chartRange30).forEachIndexed{i,v->rangeRow.addView(v,LinearLayout.LayoutParams(0,dp(35),1f).apply{if(i>0)marginStart=dp(4)})}
        control.addView(rangeRow)
        box.addView(control,lp(-1,-2,7))

        val workspace=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(12),dp(12),dp(12),dp(12));background=roundedGradient(0xFF091724.toInt(),0xFF06101A.toInt(),0x655C8097,0x24334756,20)}
        val workHead=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val titleBox=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        chartCurrentTitle=label("TEMPERATURY", 18f, Color.WHITE, true)
        chartCurrentMeta=label("°C · WSPÓLNA OŚ",8.0f,C.textDim2,true,dp(1))
        titleBox.addView(chartCurrentTitle);titleBox.addView(chartCurrentMeta)
        workHead.addView(titleBox,LinearLayout.LayoutParams(0,-2,1f))
        chartNowStat=label("—",9.6f,C.cyan,true).apply{gravity=Gravity.CENTER;background=rounded(0x1A2FD5F2,11,0x6038CDEB);setPadding(dp(8),0,dp(8),0)}
        workHead.addView(chartNowStat,lp(92,30))
        workHead.addView(iconButton(NativeIconView.Icon.RESET,"Resetuj widok"){activeChart().resetViewport(true);refreshChartChrome()},lpS(38,34,6))
        workHead.addView(iconButton(NativeIconView.Icon.EXPAND,"Pełny ekran wykresu"){showFullScreenChart(activeChart(),chartFocus==0)},lpS(38,34,6))
        workspace.addView(workHead)

        chartSeriesHost=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val seriesScroll=HorizontalScrollView(this).apply{isHorizontalScrollBarEnabled=false;overScrollMode=View.OVER_SCROLL_NEVER}
        seriesScroll.addView(chartSeriesHost,FrameLayout.LayoutParams(-2,-1))
        workspace.addView(seriesScroll,LinearLayout.LayoutParams(-1,dp(32)).apply{topMargin=dp(8);bottomMargin=dp(6)})

        chartWorkspace=FrameLayout(this)
        tempChart=ProfessionalTelemetryChartView(this).apply{
            setCatalog(tempSeries());setMode(ProfessionalTelemetryChartView.Mode.COMMON);setLineStyle(ProfessionalTelemetryChartView.LineStyle.SMOOTH);setRealOnly(true);setAnomalyFilter(true)
            onSelectionChanged={r->runOnUiThread{if(::chartCurrentValue.isInitialized)chartCurrentValue.text=r}}
        }
        servoChart=ProfessionalTelemetryChartView(this).apply{
            setCatalog(servoSeries());setMode(ProfessionalTelemetryChartView.Mode.COMMON);setLineStyle(ProfessionalTelemetryChartView.LineStyle.STEPPED);setRealOnly(true);setAnomalyFilter(false);setFixedRange(0.0,100.0)
            onSelectionChanged={r->runOnUiThread{if(::chartCurrentValue.isInitialized)chartCurrentValue.text=r}}
        }
        chartWorkspace.addView(tempChart,FrameLayout.LayoutParams(-1,chartHeight))
        chartWorkspace.addView(servoChart,FrameLayout.LayoutParams(-1,chartHeight))
        servoChart.visibility=View.GONE
        workspace.addView(chartWorkspace,lp(-1,chartHeight,0))

        // Marker inspector: a real inspection surface, not a second chart header.
        val inspector=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(10),dp(8),dp(10),dp(8));background=rounded(0x18132232,12,C.border)}
        inspector.addView(label("INSPEKTOR", 7.3f, C.cyan, true), lp(58, -2))
        chartCurrentValue=label("Dotknij przebiegu", 8.6f, C.text, false).apply{isSingleLine=true;ellipsize=TextUtils.TruncateAt.END}
        inspector.addView(chartCurrentValue,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(7)})
        workspace.addView(inspector,lp(-1,-2,6))

        // KPI rail. The big labels stay factual and quiet.
        val kpi=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        chartRangeStat=label("—", 9.4f, Color.WHITE, true)
        chartPointsStat=label("0", 9.4f, Color.WHITE, true)
        kpi.addView(chartStatCell("ZAKRES", chartRangeStat, C.accent), LinearLayout.LayoutParams(0, dp(48), 1f))
        kpi.addView(chartStatCell("PUNKTY", chartPointsStat, C.live), LinearLayout.LayoutParams(0, dp(48), 1f).apply { marginStart = dp(5) })
        chartZoomStat=label("1×", 9.4f, Color.WHITE, true)
        kpi.addView(chartStatCell("ZOOM", chartZoomStat, C.cyan), LinearLayout.LayoutParams(0, dp(48), 1f).apply { marginStart = dp(5) })
        workspace.addView(kpi, lp(-1, 48, 7))

        // One action rail with four strong destinations.
        val actionRail=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        fun primaryChartAction(title:String,desc:String,icon:NativeIconView.Icon,accent:Int,on:()->Unit):View{
            val v=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.VERTICAL;gravity=Gravity.CENTER;setPadding(dp(4),dp(4),dp(4),dp(4));background=pressableBackground(0xFF0D2133.toInt(),0x3B547086,14);isClickable=true;isFocusable=true}
            v.addView(NativeIconView(this@MainActivity).apply{this.icon=icon;tint=accent;active=true},lp(24,24))
            v.addView(label(title, 7.9f, Color.WHITE, true))
            v.addView(label(desc, 6.6f, C.textDim2, false, dp(1)))
            v.setOnClickListener{on()};return v
        }
        val actSeries=primaryChartAction("SERIE","wybór",NativeIconView.Icon.CHART,C.cyan){showSeriesSheet(activeChart(),chartFocus==0)}
        val actAxis=primaryChartAction("OŚ","skala",NativeIconView.Icon.THERMOMETER,C.accent){showModeSheet(activeChart(),chartFocus==0)}
        val actFilter=primaryChartAction("FILTRY","q · anom.",NativeIconView.Icon.SHIELD,C.live){showChartToolsSheet(activeChart(),chartFocus==0)}
        val actAnalysis=primaryChartAction("ANALIZA","A−B · cykle",NativeIconView.Icon.SETTINGS,0xFFB28CFF.toInt()){showChartAnalysisSheet(activeChart(),chartFocus==0)}
        listOf(actSeries,actAxis,actFilter,actAnalysis).forEachIndexed{i,v->actionRail.addView(v,LinearLayout.LayoutParams(0,dp(60),1f).apply{if(i>0)marginStart=dp(5)})}
        workspace.addView(actionRail,lp(-1,60,0))
        box.addView(workspace,lp(-1,-2,8))

        val hint=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        hint.addView(NativeIconView(this).apply{icon=NativeIconView.Icon.INFO;tint=C.textDim2},lp(15,15))
        hint.addView(label("marker = odczyt punktu • szczypanie = zoom • podwójny tap = reset • GAP = przerwa w danych", 7.3f, C.textDim2, false, dp(1)), LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(5) })
        box.addView(hint, lp(-1, -2, 0))

        chartFocusTemp.setOnClickListener{switchChartFocus(0)};chartFocusServo.setOnClickListener{switchChartFocus(1)}
        chartRange6.setOnClickListener{selectChartRange(chartRange6,6*3600L,chartRange24,chartRange7,chartRange30)}
        chartRange24.setOnClickListener{selectChartRange(chartRange24,24*3600L,chartRange6,chartRange7,chartRange30)}
        chartRange7.setOnClickListener{selectChartRange(chartRange7,7*24*3600L,chartRange6,chartRange24,chartRange30)}
        chartRange30.setOnClickListener{selectChartRange(chartRange30,30*24*3600L,chartRange6,chartRange24,chartRange7)}
        refreshChartChrome()
        scroll.addView(box)
        return scroll
    }

    private fun chartStatCell(titleText: String, valueView: TextView, accent: Int): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(dp(10), dp(7), dp(9), dp(6))
        background = rounded(Color.argb(24, Color.red(accent), Color.green(accent), Color.blue(accent)), 13, Color.argb(72, Color.red(accent), Color.green(accent), Color.blue(accent)))
        addView(label(titleText, 7.4f, C.textDim2, true))
        addView(valueView, lp(-1, -2, 2))
    }

    private fun selectorRow()=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(3),dp(3),dp(3),dp(3));background=rounded(C.surface,16,C.border)}

    private fun chartChip(text: String, on: Boolean): TextView = label(text, 9.6f, if(on) C.cyan else C.textDim, true).apply {
        gravity = Gravity.CENTER
        isSingleLine = true
        isSelected = on
        background = if (on) rounded(0x2738CBE6, 12, 0xB04CD5EA.toInt()) else rounded(0xFF0F1F31.toInt(), 12, C.border)
        setPadding(dp(10), 0, dp(10), 0)
    }

    private fun chartAction(title: String, desc: String): View = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        setPadding(dp(8), 0, dp(8), 0)
        background = pressableBackground(0xFF0D2133.toInt(), 0x3A547086, 13)
        isClickable = true; isFocusable = true
        val icon = when(title) { "SERIE" -> NativeIconView.Icon.CHART; "OŚ" -> NativeIconView.Icon.THERMOMETER; "FILTRY" -> NativeIconView.Icon.SHIELD; else -> NativeIconView.Icon.SETTINGS }
        addView(NativeIconView(this@MainActivity).apply { this.icon = icon; tint = if(title == "SERIE") C.cyan else C.textDim2 }, lp(23, 23))
        val copy = LinearLayout(this@MainActivity).apply { orientation = LinearLayout.VERTICAL }
        copy.addView(label(title, 8.6f, Color.WHITE, true))
        copy.addView(label(desc, 7.0f, if(title == "SERIE") C.cyan else C.textDim2, false, dp(2)))
        addView(copy, LinearLayout.LayoutParams(0, -2, 1f).apply { marginStart = dp(6) })
    }

    private fun activeChart(): ProfessionalTelemetryChartView = if (chartFocus == 0) tempChart else servoChart

    private fun switchChartFocus(which: Int) {
        chartFocus = which.coerceIn(0,1)
        updateSelector(chartFocusTemp, chartFocus == 0)
        updateSelector(chartFocusServo, chartFocus == 1)
        tempChart.visibility = if (chartFocus == 0) View.VISIBLE else View.GONE
        servoChart.visibility = if (chartFocus == 1) View.VISIBLE else View.GONE
        chartCurrentTitle.text = if (chartFocus == 0) "TEMPERATURY" else "POZYCJE SERW"
        chartCurrentMeta.text = if (chartFocus == 0) "Temperatury instalacji · °C" else "Klapa + syberek · % rzeczywiste"
        refreshChartChrome()
    }

    private fun selectChartRange(selected: TextView, seconds: Long, vararg others: TextView) {
        activeRangeSeconds = seconds
        updateSelector(selected, true)
        others.forEach { updateSelector(it, false) }
        loadHistory(seconds)
    }

    private fun refreshChartChrome() {
        if (::chartSeriesHost.isInitialized) {
            chartSeriesHost.removeAllViews()
            val chart = activeChart()
            val catalog = if (chartFocus == 0) tempSeries() else servoSeries()
            catalog.forEach { s ->
                val on = chart.isSeriesEnabled(s.id)
                val chip = LinearLayout(this).apply {
                    orientation = LinearLayout.HORIZONTAL
                    gravity = Gravity.CENTER_VERTICAL
                    setPadding(dp(9), 0, dp(10), 0)
                    contentDescription = if (on) "Ukryj ${s.label}" else "Pokaż ${s.label}"
                    background = if (on) {
                        rounded(Color.argb(42, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent)), 11, Color.argb(165, Color.red(s.accent), Color.green(s.accent), Color.blue(s.accent)))
                    } else {
                        rounded(0xFF0B1928.toInt(), 11, 0x444B6275)
                    }
                    val dot = View(this@MainActivity).apply { background = rounded(s.accent, 4, 0) }
                    addView(dot, lp(7, 7))
                    val txt = label(s.label, 8.7f, if (on) s.accent else C.textDim2, true).apply { alpha = if (on) 1f else .72f }
                    addView(txt, LinearLayout.LayoutParams(-2, -2).apply { marginStart = dp(6) })
                    setOnClickListener {
                        chart.setSeriesEnabled(s.id, !on)
                        refreshChartChrome()
                    }
                }
                chartSeriesHost.addView(chip, LinearLayout.LayoutParams(-2, dp(30)).apply { setMargins(0,0,dp(5),0) })
            }
            val more = action("USTAWIENIA SERII", C.surface2, C.textDim, 0).apply {
                textSize = 7.9f
                setOnClickListener { showSeriesSheet(chart, chartFocus == 0) }
            }
            chartSeriesHost.addView(more, LinearLayout.LayoutParams(-2, dp(30)))
        }
        if (::chartCurrentValue.isInitialized) {
            chartCurrentValue.text = chartReadout(activeChart(), "Brak danych")
        }
        if (::chartNowStat.isInitialized) {
            val r = activeChart().readouts().firstOrNull() ?: "—"
            chartNowStat.text = r.take(24)
            val summary = activeChart().summaryStats()
            chartRangeStat.text = summary.rangeText
            chartPointsStat.text = summary.pointsText
        }
        if (::chartZoomStat.isInitialized) {
            chartZoomStat.text = if (activeChart().currentZoom() < 1.02f) "1×" else String.format(Locale.US, "%.1f×", activeChart().currentZoom())
        }
    }

    private fun buildSettings(): ScrollView {
        val s = ScrollView(this)
        val b = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(15), dp(12), dp(15), dp(100)) }
        b.addView(label("USTAWIENIA", 26f, Color.WHITE, true))
        b.addView(label("Sterowanie natywne · komendy /piec/cmd · potwierdzenie ACK", 11.0f, C.textDim, false, dp(3)))
        settingCard(b, "Pompa", "Tryb AUTO / WŁ. / WYŁ.") { showPumpMenu() }
        settingCard(b, "Serwo", "Tryb + Klapa + Syberek") { showServoMenu() }
        settingCard(b, "Mieszadło", "AUTO / WŁ. / WYŁ.") { showMixerMenu() }
        settingCard(b, "Alarmy", "Próg · wyciszenie · reset") { showSmokeMenu() }
        settingCard(b, "Czujniki", "Symulacja wartości i czasu") { showSensorMenu("Zewnętrzna", "zewn", "t_zewn", "°C", -30f, 50f, .5f) }
        s.addView(b)
        return s
    }

    // ── WEATHER TAB ──────────────────────────────────────────────────────
    private var weatherHeroAnim: WeatherView? = null
    private var weatherLastFetchMs = 0L
    private var weatherData: WeatherService.WeatherData? = null
    private var weatherDays = 7

    private fun buildWeather(): ScrollView {
        val s = ScrollView(this)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(15), dp(12), dp(15), dp(100))
        }

        // ── Header
        root.addView(label("POGODA", 26f, Color.WHITE, true))
        val locLine = label("${String.format(Locale.US, "%.4f", WeatherService.DEFAULT_LAT)}° N · ${String.format(Locale.US, "%.4f", WeatherService.DEFAULT_LON)}° E · Open-Meteo", 9.5f, C.textDim, false, dp(3))
        root.addView(locLine)

        // ── Hero card — animated weather + current temperature
        val heroCard = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(18), dp(18), dp(18), dp(18))
            background = roundedGradient(C.hero, 0xFF0B2032.toInt(), 0x401A3A5C, 0x22264A6E, 20)
        }
        val heroLeft = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val heroTempLabel = label("--°C", 38f, Color.WHITE, true)
        heroTempLabel.tag = "weather_hero_temp"
        heroLeft.addView(heroTempLabel)
        val heroDescLabel = label("Pobieram…", 13f, C.textDim, false, dp(4))
        heroDescLabel.tag = "weather_hero_desc"
        heroLeft.addView(heroDescLabel)
        val heroFeelsLabel = label("", 10.5f, C.textDim2, false, dp(3))
        heroFeelsLabel.tag = "weather_hero_feels"
        heroLeft.addView(heroFeelsLabel)
        heroCard.addView(heroLeft, LinearLayout.LayoutParams(0, -2, 1f))

        weatherHeroAnim = WeatherView(this).apply {
            background = rounded(0x1A00D4F5, 16, 0x2036BBD5)
        }
        heroCard.addView(weatherHeroAnim!!, lp(110, 110))
        root.addView(heroCard, lp(-1, -2, 8))

        // ── Metric cards grid — 2 columns
        val metricGrid = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
        }
        val metricCol1 = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val metricCol2 = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(8), 0, 0, 0) }

        // Row 1: Wilgotność + Wiatr
        val humidCard = weatherMetricCard("WILGOTNOŚĆ", "--%", "💧", C.blue)
        humidCard.tag = "weather_humid"
        metricCol1.addView(humidCard, lp(-1, -2, 8))

        val windCard = weatherMetricCard("WIATR", "-- km/h", "💨", C.cyan)
        windCard.tag = "weather_wind"
        metricCol2.addView(windCard, lp(-1, -2, 8))

        // Row 2: Ciśnienie + Zachmurzenie
        val pressCard = weatherMetricCard("CIŚNIENIE", "-- hPa", "🌡️", C.violet)
        pressCard.tag = "weather_press"
        metricCol1.addView(pressCard, lp(-1, -2, 8))

        val cloudCard = weatherMetricCard("ZACHMURZENIE", "--%", "☁️", C.textDim)
        cloudCard.tag = "weather_cloud"
        metricCol2.addView(cloudCard, lp(-1, -2, 8))

        // Row 3: Opad + UV
        val precipCard = weatherMetricCard("OPAD", "-- mm", "🌧️", C.blue)
        precipCard.tag = "weather_precip"
        metricCol1.addView(precipCard, lp(-1, -2, 8))

        val uvCard = weatherMetricCard("UV INDEX", "--", "☀️", C.yellow)
        uvCard.tag = "weather_uv"
        metricCol2.addView(uvCard, lp(-1, -2, 8))

        metricGrid.addView(metricCol1, LinearLayout.LayoutParams(0, -2, 1f))
        metricGrid.addView(metricCol2, LinearLayout.LayoutParams(0, -2, 1f))
        root.addView(metricGrid, lp(-1, -2, 4))

        // ── Range selector (1D, 2D, 3D, 7D, 14D)
        val rangeRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val ranges = listOf(1 to "1D", 2 to "2D", 3 to "3D", 7 to "7D", 14 to "14D")
        ranges.forEach { (days, lbl) ->
            val btn = action(lbl, if (days == weatherDays) C.cyan else C.surface2, if (days == weatherDays) C.bg else Color.WHITE, 0)
            btn.setOnClickListener {
                weatherDays = days
                refreshWeatherTab(force = true)
            }
            rangeRow.addView(btn, LinearLayout.LayoutParams(0, -1, 1f).apply { marginStart = dp(4) })
        }
        root.addView(rangeRow, lp(-1, 40, 6))

        // ── Weather charts card (forecast graphs)
        val chartsCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            background = rounded(C.surface, 18, C.border)
        }
        chartsCard.addView(label("WYKRESY 24H", 10f, C.cyan, true))

        val tempChart = WeatherChartView(this)
        tempChart.tag = "weather_chart_temp"
        tempChart.setHeight(140)
        chartsCard.addView(tempChart, lp(-1, -2, 6))
        chartsCard.addView(label("— Temperatura (°C)", 7.5f, 0xFF55D7FF.toInt(), false, dp(2)))

        val windChart = WeatherChartView(this)
        windChart.tag = "weather_chart_wind"
        windChart.setHeight(120)
        chartsCard.addView(windChart, lp(-1, -2, 6))
        chartsCard.addView(label("— Wiatr (km/h)", 7.5f, 0xFF00D4F5.toInt(), false, dp(2)))

        val cloudChart = WeatherChartView(this)
        cloudChart.tag = "weather_chart_cloud"
        cloudChart.setHeight(120)
        chartsCard.addView(cloudChart, lp(-1, -2, 6))
        chartsCard.addView(label("— Zachmurzenie (%)", 7.5f, C.textDim, false, dp(2)))

        val precipChart = WeatherChartView(this)
        precipChart.tag = "weather_chart_precip"
        precipChart.setHeight(120)
        chartsCard.addView(precipChart, lp(-1, -2, 6))
        chartsCard.addView(label("— Opad (mm)", 7.5f, 0xFF4ADE80.toInt(), false, dp(2)))

        root.addView(chartsCard, lp(-1, -2, 8))

        // ── Hourly forecast card
        val hourlyCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            background = rounded(C.surface, 18, C.border)
        }
        val hourlyHeader = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        hourlyHeader.addView(label("PROGNOZA GODZINOWA", 10f, C.cyan, true))
        hourlyHeader.addView(View(this).apply {}, LinearLayout.LayoutParams(0, 0, 1f))
        hourlyHeader.addView(label("48h", 9f, C.textDim2, false))
        hourlyCard.addView(hourlyHeader)

        val hourlyScroll = HorizontalScrollView(this).apply {
            overScrollMode = View.OVER_SCROLL_IF_CONTENT_SCROLLS
            setPadding(0, dp(10), 0, 0)
        }
        val hourlyRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            tag = "weather_hourly_row"
        }
        hourlyScroll.addView(hourlyRow)
        hourlyCard.addView(hourlyScroll)
        root.addView(hourlyCard, lp(-1, -2, 10))

        // ── Daily forecast card
        val dailyCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            background = rounded(C.surface, 18, C.border)
        }
        val dailyHeader = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        dailyHeader.addView(label("PROGNOZA 7-DNIOWA", 10f, C.cyan, true))
        dailyHeader.addView(View(this), LinearLayout.LayoutParams(0, 0, 1f))
        dailyCard.addView(dailyHeader)

        val dailyContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            tag = "weather_daily_container"
        }
        dailyCard.addView(dailyContainer, lp(-1, -2, 8))
        root.addView(dailyCard, lp(-1, -2, 4))

        // ── Today summary card
        val summaryCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            background = rounded(C.surface, 18, C.border)
        }
        summaryCard.addView(label("PODSUMOWANIE DNIA", 10f, C.cyan, true))
        val summaryGrid = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            tag = "weather_summary"
        }
        summaryCard.addView(summaryGrid, lp(-1, -2, 6))
        root.addView(summaryCard, lp(-1, -2, 10))

        // ── Refresh button
        val refreshBtn = action("ODŚWIEŻ DANE", C.surface2, Color.WHITE, 0)
        refreshBtn.setOnClickListener { refreshWeatherTab(force = true) }
        root.addView(refreshBtn, lp(-1, 48, 6))

        s.addView(root)
        return s
    }

    private fun weatherMetricCard(title: String, value: String, emoji: String, accentColor: Int): LinearLayout {
        return LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(12))
            background = rounded(C.surface, 16, C.border)
            addView(label("$emoji $title", 9f, C.textDim, true))
            addView(label(value, 16f, Color.WHITE, true, dp(4)))
        }
    }

    private fun refreshWeatherTab(force: Boolean = false) {
        val now = System.currentTimeMillis()
        if (!force && weatherData != null && now - weatherLastFetchMs < 5 * 60 * 1000L) return
        weatherLastFetchMs = now

        WeatherService.fetchWeather(days = weatherDays) { data ->
            weatherData = data
            runOnUiThread { renderWeatherTab(data) }
        }
    }

    private fun renderWeatherTab(data: WeatherService.WeatherData) {
        val cur = data.current ?: return

        // Hero
        val heroTemp = weatherView.findViewWithTag("weather_hero_temp") as? TextView
        val heroDesc = weatherView.findViewWithTag("weather_hero_desc") as? TextView
        val heroFeels = weatherView.findViewWithTag("weather_hero_feels") as? TextView
        heroTemp?.text = if (cur.temperature.isFinite()) String.format(Locale.US, "%.1f°C", cur.temperature) else "--°C"
        heroDesc?.text = WeatherService.getWeatherDescription(cur.weatherCode)
        heroFeels?.text = buildString {
            append("Odczuwalna ")
            append(if (cur.feelsLike.isFinite()) String.format(Locale.US, "%.1f°C", cur.feelsLike) else "--")
            append("  ·  ")
            append(WeatherService.getWindDirection(cur.windDirection))
            append(" ")
            append(if (cur.windSpeed.isFinite()) String.format(Locale.US, "%.0f km/h", cur.windSpeed) else "--")
        }
        weatherHeroAnim?.bind(cur.weatherCode, cur.isDay, cur.temperature)

        // Metrics
        val humid = weatherView.findViewWithTag("weather_humid") as? LinearLayout
        humid?.let { updateMetricCard(it, "${cur.humidity}%") }
        val wind = weatherView.findViewWithTag("weather_wind") as? LinearLayout
        wind?.let { updateMetricCard(it, "${fmtNum(cur.windSpeed)} km/h\nPorywy: ${fmtNum(cur.windGusts)} km/h") }
        val press = weatherView.findViewWithTag("weather_press") as? LinearLayout
        press?.let { updateMetricCard(it, if (cur.pressure.isFinite()) String.format(Locale.US, "%.0f hPa", cur.pressure) else "-- hPa") }
        val cloud = weatherView.findViewWithTag("weather_cloud") as? LinearLayout
        cloud?.let { updateMetricCard(it, if (cur.cloudCover.isFinite()) String.format(Locale.US, "%.0f%%", cur.cloudCover) else "--%") }
        val precip = weatherView.findViewWithTag("weather_precip") as? LinearLayout
        precip?.let { updateMetricCard(it, String.format(Locale.US, "%.1f mm", cur.precipitation)) }
        val uv = weatherView.findViewWithTag("weather_uv") as? LinearLayout
        uv?.let { updateMetricCard(it, String.format(Locale.US, "%.1f", cur.uvIndex)) }

        // Weather charts
        val hourlyFmtChart = java.text.SimpleDateFormat("HH:mm", Locale.US)
        val dailyFmtChart = java.text.SimpleDateFormat("dd.MM", Locale.US)
        val nowMsChart = System.currentTimeMillis()
        val maxHours = weatherDays * 24
        val relevantHourly = data.hourly.filter { it.time >= nowMsChart - 3600000L }.take(maxHours)

        val xLabels = relevantHourly.map {
            if (weatherDays > 2) dailyFmtChart.format(java.util.Date(it.time))
            else hourlyFmtChart.format(java.util.Date(it.time))
        }

        val tempChart = weatherView.findViewWithTag("weather_chart_temp") as? WeatherChartView
        if (tempChart != null && relevantHourly.isNotEmpty()) {
            tempChart.setData(
                relevantHourly.map { if (it.temperature.isFinite()) it.temperature.toFloat() else 0f },
                0xFF55D7FF.toInt(), "°C"
            )
            tempChart.setXLabels(xLabels)
        }

        val windChart = weatherView.findViewWithTag("weather_chart_wind") as? WeatherChartView
        if (windChart != null && relevantHourly.isNotEmpty()) {
            windChart.setData(
                relevantHourly.map { it.windSpeed.toFloat() },
                0xFF00D4F5.toInt(), ""
            )
            windChart.setXLabels(xLabels)
        }

        val cloudChart = weatherView.findViewWithTag("weather_chart_cloud") as? WeatherChartView
        if (cloudChart != null && relevantHourly.isNotEmpty()) {
            cloudChart.setData(
                relevantHourly.map { if (it.cloudCover.isFinite()) it.cloudCover.toFloat() else 0f },
                0xFF8EA6BA.toInt(), "%", 0f, 100f
            )
            cloudChart.setXLabels(xLabels)
        }

        val precipChart = weatherView.findViewWithTag("weather_chart_precip") as? WeatherChartView
        if (precipChart != null && relevantHourly.isNotEmpty()) {
            precipChart.setData(
                relevantHourly.map { it.precipitation.toFloat() },
                0xFF4ADE80.toInt(), "mm", 0f
            )
            precipChart.setXLabels(xLabels)
        }

        // Hourly forecast
        val hourlyRow = weatherView.findViewWithTag("weather_hourly_row") as? LinearLayout
        hourlyRow?.removeAllViews()
        val hourlyFmt = java.text.SimpleDateFormat("HH:mm", Locale.US)
        val nowMs = System.currentTimeMillis()
        val hourlyItems = data.hourly.filter { it.time >= nowMs - 3600000L }.take(24)
        for (h in hourlyItems) {
            val item = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER
                setPadding(dp(10), dp(8), dp(10), dp(8))
                background = rounded(C.surface2, 12, C.border)
                minimumWidth = dp(58)
            }
            val timeStr = if (h.time > 0) hourlyFmt.format(java.util.Date(h.time)) else "--:--"
            item.addView(label(timeStr, 8.5f, C.textDim2, true))
            item.addView(label(WeatherService.getWeatherIcon(h.weatherCode, cur.isDay), 16f, Color.WHITE, false, dp(4)))
            item.addView(label(if (h.temperature.isFinite()) String.format(Locale.US, "%.0f°", h.temperature) else "--", 11f, Color.WHITE, true, dp(3)))
            if (h.precipitationProb > 0) {
                item.addView(label("${h.precipitationProb}%", 8f, C.blue, true, dp(2)))
            }
            hourlyRow?.addView(item, LinearLayout.LayoutParams(-2, -2).apply { setMargins(0, 0, dp(6), 0) })
        }

        // Daily forecast
        val dailyContainer = weatherView.findViewWithTag("weather_daily_container") as? LinearLayout
        dailyContainer?.removeAllViews()
        val dayFmt = java.text.SimpleDateFormat("EEE dd.MM", Locale("pl", "PL"))
        for (d in data.daily) {
            val row = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(10), dp(10), dp(10), dp(10))
                background = rounded(C.surface2, 12, C.border)
            }
            val dateStr = if (d.date > 0) dayFmt.format(java.util.Date(d.date)) else "--"
            row.addView(label(dateStr, 10.5f, C.text, true), lp(0, -2, 1f))
            row.addView(label(WeatherService.getWeatherIcon(d.weatherCode), 14f, Color.WHITE, false))
            // Min-max temp bar
            val tempRange = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER
                setPadding(dp(10), 0, dp(10), 0)
            }
            tempRange.addView(label(if (d.tempMin.isFinite()) String.format(Locale.US, "%.0f°", d.tempMin) else "--", 10f, C.blue, true))
            tempRange.addView(label(" — ", 10f, C.textDim2, false))
            tempRange.addView(label(if (d.tempMax.isFinite()) String.format(Locale.US, "%.0f°", d.tempMax) else "--", 10f, C.err, true))
            row.addView(tempRange)
            if (d.precipitationSum > 0) {
                row.addView(label(String.format(Locale.US, "%.1fmm", d.precipitationSum), 9f, C.blue, true, dp(2)))
            }
            dailyContainer?.addView(row, lp(-1, -2, 5))
        }

        // Today summary
        val summary = weatherView.findViewWithTag("weather_summary") as? LinearLayout
        summary?.removeAllViews()
        if (data.daily.isNotEmpty()) {
            val today = data.daily[0]
            val summaryItems = listOf(
                "🌡️ Max" to if (today.tempMax.isFinite()) String.format(Locale.US, "%.1f°C", today.tempMax) else "--",
                "🌡️ Min" to if (today.tempMin.isFinite()) String.format(Locale.US, "%.1f°C", today.tempMin) else "--",
                "🌧️ Opad" to String.format(Locale.US, "%.1f mm", today.precipitationSum),
                "💨 Wiatr max" to String.format(Locale.US, "%.0f km/h", today.windSpeedMax),
                "💨 Porywy max" to String.format(Locale.US, "%.0f km/h", today.windGustsMax),
                "☀️ UV max" to String.format(Locale.US, "%.1f", today.uvIndexMax),
                "🌅 Wschód" to if (today.sunrise > 0) java.text.SimpleDateFormat("HH:mm", Locale.US).format(java.util.Date(today.sunrise)) else "--",
                "🌇 Zachód" to if (today.sunset > 0) java.text.SimpleDateFormat("HH:mm", Locale.US).format(java.util.Date(today.sunset)) else "--",
                "☀️ Nasłonecznienie" to String.format(Locale.US, "%.0f W/m²", today.shortwaveRadiation)
            )
            val gridRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
            val col1 = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
            val col2 = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(12), 0, 0, 0) }
            summaryItems.forEachIndexed { idx, (k, v) ->
                val item = LinearLayout(this@MainActivity).apply {
                    orientation = LinearLayout.HORIZONTAL
                    gravity = Gravity.CENTER_VERTICAL
                    setPadding(dp(8), dp(6), dp(8), dp(6))
                }
                item.addView(label(k, 9.5f, C.textDim, false))
                item.addView(View(this@MainActivity), LinearLayout.LayoutParams(0, 0, 1f))
                item.addView(label(v, 10f, Color.WHITE, true))
                if (idx % 2 == 0) col1.addView(item) else col2.addView(item)
            }
            gridRow.addView(col1, LinearLayout.LayoutParams(0, -2, 1f))
            gridRow.addView(col2, LinearLayout.LayoutParams(0, -2, 1f))
            summary?.addView(gridRow)
        }
    }

    private fun updateMetricCard(card: LinearLayout, value: String) {
        if (card.childCount >= 2) {
            (card.getChildAt(1) as? TextView)?.text = value
        }
    }

    private fun fmtNum(v: Double): String {
        return if (v.isFinite()) String.format(Locale.US, "%.0f", v) else "--"
    }

    private fun buildMore(): ScrollView {
        val s = ScrollView(this)
        val b = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(15), dp(12), dp(15), dp(100)) }
        b.addView(label("WIĘCEJ", 26f, Color.WHITE, true))
        b.addView(label("Moduły serwisowe i informacje", 11.0f, C.textDim, false, dp(3)))
        listOf(
            "Logi" to "Diagnostyka zdarzeń sterownika",
            "Terminal" to "Narzędzia serwisowe",
            "OTA" to "Aktualizacja firmware",
            "Sesja" to "Stan logowania i bezpieczeństwo"
        ).forEach { (t, d) -> settingCard(b, t, d) { showModule(t) } }
        s.addView(b)
        return s
    }

    private fun settingCard(parent: LinearLayout, title: String, desc: String, on: () -> Unit) {
        val c = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(14), dp(13), dp(11), dp(13))
            background = pressableBackground(C.surface, C.border, 18)
            isClickable = true
            isFocusable = true
            setOnClickListener { on() }
        }
        val iconType = when (title) {
            "Pompa" -> NativeIconView.Icon.PUMP
            "Serwo" -> NativeIconView.Icon.SERVO
            "Mieszadło" -> NativeIconView.Icon.MIXER
            "Alarmy" -> NativeIconView.Icon.SHIELD
            "Czujniki" -> NativeIconView.Icon.THERMOMETER
            "Pogoda" -> NativeIconView.Icon.WEATHER
            "Logi" -> NativeIconView.Icon.LOGS
            "Terminal" -> NativeIconView.Icon.TERMINAL
            "OTA" -> NativeIconView.Icon.UPLOAD
            else -> NativeIconView.Icon.SETTINGS
        }
        val icon = NativeIconView(this).apply {
            this.icon = iconType; tint = C.cyan; active = false
            background = rounded(0x1626B9D6, 15, 0x4836BBD5)
        }
        c.addView(icon, lp(44, 44))
        val texts=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        texts.addView(label(title, 14.5f, Color.WHITE, true))
        texts.addView(label(desc, 10.4f, C.textDim, false, dp(3)))
        c.addView(texts, LinearLayout.LayoutParams(0, -2, 1f).apply { setMargins(dp(12), 0, 0, 0) })
        c.addView(NativeIconView(this).apply { this.icon=NativeIconView.Icon.NEXT; tint=C.textDim2; active=false }, lp(26, 26))
        parent.addView(c, lp(-1, -2, 8))
    }

    private fun navigate(index: Int) {
        dashboardView.visibility = if (index == 0) View.VISIBLE else View.GONE
        chartsView.visibility = if (index == 1) View.VISIBLE else View.GONE
        weatherView.visibility = if (index == 2) View.VISIBLE else View.GONE
        settingsView.visibility = if (index == 3) View.VISIBLE else View.GONE
        moreView.visibility = if (index == 4) View.VISIBLE else View.GONE
        navButtons.forEachIndexed { i, b ->
            val on = i == index
            b.isSelected = on
            b.background = navBg(on)
            (b.tag as? TextView)?.setTextColor(if (on) C.cyan else C.textDim2)
            (b.getChildAt(0) as? NativeIconView)?.apply { tint = if (on) C.cyan else C.textDim2; active = on; invalidate() }
        }
        if (index == 1 && currentIdToken != null && tempChart.snapshot().isEmpty()) loadHistory(24 * 3600L)
        if (index == 2) refreshWeatherTab()
    }

    private fun restoreSession() {
        val saved = authStore.load() ?: run { runOnUiThread { showSignedOut() }; return }
        try {
            val now = System.currentTimeMillis()
            if (!saved.idToken.isNullOrBlank() && saved.expiresAtMs > now + TOKEN_REFRESH_SKEW_MS) {
                currentIdToken = saved.idToken
                tokenExpiresAtMs = saved.expiresAtMs
            } else {
                persistSession(saved.email, firebase.refresh(saved.refreshToken))
            }
            sessionNeedsLogin = false
            runOnUiThread {
                subtitle.text = "ONLINE • Firebase • aktualizacja teraz"
                connectionChip.text = "● LIVE"
                connectionChip.setTextColor(C.live)
                hideLogin()
                startStatusPolling()
                startMaintenance()
            }
        } catch (e: FirebaseAuthRest.FirebaseAuthException) {
            if (isTrulyInvalidSession(e)) {
                authStore.clear(); currentIdToken = null; tokenExpiresAtMs = 0; sessionNeedsLogin = true
                runOnUiThread { showSignedOut() }
            } else {
                sessionNeedsLogin = false
                runOnUiThread {
                    subtitle.text = "ONLINE • Firebase • aktualizacja teraz"
                    connectionChip.text = "● OFFLINE"
                    connectionChip.setTextColor(C.warn)
                    hideLogin(); startStatusPolling(); startMaintenance(); loadHistory(activeRangeSeconds)
                }
            }
        } catch (_: Exception) {
            sessionNeedsLogin = false
            runOnUiThread {
                subtitle.text = "ONLINE • Firebase • aktualizacja teraz"
                connectionChip.text = "● OFFLINE"
                connectionChip.setTextColor(C.warn)
                hideLogin(); startStatusPolling(); startMaintenance(); loadHistory(activeRangeSeconds)
            }
        }
    }

    private fun doLogin(email: String, password: String, msg: TextView, button: View) {
        if (email.isBlank() || password.isBlank()) { msg.text = "Podaj e-mail i hasło."; return }
        button.isEnabled = false
        msg.text = "Łączenie z Firebase…"
        executor.execute {
            try {
                val session = firebase.signIn(email, password)
                persistSession(email, session)
                sessionNeedsLogin = false
                runOnUiThread {
                    msg.text = "Zalogowano"
                    button.isEnabled = true
                    subtitle.text = "ONLINE • Firebase • aktualizacja teraz"
                    connectionChip.text = "● LIVE"
                    connectionChip.setTextColor(C.live)
                    hideLogin(); startStatusPolling(); startMaintenance(); loadHistory(activeRangeSeconds)
                }
                runCatching { firebase.getStatus(session.idToken) }.onSuccess { d -> lastStatusData = d; runOnUiThread { renderDashboard(d) } }
            } catch (e: Exception) {
                runOnUiThread { msg.text = "Logowanie nieudane: ${e.message}"; button.isEnabled = true }
            }
        }
    }

    private fun hideLogin() { loginView.visibility = View.GONE; content.visibility = View.VISIBLE }
    private fun showChecking() { loginView.visibility = View.GONE; content.visibility = View.INVISIBLE }
    private fun showSignedOut() { content.visibility = View.INVISIBLE; loginView.visibility = View.VISIBLE }
    private fun wipeLocalData() {
        lastStatusData = null; lastStatusAtMs = 0L; lastStatusOnline = false
        tempChart.setData(emptyList(), false); servoChart.setData(emptyList(), false)
        historyInfo.text = "Ładowanie historii…"
    }
    private fun logout() { authStore.clear(); currentIdToken = null; tokenExpiresAtMs = 0; sessionNeedsLogin = true; wipeLocalData(); showSignedOut() }
    private fun handleInvalidSession() { authStore.clear(); currentIdToken = null; tokenExpiresAtMs = 0; sessionNeedsLogin = true; runOnUiThread { wipeLocalData(); showSignedOut() } }
    private fun isTrulyInvalidSession(e: FirebaseAuthRest.FirebaseAuthException): Boolean = when (e.firebaseCode) {
        "INVALID_REFRESH_TOKEN", "TOKEN_EXPIRED", "USER_DISABLED" -> true
        else -> false
    }

    private fun startStatusPolling() {
        if (pollingStarted) return
        pollingStarted = true
        // Fixed DELAY (not fixed rate): after the process was frozen in the background
        // fixed-rate would fire all missed runs back-to-back.
        scheduler.scheduleWithFixedDelay({ pollStatusOnce() }, 1, 5, TimeUnit.SECONDS)
    }

    private fun pollStatusOnce() {
        if (sessionNeedsLogin || !appVisible) return
        try {
            val token = ensureToken(false) ?: return
            val d = firebase.getStatus(token)
            lastStatusData = d
            lastStatusOnline = true
            // Fix: Cache last status for offline display
            runCatching { LastStatusCache.save(this, d) }
            runOnUiThread { renderDashboard(d) }
        } catch (e: FirebaseAuthRest.FirebaseAuthException) {
            if (isTrulyInvalidSession(e)) handleInvalidSession()
            else {
                lastStatusOnline = false
                // Fix: Show cached data when offline
                runOnUiThread { showOfflineFallback() }
            }
        } catch (_: Exception) {
            lastStatusOnline = false
            // Fix: Show cached data when offline
            runOnUiThread { showOfflineFallback() }
        }
    }

    // Fix: Offline fallback — show last cached status when network fails
    private fun showOfflineFallback() {
        val cached = LastStatusCache.load(this)
        if (cached != null) {
            val (data, ts) = cached
            val ageSec = (System.currentTimeMillis() - ts) / 1000L
            lastStatusData = data
            connectionChip.text = "● OFFLINE (${ageSec}s)"
            connectionChip.setTextColor(C.warn)
            subtitle.text = "OFFLINE • ostatni odczyt ${ageSec}s temu"
            renderDashboard(data)
        } else {
            connectionChip.text = "● OFFLINE"
            connectionChip.setTextColor(C.err)
            subtitle.text = "OFFLINE • brak zapisanych danych"
        }
    }

    private fun startMaintenance() {
        if (maintenanceStarted) return
        maintenanceStarted = true
        scheduler.scheduleWithFixedDelay({ if (appVisible && !sessionNeedsLogin) runCatching { ensureToken(false) } }, 5, 30, TimeUnit.SECONDS)
        scheduler.scheduleWithFixedDelay({
            runOnUiThread {
                if (sessionNeedsLogin || !appVisible) return@runOnUiThread
                // Age is evaluated here, on the UI thread, right when it is drawn. Computing it on the
                // worker thread let delayed runnables mark tiles STALE after a fresh render.
                val age = if (lastStatusAtMs > 0L) (System.currentTimeMillis() - lastStatusAtMs) / 1000L else Long.MAX_VALUE
                when {
                    lastStatusOnline && age <= 8L -> {
                        connectionChip.text = if (age <= 1L) "● LIVE" else "● LIVE ${age}s"
                        connectionChip.setTextColor(C.live)
                        subtitle.text = if (age <= 1L) "ONLINE • Firebase • teraz" else "ONLINE • Firebase • ${age}s temu"
                        tileViews.values.forEach { it.setStale(false) }
                    }
                    lastStatusOnline && age <= 20L -> {
                        connectionChip.text = "● ŚWIEŻE"
                        connectionChip.setTextColor(C.warn)
                        subtitle.text = "ONLINE • Firebase • ${age}s temu"
                    }
                    lastStatusAtMs > 0L -> {
                        connectionChip.text = "● NIEŚWIEŻE"
                        connectionChip.setTextColor(C.warn)
                        subtitle.text = "Firebase • ostatni odczyt ${age}s temu"
                        tileViews.values.forEach { it.setStale(true) }
                    }
                }
            }
        }, 1, 1, TimeUnit.SECONDS)
    }

    private fun ensureToken(force: Boolean): String? {
        val now = System.currentTimeMillis()
        if (!force && !currentIdToken.isNullOrBlank() && tokenExpiresAtMs > now + 120000) return currentIdToken
        if (sessionNeedsLogin) return null
        val saved = authStore.load() ?: return null
        val s = firebase.refresh(saved.refreshToken)
        // The user may have logged out while the refresh request was in flight.
        if (sessionNeedsLogin) return null
        persistSession(saved.email, s)
        return s.idToken
    }

    private fun persistSession(email: String, s: FirebaseAuthRest.Session) {
        authStore.save(email, s.refreshToken, s.idToken, s.expiresAtMs, s.localId)
        currentIdToken = s.idToken
        tokenExpiresAtMs = s.expiresAtMs
    }

    private fun renderDashboard(d: JSONObject) {
        lastStatusAtMs = System.currentTimeMillis()
        connectionChip.text = "● LIVE"
        connectionChip.setTextColor(C.live)

        // Apply advanced filters to remove DS18B20 spikes/anomalies
        val timestamp = d.optLong("ts", System.currentTimeMillis())
        val rawOutside = d.optDouble("t_zewn", Double.NaN).toFloat()
        val rawHeat = d.optDouble("t_ogrz", Double.NaN).toFloat()
        val rawBoiler = d.optDouble("t_bojler", Double.NaN).toFloat()
        val rawPanel = d.optDouble("t_panel", Double.NaN).toFloat()
        val rawRoom = d.optDouble("t_pokoj", Double.NaN).toFloat()
        val rawPressure = d.optDouble("cisnienie", Double.NaN).toFloat()
        val rawHumidity = d.optDouble("wilgotnosc", Double.NaN).toFloat()
        
        // Layer 1: Spike filtering (AdvancedSensorFilter - median + rate limit)
        var filteredOutside = filterOutside.filter(rawOutside, timestamp)
        var filteredHeat = filterHeating.filter(rawHeat, timestamp)
        var filteredBoiler = filterBoiler.filter(rawBoiler, timestamp)
        var filteredPanel = filterPanel.filter(rawPanel, timestamp)
        var filteredRoom = filterRoom.filter(rawRoom, timestamp)
        val pressure = filterPressure.filter(rawPressure, timestamp).toDouble()
        val humidity = filterHumidity.filter(rawHumidity, timestamp).toDouble()
        
        // Log spike rejections (Layer 1 filter already handled DS18B20 errors: -16°C, 85°C, -127°C)
        if (filteredOutside != rawOutside && rawOutside.isFinite()) Log.d("SpikeFilter", "t_zewn: $rawOutside → $filteredOutside (${filterOutside.getLastRejectionReason()})")
        if (filteredHeat != rawHeat && rawHeat.isFinite()) Log.d("SpikeFilter", "t_ogrz: $rawHeat → $filteredHeat (${filterHeating.getLastRejectionReason()})")
        if (filteredBoiler != rawBoiler && rawBoiler.isFinite()) Log.d("SpikeFilter", "t_bojler: $rawBoiler → $filteredBoiler (${filterBoiler.getLastRejectionReason()})")
        if (filteredPanel != rawPanel && rawPanel.isFinite()) Log.d("SpikeFilter", "t_panel: $rawPanel → $filteredPanel (${filterPanel.getLastRejectionReason()})")
        if (filteredRoom != rawRoom && rawRoom.isFinite()) Log.d("SpikeFilter", "t_pokoj: $rawRoom → $filteredRoom (${filterRoom.getLastRejectionReason()})")
        
        // Layer 2: Sensor health monitoring (stagnation warnings — do NOT block values!)
        // Stagnation is OFTEN NORMAL: boiler 6h stable when not heating CWU, room 6h when insulated
        val healthOut = healthOutside.addReading(filteredOutside, timestamp)
        val healthHeat = healthHeating.addReading(filteredHeat, timestamp)
        val healthBoil = healthBoiler.addReading(filteredBoiler, timestamp)
        val healthPan = healthPanel.addReading(filteredPanel, timestamp)
        val healthRm = healthRoom.addReading(filteredRoom, timestamp)
        val healthHum = healthHumidity.addReading(rawHumidity, timestamp)
        val healthPres = healthPressure.addReading(pressure.toFloat(), timestamp)
        if (healthOut.isStagnant) Log.w("Health", healthOut.statusText)
        if (healthHeat.isStagnant) Log.w("Health", healthHeat.statusText)
        if (healthBoil.isStagnant) Log.w("Health", healthBoil.statusText)
        if (healthPan.isStagnant) Log.w("Health", healthPan.statusText)
        if (healthRm.isStagnant) Log.w("Health", healthRm.statusText)
        if (!healthHum.isOnline) Log.w("Health", "wilgotnosc: ${healthHum.statusText}")
        
        val outside = filteredOutside.toDouble()
        val heat = filteredHeat.toDouble()
        val boiler = filteredBoiler.toDouble()
        val panel = filteredPanel.toDouble()
        val room = filteredRoom.toDouble()
        val pump = d.optBoolean("pompa", false)
        val smokeAlarm = d.optBoolean("dym_alarm", false)
        val smokeOn = d.optBoolean("dym_wlaczony", false)
        val heatAlarm = d.optBoolean("alarm_ogrzewanie", false)
        val panelAlarm = d.optBoolean("alarm_panel", false)
        val mode = when (d.optInt("tryb_serwa", 0)) { 1 -> "AUTO"; 2 -> "RĘCZNY"; 3 -> "BEZPIECZNY"; else -> "—" }
        fun temp(v: Double) = if (v.isFinite()) String.format(Locale.US, "%.1f °C", v) else "—"

        setTile(TileId.OUTSIDE, temp(outside), if (!healthOut.isOnline) "OFFLINE" else if (healthOut.isStagnant) "⚠ STABILNY" else "LIVE", (((outside + 20) / 60).toFloat()).coerceIn(0f,1f), d=d)
        setTile(TileId.HEATING, temp(heat), if (heatAlarm) "ALARM" else if (!healthHeat.isOnline) "OFFLINE" else if (healthHeat.isStagnant) "⚠ STABILNY" else "AKTYWNY", (heat/160).toFloat().coerceIn(0f,1f), alarm=heatAlarm, d=d)
        setTile(TileId.BOILER, temp(boiler), if (!healthBoil.isOnline) "OFFLINE" else if (healthBoil.isStagnant) "⚠ STABILNY" else if (boiler > 45) "GORĄCY" else "STABILNY", ((boiler-15)/55).toFloat().coerceIn(0f,1f), d=d)
        setTile(TileId.PANEL, temp(panel), if (panelAlarm) "ALARM" else if (!healthPan.isOnline) "OFFLINE" else if (healthPan.isStagnant) "⚠ STABILNY" else "LIVE", .5f, alarm=panelAlarm, d=d)
        setTile(TileId.ROOM, temp(room), if (!healthRm.isOnline) "OFFLINE" else if (healthRm.isStagnant) "⚠ STABILNY" else "LIVE", .5f, d=d)
        setTile(TileId.PRESSURE, if (pressure.isFinite()) String.format(Locale.US,"%.0f hPa",pressure) else "—", if (!healthPres.isOnline) "OFFLINE" else "LIVE", ((pressure-970)/70).toFloat().coerceIn(0f,1f), d=d)
        setTile(TileId.HUMIDITY, if (!healthHum.isOnline || humidity == 0.0) "—" else if (humidity.isFinite()) String.format(Locale.US,"%.0f %%",humidity) else "—", if (!healthHum.isOnline) "CZUJNIK BRAK" else "LIVE", (humidity/100).toFloat().coerceIn(0f,1f), d=d)
        // [SYMULACJA 1:1 z Piec.html] Pokazuje "SYMULACJA · X min" w kafelkach gdy symulacja aktywna
        fun symStatus(sensorKey: String): String? {
            val symActive = d.optBoolean("symulacja_$sensorKey", false)
            return if (symActive) {
                val mins = d.optInt("symulacja_${sensorKey}_min", 0)
                "SYMULACJA · $mins min"
            } else null
        }
        
        val symOutside = symStatus("zewn")
        val symHeat = symStatus("ogrz")
        val symBoiler = symStatus("bojler")
        val symPanel = symStatus("panel")
        val symRoom = symStatus("pokoj")
        val symPressure = symStatus("cisnienie")
        val symHumidity = symStatus("wilgotnosc")
        val symSmoke = symStatus("dym")
        
        setTile(TileId.OUTSIDE, temp(outside), symOutside ?: if (!healthOut.isOnline) "OFFLINE" else if (healthOut.isStagnant) "⚠ STABILNY" else "LIVE", (((outside + 20) / 60).toFloat()).coerceIn(0f,1f), d=d)
        setTile(TileId.HEATING, temp(heat), symHeat ?: if (heatAlarm) "ALARM" else if (!healthHeat.isOnline) "OFFLINE" else if (healthHeat.isStagnant) "⚠ STABILNY" else "AKTYWNY", (heat/160).toFloat().coerceIn(0f,1f), alarm=heatAlarm, d=d)
        setTile(TileId.BOILER, temp(boiler), symBoiler ?: if (!healthBoil.isOnline) "OFFLINE" else if (healthBoil.isStagnant) "⚠ STABILNY" else if (boiler > 45) "GORĄCY" else "STABILNY", ((boiler-15)/55).toFloat().coerceIn(0f,1f), d=d)
        setTile(TileId.PANEL, temp(panel), symPanel ?: if (panelAlarm) "ALARM" else if (!healthPan.isOnline) "OFFLINE" else if (healthPan.isStagnant) "⚠ STABILNY" else "LIVE", .5f, alarm=panelAlarm, d=d)
        setTile(TileId.ROOM, temp(room), symRoom ?: if (!healthRm.isOnline) "OFFLINE" else if (healthRm.isStagnant) "⚠ STABILNY" else "LIVE", .5f, d=d)
        setTile(TileId.PRESSURE, if (pressure.isFinite()) String.format(Locale.US,"%.0f hPa",pressure) else "—", symPressure ?: if (!healthPres.isOnline) "OFFLINE" else "LIVE", ((pressure-970)/70).toFloat().coerceIn(0f,1f), d=d)
        setTile(TileId.HUMIDITY, if (!healthHum.isOnline || humidity == 0.0) "—" else if (humidity.isFinite()) String.format(Locale.US,"%.0f %%",humidity) else "—", symHumidity ?: if (!healthHum.isOnline) "CZUJNIK BRAK" else "LIVE", (humidity/100).toFloat().coerceIn(0f,1f), d=d)
        // Fix: Firmware sends degrees (0-180 for klapa, 0-90 for syberek), convert to percent
        val flapDeg = d.optInt("klapa",0)
        val flap = (flapDeg * 100 / 180).coerceIn(0,100)
        val damperDeg = d.optInt("syberka",0)
        val damper = (damperDeg * 100 / 90).coerceIn(0,100)
        setTile(TileId.PUMP, if (pump) "WŁĄCZONA" else "WYŁĄCZONA", if (pump) "AKTYWNA" else "AUTO", if (pump) 1f else 0f, active=pump, d=d)
        setTile(TileId.SERVO, "K$flap • S$damper", mode, flap/100f, active=mode=="RĘCZNY", d=d)
        val mix = d.optBoolean("mieszadlo", false)
        setTile(TileId.MIXER, if (mix) "WŁĄCZONE" else "WYŁĄCZONE", if (mix) "AKTYWNE" else "AUTO", if (mix) 1f else 0f, active=mix, d=d)
        setTile(TileId.SMOKE, when { smokeAlarm -> "ALARM"; smokeOn -> "AKTYWNY"; else -> "OK" }, symSmoke ?: if(smokeAlarm)"BEZPIECZEŃSTWO" else "MONITORING", .3f, active=smokeOn, alarm=smokeAlarm, d=d)
        setTile(TileId.CHARTS, "OTWÓRZ", "historia • 6 H / 24 H / 7 DNI / 30 DNI", .8f, d=d)
        setTile(TileId.CLOCK, SimpleDateFormat("HH:mm", Locale.US).format(Date()), "CZAS TELEFONU", .2f, d=d)

        heroTemp.text = temp(heat)
        heroMode.text = mode
        heroPump.text = if (pump) "ON" else "OFF"
        val anyAlarm = smokeAlarm || heatAlarm || panelAlarm
        heroAlarm.text = if (anyAlarm) "ALARM" else "OK"
        heroAlarm.setTextColor(if (anyAlarm) C.err else Color.WHITE)
        (quickPumpView.findViewWithTag("quickStatus") as? TextView)?.text = if (pump) "WŁĄCZONA · PRACA" else "WYŁĄCZONA"
        (quickServoView.findViewWithTag("quickStatus") as? TextView)?.text = "K${flap} · S${damper} · $mode"
        (quickMixerView.findViewWithTag("quickStatus") as? TextView)?.text = if (mix) "WŁĄCZONE · AKTYWNE" else "WYŁĄCZONE"
        val state = when {
            anyAlarm -> "ALARM"
            mode == "BEZPIECZNY" -> "TRYB BEZPIECZNY"
            pump -> "PRACA · POMPA"
            heat.isFinite() && heat >= 45.0 -> "DOGZEWANIE"
            else -> "CZUWANIE"
        }
        heroState.text = state
        heroState.setTextColor(if (anyAlarm) C.err else if (pump) C.accent else C.live)
        alarmBanner.visibility = if (anyAlarm) View.VISIBLE else View.GONE
        if (anyAlarm) {
            val reasons = buildList {
                if (heatAlarm) add("przegrzanie kotła")
                if (panelAlarm) add("alarm panelu")
                if (smokeAlarm) add("czujnik dymu")
            }
            alarmBannerText.text = "ALARM • ${reasons.joinToString(" · ")}"
        }
        heroOverview.submit(outside, heat, boiler, pump)
        heroBoiler.bind(heat, heat.isFinite() && heat > 45, smokeAlarm || heatAlarm || panelAlarm)
    }

    private fun setTile(id: TileId, value: String, extra: String, fraction: Float, active: Boolean = false, alarm: Boolean = false, d: JSONObject? = null) {
        val (sim, simMin) = if (d != null) isSimulated(id, d) else Pair(false, 0)
        val displayValue = value
        val displayExtra = if (sim) "SYM ${simMin}min" else extra
        tileViews[id]?.bind(kindFor(id), tileViews[id]?.contentDescription?.toString() ?: "", displayValue, displayExtra, accentFor(id), active, alarm, fraction, sim, simMin)
    }

    private fun simField(id: TileId): String? = when (id) {
        TileId.OUTSIDE -> "zewn"
        TileId.HEATING -> "ogrz"
        TileId.BOILER -> "bojler"
        TileId.PANEL -> "panel"
        TileId.ROOM -> "pokoj"
        TileId.PRESSURE -> "cisnienie"
        TileId.HUMIDITY -> "wilgotnosc"
        TileId.SMOKE -> "dym"
        else -> null
    }

    private fun isSimulated(id: TileId, d: org.json.JSONObject): Pair<Boolean, Int> {
        val field = simField(id) ?: return Pair(false, 0)
        val active = d.optBoolean("symulacja_$field", false)
        val minutes = d.optInt("symulacja_${field}_min", 0)
        return Pair(active && minutes > 0, minutes)
    }

    private fun onTileClicked(id: TileId) {
        when (id) {
            TileId.OUTSIDE -> showSensorMenu("Zewnętrzna", "zewn", "t_zewn", "°C", -30f, 50f, .5f)
            TileId.HEATING -> showOverheatMenu(false)
            TileId.BOILER -> showSensorMenu("Bojler", "bojler", "t_bojler", "°C", 0f, 160f, .5f)
            TileId.PANEL -> showOverheatMenu(true)
            TileId.ROOM -> showSensorMenu("Pomieszczenie", "pokoj", "t_pokoj", "°C", -50f, 50f, .5f)
            TileId.PRESSURE -> showSensorMenu("Ciśnienie", "cisnienie", "cisnienie", "hPa", 970f, 1040f, 1f)
            TileId.HUMIDITY -> showSensorMenu("Wilgotność", "wilgotnosc", "wilgotnosc", "%", 0f, 100f, 1f)
            TileId.PUMP -> showPumpMenu()
            TileId.SERVO -> showServoMenu()
            TileId.MIXER -> showMixerMenu()
            TileId.SMOKE -> showSmokeMenu()
            TileId.CHARTS -> navigate(1)
            TileId.CLOCK -> showClockMenu()
        }
    }

    private fun loadHistory(seconds: Long) {
        val end = System.currentTimeMillis() / 1000
        val start = end - seconds
        val rangeLabel = when (seconds) { 6*3600L -> "6 H"; 24*3600L -> "24 H"; 7*24*3600L -> "7 DNI"; else -> "30 DNI" }
        historyInfo.text = "Pobieram $rangeLabel…"
        val requestId = historyRequestId.incrementAndGet()
        historyExecutor.execute {
            // A newer range was already requested while this one was waiting in the queue.
            if (requestId != historyRequestId.get()) return@execute
            try {
                val token = ensureToken(false) ?: throw IllegalStateException("Brak sesji")
                val rows = TelemetryHistoryRepository.fetchRange(firebase, token, start, end)
                runOnUiThread {
                    if (requestId != historyRequestId.get() || sessionNeedsLogin) return@runOnUiThread
                    tempChart.setData(rows)
                    servoChart.setData(rows)
                    historyInfo.text = "${rows.size} próbek  ·  ${fmtTs(start)} → ${fmtTs(end)}"
                    refreshChartChrome()
                }
            } catch (e: Exception) {
                runOnUiThread { if (requestId == historyRequestId.get()) historyInfo.text = "Błąd historii: ${e.message}" }
            }
        }
    }

    private fun setRangeState(selected: TextView, vararg others: TextView) {
        updateSelector(selected, true)
        others.forEach { updateSelector(it, false) }
    }

    private fun showSeriesSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val catalog = if (temperature) tempSeries() else servoSeries()
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val selectedCount = label("${chart.selectedSeries().size} wybrane", 9.5f, C.cyan, true)
        val intro = LinearLayout(this).apply { orientation=LinearLayout.HORIZONTAL; gravity=Gravity.CENTER_VERTICAL }
        intro.addView(label("Widoczne przebiegi",10.7f,C.textDim,false),LinearLayout.LayoutParams(0,-2,1f))
        intro.addView(selectedCount)
        body.addView(intro,lp(-1,-2,2))

        val rowsHost=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        fun redraw(){
            rowsHost.removeAllViews()
            selectedCount.text="${chart.selectedSeries().size} wybrane"
            catalog.forEach { sr ->
                val on=chart.isSeriesEnabled(sr.id)
                val row=LinearLayout(this).apply{
                    orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL
                    setPadding(dp(11),dp(9),dp(9),dp(9))
                    background=rounded(if(on)Color.argb(28,Color.red(sr.accent),Color.green(sr.accent),Color.blue(sr.accent)) else C.surface2,16,
                        if(on)Color.argb(145,Color.red(sr.accent),Color.green(sr.accent),Color.blue(sr.accent)) else C.border)
                    isClickable=true;isFocusable=true
                }
                val swatch=View(this).apply{background=rounded(sr.accent,7,0)}
                row.addView(swatch,lp(5,28))
                val cp=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
                cp.addView(label(sr.label,13f,Color.WHITE,true))
                cp.addView(label(if(sr.servo)"Pozycja rzeczywista · %" else "Kanał telemetryczny · ${sr.unit}",9.3f,C.textDim,false,dp(2)))
                row.addView(cp,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(10)})
                val check=NativeIconView(this).apply{icon=if(on)NativeIconView.Icon.SHIELD else NativeIconView.Icon.CLOSE;tint=if(on)C.live else C.textDim2;active=on}
                row.addView(check,lp(30,30))
                row.setOnClickListener{chart.setSeriesEnabled(sr.id,!chart.isSeriesEnabled(sr.id));redraw();refreshChartChrome()}
                rowsHost.addView(row,lp(-1,-2,7))
            }
        }
        redraw(); body.addView(rowsHost)
        val foot=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        val clear=action("MINIMUM 1",C.surface2,C.textDim,0).apply{setOnClickListener{if(chart.selectedSeries().size>1){val keep=chart.selectedSeries().first().id;catalog.forEach{chart.setSeriesEnabled(it.id,it.id==keep)};redraw();refreshChartChrome()}}}
        val done=action("GOTOWE",C.accent,C.bg,0).apply{setOnClickListener{dismissSheet()}}
        foot.addView(clear,LinearLayout.LayoutParams(0,dp(44),1f));foot.addView(done,LinearLayout.LayoutParams(0,dp(44),1f).apply{marginStart=dp(7)})
        body.addView(foot,lp(-1,44,10))
        showSheet(if(temperature)"Serie temperatur" else "Serie serw",body)
    }

    private fun Boolean.ifThen(a:String,b:String)=if(this)a else b

    private fun showModeSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val body=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        body.addView(label("Wybierz sposób prezentacji skali i osi. Zmiana działa od razu.",10.7f,C.textDim,false,dp(2)))
        val options=listOf(
            ProfessionalTelemetryChartView.Mode.SPLIT to ("PODZIELONE OSIE" to "Oddzielna skala dla różnych jednostek"),
            ProfessionalTelemetryChartView.Mode.COMMON to ("WSPÓLNA OŚ" to "Jedna skala — najlepsza dla kilku temperatur"),
            ProfessionalTelemetryChartView.Mode.NORMALIZED to ("NORMALIZOWANE" to "Każda seria 0–100%"),
            ProfessionalTelemetryChartView.Mode.FROM_START to ("OD STARTU" to "Zmiana procentowa względem początku"),
            ProfessionalTelemetryChartView.Mode.XY to ("XY / KORELACJA" to "Porównanie dwóch serii punkt po punkcie")
        )
        options.forEach{(m,tx)->
            val on=chart.currentMode()==m
            val row=LinearLayout(this).apply{
                orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL
                setPadding(dp(11),dp(10),dp(10),dp(10))
                background=rounded(if(on)Color.argb(34,85,216,247) else C.surface2,16,if(on)C.cyan else C.border)
                isClickable=true;isFocusable=true
            }
            val icon=NativeIconView(this).apply{icon=when(m){ProfessionalTelemetryChartView.Mode.NORMALIZED->NativeIconView.Icon.CHART;ProfessionalTelemetryChartView.Mode.SPLIT->NativeIconView.Icon.THERMOMETER;ProfessionalTelemetryChartView.Mode.XY->NativeIconView.Icon.CHART;else->NativeIconView.Icon.SETTINGS};tint=if(on)C.cyan else C.textDim2;active=on}
            row.addView(icon,lp(32,32))
            val copy=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
            copy.addView(label(tx.first,13.0f,Color.WHITE,true))
            copy.addView(label(tx.second,9.4f,C.textDim,false,dp(2)))
            row.addView(copy,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(9)})
            val check=label(if(on)"AKTYWNE" else "",7.9f,if(on)C.cyan else C.textDim2,true).apply{gravity=Gravity.CENTER}
            row.addView(check,lp(54,26))
            row.setOnClickListener{chart.setMode(m);dismissSheet();refreshChartChrome()}
            body.addView(row,lp(-1,-2,7))
        }
        showSheet("Widok wykresu",body)
    }

    private fun showFullScreenChart(source: ProfessionalTelemetryChartView, temperature: Boolean) {
        val dialog = Dialog(this)
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        val frame = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(14), dp(12), dp(14), dp(16))
            background = roundedGradient(0xFF06121E.toInt(), 0xFF0C2032.toInt(), 0x72466C84, 0x2E2E4559, 22)
        }
        val header = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val hc = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        hc.addView(label(if (temperature) "TEMPERATURY" else "POZYCJE SERW", 20f, Color.WHITE, true))
        hc.addView(label(if (temperature) "Studio historii telemetrycznej · °C" else "Studio pozycji rzeczywistej · %", 9.2f, C.textDim, false, dp(2)))
        header.addView(hc, LinearLayout.LayoutParams(0, -2, 1f))
        header.addView(iconButton(NativeIconView.Icon.CLOSE, "Zamknij pełny ekran") { dialog.dismiss() }, lp(42, 40))
        frame.addView(header)
        val meta = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(9), 0, dp(7))
        }
        val leftMeta = label("${source.selectedSeries().size} serie  ·  ${if (source.isRealOnly()) "REAL" else "RAW"}", 8.6f, C.cyan, true)
        meta.addView(leftMeta, LinearLayout.LayoutParams(0, -2, 1f))
        meta.addView(action(shortMode(source).removePrefix("WIDOK: "), C.surface2, C.text, 0).apply { setOnClickListener { dialog.dismiss(); showModeSheet(source, temperature) } }, lp(118, 38))
        frame.addView(meta)

        val clone = ProfessionalTelemetryChartView(this).apply {
            setCatalog(if (temperature) tempSeries() else servoSeries())
            setData(source.snapshot(), false)
            setMode(source.currentMode())
            setLineStyle(if (source.isSmooth()) ProfessionalTelemetryChartView.LineStyle.SMOOTH else ProfessionalTelemetryChartView.LineStyle.STEPPED)
            setRealOnly(source.isRealOnly())
            setAnomalyFilter(source.isAnomalyFilter())
            if (!temperature) setFixedRange(0.0, 100.0)
        }
        val catalog = if (temperature) tempSeries() else servoSeries()
        catalog.forEach { s -> clone.setSeriesEnabled(s.id, source.isSeriesEnabled(s.id)) }
        frame.addView(clone, LinearLayout.LayoutParams(-1, 0, 1f))
        frame.addView(label("DOTKNIJ  marker     SZCZYPNIJ  zoom     PODWÓJNY TAP  reset", 8.2f, C.textDim2, false).apply { gravity = Gravity.CENTER; setPadding(0, dp(9), 0, 0) })
        dialog.setContentView(frame)
        dialog.show()
        dialog.window?.apply {
            setBackgroundDrawableResource(android.R.color.transparent)
            setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT)
            attributes = attributes.apply { gravity = Gravity.CENTER; dimAmount = .74f }
            addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND)
        }
    }

    private fun showChartToolsSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val body=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        body.addView(label("To są ustawienia prezentacji. Dane RAW w Firebase pozostają bez zmian.",10.7f,C.textDim,false,dp(2)))
        fun toggleRow(title:String,desc:String,on:Boolean,iconType:NativeIconView.Icon,click:()->Unit){
            val row=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(7),dp(8),dp(7),dp(8));background=rounded(C.surface2,16,if(on)0x8042A8C0.toInt() else C.border);isClickable=true;isFocusable=true}
            val iv=NativeIconView(this).apply{icon=iconType;tint=if(on)C.cyan else C.textDim2;active=on}
            row.addView(iv,lp(34,34))
            val cp=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
            cp.addView(label(title,13f,Color.WHITE,true));cp.addView(label(desc,9.3f,C.textDim,false,dp(2)))
            row.addView(cp,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(9)})
            val sw=TextView(this).apply{gravity=Gravity.CENTER;text=if(on)"ON" else "OFF";textSize=8.3f;setTextColor(if(on)C.bg else C.textDim);typeface=Typeface.DEFAULT_BOLD;background=rounded(if(on)C.cyan else 0x1AFFFFFF,13,if(on)C.cyan else C.border)}
            row.addView(sw,lp(50,28));row.setOnClickListener{click()}
            body.addView(row,lp(-1,-2,7))
        }
        toggleRow("Rzeczywiste","ukryj q≠0 i symulację",chart.isRealOnly(),NativeIconView.Icon.SHIELD){chart.setRealOnly(!chart.isRealOnly());dismissSheet();showChartToolsSheet(chart,temperature)}
        toggleRow("Filtr anomalii","odrzuć odstające skoki",chart.isAnomalyFilter(),NativeIconView.Icon.CHART){chart.setAnomalyFilter(!chart.isAnomalyFilter());dismissSheet();showChartToolsSheet(chart,temperature)}
        toggleRow("Pasmo min–max","wypełnienie zakresu dwóch pierwszych serii",chart.isBandVisible(),NativeIconView.Icon.CHART){chart.toggleBand();dismissSheet();showChartToolsSheet(chart,temperature)}
        val lineRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(11),dp(10),dp(10),dp(10));background=rounded(C.surface2,16,C.border)}
        lineRow.addView(NativeIconView(this).apply{icon=NativeIconView.Icon.CHART;tint=C.accent;active=true},lp(34,34))
        val lineCopy=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        lineCopy.addView(label("Sposób rysowania",13f,Color.WHITE,true));lineCopy.addView(label(if(chart.isSmooth())"Gładka · interpolowana" else "Schodkowa · wartości skokowe",9.3f,C.textDim,false,dp(2)))
        lineRow.addView(lineCopy,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(9)})
        val lineBtn=action(if(chart.isSmooth())"SCHODKOWA" else "GŁADKA",C.surface2,C.text,0);lineBtn.setOnClickListener{chart.setLineStyle(if(chart.isSmooth())ProfessionalTelemetryChartView.LineStyle.STEPPED else ProfessionalTelemetryChartView.LineStyle.SMOOTH);dismissSheet();showChartToolsSheet(chart,temperature)};lineRow.addView(lineBtn,lp(92,38));body.addView(lineRow,lp(-1,-2,7))
        body.addView(label("ZOOM",8f,C.textDim2,true,dp(4)))
        val zoom=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        val minus=action("−",C.surface2,Color.WHITE,0);val reset=action("RESET WIDOKU",C.surface2,Color.WHITE,0);val plus=action("+",C.surface2,Color.WHITE,0)
        minus.setOnClickListener{chart.zoomBy(.78f)};reset.setOnClickListener{chart.resetViewport(true)};plus.setOnClickListener{chart.zoomBy(1.28f)}
        zoom.addView(minus,lp(54,42));zoom.addView(reset,LinearLayout.LayoutParams(0,dp(42),1f).apply{marginStart=dp(6);marginEnd=dp(6)});zoom.addView(plus,lp(54,42));body.addView(zoom,lp(-1,42,3))
        showSheet("Narzędzia wykresu",body)
    }

    private fun showChartAnalysisSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("Analiza liczy się z danych już pobranych do aplikacji. RAW Firebase pozostaje bez zmian.", 10.4f, C.textDim, false, dp(2)))

        val rows = chart.snapshot()
        val series = chart.selectedSeries()
        if (rows.size < 2 || series.isEmpty()) {
            body.addView(label("Brak wystarczających danych do analizy.", 12f, C.text, true, dp(14)))
            showSheet("Analiza PRO", body)
            return
        }

        series.take(6).forEach { s ->
            val vals = rows.mapNotNull { row ->
                if (s.servo) {
                    val deg = if (s.id == "servo_flap") row.k else row.s
                    if (!deg.isFinite()) null else if (s.id == "servo_flap") deg / 180.0 * 100.0 else deg / 90.0 * 100.0
                } else s.channel?.let { ch -> row.a.getOrNull(ch)?.takeIf { it.isFinite() }?.times(s.scale) }
            }
            if (vals.isEmpty()) return@forEach
            val min = vals.minOrNull() ?: 0.0
            val max = vals.maxOrNull() ?: 0.0
            val avg = vals.average()
            val delta = vals.last() - vals.first()
            val unit = if (s.servo) "%" else s.unit
            val card = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                setPadding(dp(12), dp(10), dp(12), dp(10))
                background = rounded(C.surface2, 14, C.border)
            }
            card.addView(label(s.label, 13f, s.accent, true))
            card.addView(label("MIN  ${String.format(Locale.US, "%.1f", min)} $unit   ·   MAX  ${String.format(Locale.US, "%.1f", max)} $unit", 9.5f, C.text, true, dp(5)))
            card.addView(label("ŚREDNIA  ${String.format(Locale.US, "%.1f", avg)} $unit   ·   ZMIANA  ${if (delta >= 0) "+" else ""}${String.format(Locale.US, "%.1f", delta)} $unit", 9.2f, C.textDim, false, dp(2)))
            body.addView(card, lp(-1, -2, 7))
        }

        val gaps = rows.zipWithNext().count { (a,b) -> b.ts - a.ts > 150L }
        val footer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(12), dp(10), dp(12), dp(10))
            background = rounded(C.surface, 14, C.border)
        }
        footer.addView(label("DIAGNOSTYKA", 8.4f, C.cyan, true))
        footer.addView(label("Punkty: ${rows.size}   ·   GAP: $gaps   ·   tryb: ${shortMode(chart)}", 9.5f, C.textDim, false, dp(4)))
        footer.addView(label("Rzeczywiste: ${if (chart.isRealOnly()) "TAK" else "NIE"}   ·   filtr anomalii: ${if (chart.isAnomalyFilter()) "TAK" else "NIE"}   ·   linia: ${if (chart.isSmooth()) "gładka" else "schodkowa"}", 9.2f, C.textDim2, false, dp(2)))
        body.addView(footer, lp(-1, -2, 10))

        if (series.size >= 2 && series[0].unit == series[1].unit && !series[0].servo && !series[1].servo) {
            val a = rows.mapNotNull { row -> series[0].channel?.let { ch -> row.a.getOrNull(ch)?.takeIf(Double::isFinite)?.times(series[0].scale) } }
            val b = rows.mapNotNull { row -> series[1].channel?.let { ch -> row.a.getOrNull(ch)?.takeIf(Double::isFinite)?.times(series[1].scale) } }
            if (a.isNotEmpty() && b.isNotEmpty()) {
                val n = minOf(a.size, b.size)
                val current = a[n-1] - b[n-1]
                val avgDelta = (0 until n).map { a[it] - b[it] }.average()
                body.addView(label("A − B  (${series[0].label} − ${series[1].label})  ·  teraz ${String.format(Locale.US, "%.1f", current)} ${series[0].unit}  ·  średnio ${String.format(Locale.US, "%.1f", avgDelta)} ${series[0].unit}", 9.6f, C.text, true, dp(2)))
            }
        }

        val alarm = action(if (chart.hasAlarmLines()) "EDYTUJ ALARMY" else "USTAW ALARMY", C.surface2, C.text, 0)
        alarm.setOnClickListener { dismissSheet(); showAlarmEditor(chart) }
        body.addView(alarm, lp(-1, 46, 10))
        showSheet("Analiza PRO", body)
    }

    private fun showDeltaSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val selected = chart.selectedSeries()
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("PORÓWNANIE A − B", 10f, C.cyan, true))
        body.addView(label("Wspólne chwile telemetryczne · bez zmiany danych RAW.", 10.5f, C.textDim, false, dp(3)))
        if (selected.size < 2) {
            body.addView(label("Wybierz co najmniej dwie serie w przycisku SERIE.", 12f, C.text, true, dp(14)))
        } else {
            val a = selected[0]; val b = selected[1]
            val av = chart.snapshot().mapNotNull { valueForSeries(a, it) }
            val bv = chart.snapshot().mapNotNull { valueForSeries(b, it) }
            val st = TelemetryAnalytics.stats(TelemetryAnalytics.subtract(av, bv))
            body.addView(analysisMetricCard("${a.label} − ${b.label}", st, if (a.servo) "%" else a.unit), lp(-1, -2, 12))
        }
        body.addView(action("WYBIERZ SERIE", C.accent, C.bg, 0).apply { setOnClickListener { dismissSheet(); showSeriesSheet(chart, temperature) } }, lp(-1, 46, 8))
        showSheet("A − B", body)
    }

    private fun showCycleSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("ANALIZA CYKLI", 10f, C.cyan, true))
        if (!temperature) {
            body.addView(label("Cykle pracy dotyczą temperatury kotła.", 12f, C.text, true, dp(14)))
        } else {
            val channel = tempSeries().firstOrNull { it.id == "t_ogrz" }?.channel ?: 1
            val cs = TelemetryAnalytics.heatingCycles(chart.snapshot(), channel)
            body.addView(analysisPair("STAN", cs.currentState, C.live))
            body.addView(analysisPair("CYKLE", cs.cycles.toString(), C.cyan))
            body.addView(analysisPair("ŚREDNI CZAS", String.format(Locale.US, "%.1f min", cs.averageMinutes), C.text))
            body.addView(analysisPair("NAJDŁUŻSZY", String.format(Locale.US, "%.1f min", cs.longestMinutes), C.text))
            body.addView(label("Próg analizy 45 °C · histereza 2 °C. To informacja z historii, nie nastawa pieca.", 9.3f, C.textDim2, false, dp(10)))
        }
        showSheet("Cykle pracy", body)
    }

    private fun showNormSheet(chart: ProfessionalTelemetryChartView, temperature: Boolean) {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("ODCHYLENIE OD NORMY", 10f, C.cyan, true))
        body.addView(label("Norma jest liczona z aktualnie pobranej historii.", 10.5f, C.textDim, false, dp(3)))
        chart.selectedSeries().take(6).forEach { sr ->
            val values = chart.snapshot().mapNotNull { valueForSeries(sr, it) }
            val st = TelemetryAnalytics.stats(values) ?: return@forEach
            val mad = TelemetryAnalytics.mad(values) ?: 0.0
            val unit = if (sr.servo) "%" else sr.unit
            val card = analysisMetricCard(sr.label, st, unit)
            card.addView(label("MAD  ${String.format(Locale.US, "%.1f", mad)} $unit   ·   STD  ${String.format(Locale.US, "%.1f", st.stdev)} $unit", 9.2f, C.textDim, false, dp(2)))
            body.addView(card, lp(-1, -2, 7))
        }
        showSheet("Odchylenie", body)
    }

    private fun valueForSeries(series: ProfessionalTelemetryChartView.Series, row: TelemetryRecord): Double? {
        if (series.servo) {
            val deg = if (series.id == "servo_flap") row.k else row.s
            return if (!deg.isFinite()) null else if (series.id == "servo_flap") deg / 180.0 * 100.0 else deg / 90.0 * 100.0
        }
        val ch = series.channel ?: return null
        return row.a.getOrNull(ch)?.takeIf { it.isFinite() }?.times(series.scale)
    }

    private fun analysisMetricCard(titleText: String, st: TelemetryAnalytics.SeriesStats?, unit: String): LinearLayout = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(dp(12), dp(10), dp(12), dp(10))
        background = rounded(C.surface2, 15, C.border)
        addView(label(titleText, 13f, Color.WHITE, true))
        if (st != null) {
            addView(label("MIN ${String.format(Locale.US, "%.1f", st.min)} $unit   ·   MAX ${String.format(Locale.US, "%.1f", st.max)} $unit", 9.7f, C.text, true, dp(5)))
            addView(label("ŚREDNIA ${String.format(Locale.US, "%.1f", st.average)} $unit   ·   ZMIANA ${if (st.delta >= 0) "+" else ""}${String.format(Locale.US, "%.1f", st.delta)} $unit", 9.3f, C.textDim, false, dp(2)))
        }
    }

    private fun analysisPair(name: String, value: String, color: Int): View = LinearLayout(this).apply {
        orientation = LinearLayout.HORIZONTAL
        gravity = Gravity.CENTER_VERTICAL
        setPadding(dp(12), dp(10), dp(12), dp(10))
        background = rounded(C.surface2, 14, C.border)
        addView(label(name, 8.3f, C.textDim2, true), LinearLayout.LayoutParams(0, -2, 1f))
        addView(label(value, 14f, color, true))
        layoutParams = lp(-1, -2, 7)
    }

    private fun showAlarmEditor(chart: ProfessionalTelemetryChartView) {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("Poziomy są liniami pomocniczymi na wykresie. Nie wysyłają żadnej komendy do pieca.", 10.2f, C.textDim, false))
        fun field(hint: String): EditText = EditText(this).apply {
            this.hint = hint
            setHintTextColor(C.textDim2)
            setTextColor(Color.WHITE)
            textSize = 11.0f
            inputType = android.text.InputType.TYPE_CLASS_NUMBER or android.text.InputType.TYPE_NUMBER_FLAG_DECIMAL or android.text.InputType.TYPE_NUMBER_FLAG_SIGNED
            setSingleLine(true)
            background = rounded(C.surface2, 13, C.border)
            setPadding(dp(10), 0, dp(10), 0)
        }
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val loLo = field("LoLo"); val lo = field("Lo"); val hi = field("Hi"); val hiHi = field("HiHi")
        listOf(loLo, lo, hi, hiHi).forEachIndexed { i, v -> row.addView(v, LinearLayout.LayoutParams(0,dp(44), 1f).apply { if (i > 0) marginStart = dp(6) }) }
        body.addView(row, lp(-1, 44, 12))
        val buttons = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val apply = action("POKAŻ NA WYKRESIE", C.accent, C.bg, 0)
        val clear = action("WYCZYŚĆ", C.surface2, C.text, 0)
        apply.setOnClickListener {
            fun num(e: EditText): Double? = e.text.toString().replace(',', '.').trim().takeIf { it.isNotBlank() }?.toDoubleOrNull()
            chart.setAlarmLines(num(loLo), num(lo), num(hi), num(hiHi), true)
            dismissSheet(); refreshChartChrome()
        }
        clear.setOnClickListener { chart.clearAlarmLines(); dismissSheet(); refreshChartChrome() }
        buttons.addView(apply, LinearLayout.LayoutParams(0,dp(46), 1f))
        buttons.addView(clear, LinearLayout.LayoutParams(0,dp(46), 1f).apply { marginStart = dp(6) })
        body.addView(buttons, lp(-1, 46, 10))
        showSheet("Alarmy 4-poziomowe", body)
    }

    private var activeSheet: Dialog? = null
    private fun showSheet(title: String, body: View) {
        activeSheet?.dismiss()
        val d = Dialog(this)
        d.requestWindowFeature(Window.FEATURE_NO_TITLE)
        val frame = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(7), dp(16), dp(18))
            background = rounded(C.surface, 22, C.border)
        }
        val handle = View(this).apply { background = rounded(0x5A71869A, 8, 0) }
        frame.addView(handle, lp(42, 5, 2))
        val hd = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(3), 0, 0) }
        val sheetIconType = when {
            title.contains("Serw", true) -> NativeIconView.Icon.SERVO
            title.contains("Pompa", true) -> NativeIconView.Icon.PUMP
            title.contains("Mieszad", true) -> NativeIconView.Icon.MIXER
            title.contains("Wykres", true) || title.contains("Serie", true) || title.contains("Widok", true) -> NativeIconView.Icon.CHART
            title.contains("Dym", true) || title.contains("Alarm", true) -> NativeIconView.Icon.SHIELD
            title.contains("Pogoda", true) -> NativeIconView.Icon.OUTSIDE
            title.contains("OTA", true) -> NativeIconView.Icon.UPLOAD
            else -> NativeIconView.Icon.SETTINGS
        }
        hd.addView(NativeIconView(this).apply { this.icon=sheetIconType; tint=C.cyan; active=true; background=rounded(0x1426B9D6,12,0x4036BBD5) },lp(38,38))
        hd.addView(label(title, 18.2f, Color.WHITE, true), LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(10)})
        val close = iconButton(NativeIconView.Icon.CLOSE, "Zamknij") { d.dismiss() }
        hd.addView(close, lp(42, 40))
        frame.addView(hd)
        val scroll = ScrollView(this).apply { addView(body); overScrollMode = View.OVER_SCROLL_IF_CONTENT_SCROLLS }
        frame.addView(scroll, LinearLayout.LayoutParams(-1, 0, 1f).apply { topMargin = dp(10) })
        d.setContentView(frame)
        d.setOnShowListener {
            val w = d.window ?: return@setOnShowListener
            w.setBackgroundDrawableResource(android.R.color.transparent)
            w.setDimAmount(.62f)
            w.addFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND)
            w.setLayout(WindowManager.LayoutParams.MATCH_PARENT, sheetHeight())
            w.setGravity(Gravity.BOTTOM)
        }
        d.setOnDismissListener { if (activeSheet === d) activeSheet = null }
        activeSheet = d
        d.show()
        d.window?.apply {
            setBackgroundDrawableResource(android.R.color.transparent)
            setLayout(WindowManager.LayoutParams.MATCH_PARENT, sheetHeight())
            attributes = attributes.apply { gravity = Gravity.BOTTOM; dimAmount = .58f }
        }
    }
    private fun sheetHeight(): Int = minOf(dp(700), (resources.displayMetrics.heightPixels * 0.84f).toInt())
    private fun dismissSheet() { activeSheet?.dismiss(); activeSheet = null }

    private fun showSensorMenu(title:String, field:String, key:String, unit:String, min:Float, max:Float, step:Float) {
        val d = lastStatusData
        val isSimActive = d?.optBoolean("symulacja_$field", false) == true
        val simMin = d?.optInt("symulacja_${field}_min", 0) ?: 0
        val simValue = d?.optDouble(key, Double.NaN) ?: Double.NaN

        val box = LinearLayout(this).apply { orientation=LinearLayout.VERTICAL; setPadding(dp(4),dp(4),dp(4),dp(10)) }

        // Status bar - if simulation active, show yellow highlight
        if (isSimActive && simMin > 0) {
            val simBox = LinearLayout(this).apply {
                orientation=LinearLayout.HORIZONTAL; gravity=Gravity.CENTER_VERTICAL
                background=rounded(0x1EEAB308,12,0x30EAB308); setPadding(dp(12),dp(10),dp(12),dp(10))
            }
            simBox.addView(label("SYMULACJA AKTYWNA",10f,0xFFFFD166.toInt(),true))
            val countdownLbl = label("$simMin min",15f,0xFFFFE082.toInt(),true)
            countdownLbl.tag = "sim_countdown_$field"
            simBox.addView(View(this), LinearLayout.LayoutParams(0,0,1f))
            simBox.addView(countdownLbl)
            box.addView(simBox, lp(-1,-2,6))

            // Countdown timer
            val handler = android.os.Handler(android.os.Looper.getMainLooper())
            val runnable = object : Runnable {
                var remaining = simMin * 60
                override fun run() {
                    if (remaining > 0 && box.tag == "sim_active") {
                        remaining--
                        val m = remaining / 60; val s = remaining % 60
                        (box.findViewWithTag("sim_countdown_$field") as? TextView)?.text = String.format("%d:%02d", m, s)
                        handler.postDelayed(this, 1000)
                    }
                }
            }
            box.tag = "sim_active"
            handler.post(runnable)
        }

        box.addView(label("AKTUALNIE",9.5f,C.textDim,true, if (isSimActive) dp(12) else 0))
        val curVal = valueFor(d,key,unit)
        val curLabel = label(curVal,20f, if (isSimActive) 0xFFFFE082.toInt() else Color.WHITE, true, dp(3))
        curLabel.tag = "sim_current_$field"
        box.addView(curLabel)

        if (isSimActive) {
            box.addView(label("Symulowana wartość (po zastosowaniu zmieni odczyt)",9.5f,0xFFEAB308.toInt(),false,dp(2)))
        }

        val seekMax = ((max-min)/step).toInt().coerceAtLeast(1)
        val initialVal = if (simValue.isFinite()) simValue else numberFrom(d,key,min.toDouble())
        val seekProgress = ((initialVal-min)/step).toInt().coerceIn(0,seekMax)
        val seek = SeekBar(this).apply { this.max=seekMax; progress=seekProgress }
        val v=label(formatSlider(seek.progress,min,step,unit),15f,Color.WHITE,true,dp(8));
        seek.setOnSeekBarChangeListener(object:SeekBar.OnSeekBarChangeListener{override fun onProgressChanged(s:SeekBar?,p:Int,f:Boolean){v.text=formatSlider(p,min,step,unit)};override fun onStartTrackingTouch(s:SeekBar?){};override fun onStopTrackingTouch(s:SeekBar?){}})
        box.addView(v);box.addView(seek)

        val initialTimeMin = if (isSimActive && simMin > 0) ((simMin / 5).coerceIn(1,36) - 1) else 11
        val time=SeekBar(this).apply{this.max=35;progress=initialTimeMin}
        val tl=label("Czas: ${(time.progress+1)*5} min",11.5f,C.textDim,false,dp(8));
        time.setOnSeekBarChangeListener(object:SeekBar.OnSeekBarChangeListener{override fun onProgressChanged(s:SeekBar?,p:Int,f:Boolean){tl.text="Czas: ${(p+1)*5} min"};override fun onStartTrackingTouch(s:SeekBar?){};override fun onStopTrackingTouch(s:SeekBar?){}})
        box.addView(tl);box.addView(time)

        val apply=action("ZASTOSUJ",C.accent,C.bg,0)
        apply.setOnClickListener {
            val simVal = sliderValue(seek,min,step)
            val simTime = (time.progress+1)*5
            sendCommand("symuluj $field $simVal $simTime")
            // Update UI
            (box.findViewWithTag("sim_countdown_$field") as? TextView)?.text = "$simTime min"
            box.tag = "sim_active"
        }
        box.addView(apply,lp(-1,48,10))

        val stop=action("WYŁĄCZ SYMULACJĘ",C.surface2,Color.WHITE,0)
        stop.setOnClickListener{
            sendCommand("symuluj_stop $field")
            box.tag = null
            activeSheet?.dismiss()
        }
        box.addView(stop,lp(-1,48,7))

        showSheet(title,box)
    }

    private fun showOverheatMenu(panel:Boolean){
        val key=if(panel)"t_panel" else "t_ogrz"
        val d=lastStatusData
        val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(4),dp(4),dp(4),dp(10))}
        box.addView(label("TEMPERATURA",9.5f,C.textDim,true))
        box.addView(label(valueFor(d,key,"°C"),20f,Color.WHITE,true,dp(3)))
        val isAlarm=d?.optBoolean(if(panel)"alarm_panel" else "alarm_ogrzewanie") == true
        box.addView(label(if(isAlarm)"ALARM AKTYWNY" else "Brak alarmu przegrzania",12f,if(isAlarm)C.err else C.live,false,dp(4)))

        box.addView(label("PRÓG ALARMU °C",9f,C.textDim,true,dp(10)))
        val progRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val progInp=EditText(this).apply{setText((d?.optInt("progAlarmTemp",100)?:100).toString());setTextColor(Color.WHITE);setBackgroundColor(0x22FFFFFF.toInt());setPadding(dp(10),dp(8),dp(10),dp(8));inputType=android.text.InputType.TYPE_CLASS_NUMBER}
        progRow.addView(progInp,LinearLayout.LayoutParams(0,-2,1f))
        val progBtn=action("USTAW",C.cyan,C.bg,0);progBtn.setOnClickListener{sendCommand("ustaw progAlarmTemp "+progInp.text.toString())};progRow.addView(progBtn,lp(-1,42))
        box.addView(progRow,lp(-1,-2,4))
        box.addView(label("Histereza: piec −10°C, panel −4°C",8f,C.textDim,false,dp(2)))

        box.addView(label("HISTEREZA °C",9f,C.textDim,true,dp(10)))
        val histRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val histInp=EditText(this).apply{setText((d?.optInt("histerServo",5)?:5).toString());setTextColor(Color.WHITE);setBackgroundColor(0x22FFFFFF.toInt());setPadding(dp(10),dp(8),dp(10),dp(8));inputType=android.text.InputType.TYPE_CLASS_NUMBER}
        histRow.addView(histInp,LinearLayout.LayoutParams(0,-2,1f))
        val histBtn=action("USTAW",C.cyan,C.bg,0);histBtn.setOnClickListener{sendCommand("ustaw histerServo "+histInp.text.toString())};histRow.addView(histBtn,lp(-1,42))
        box.addView(histRow,lp(-1,-2,4))

        box.addView(label("AKCJE",9f,C.textDim,true,dp(14)))
        listOf("WYCISZ BRZĘCZYK" to "wycisz_ogrz","RESET ALARMÓW" to "reset_alarmow").forEach{(t,c)->
            val b=action(t,0xFF3A1820.toInt(),0xFFFFB2BE.toInt(),0);b.setOnClickListener{sendCommand(c)};box.addView(b,lp(-1,48,6))
        }
        showSheet(if(panel)"Panel słoneczny — próg przegrzania" else "Piec C.O. — próg przegrzania",box)
    }
    private fun showPumpMenu(){val d=lastStatusData;val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL};box.addView(label("STAN",9.5f,C.textDim,true));box.addView(label(if(d?.optBoolean("pompa")==true)"WŁĄCZONA" else "WYŁĄCZONA",19f,Color.WHITE,true,dp(3)));listOf("WŁ." to "pompa_wl","WYŁ." to "pompa_wyl","AUTO" to "pompa_auto").forEach{(t,c)->val b=action(t,C.surface2,Color.WHITE,0);b.setOnClickListener{sendCommand(c)};box.addView(b,lp(-1,48,7))};box.addView(label("Strategia: ${when(d?.optInt("wybor")){1->"Trociniak";2->"Kopciuch";3->"Automatyczny";else->"—"}}",12f,C.textDim,false,dp(10)));showSheet("Pompa",box)}
    private fun showServoMenu(){
      val d=lastStatusData;
      val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL};
      box.addView(label("TRYB",9.5f,C.textDim,true));
      box.addView(label(when(d?.optInt("tryb_serwa")){1->"AUTO";2->"RĘCZNY";3->"BEZPIECZNY";else->"—"},19f,Color.WHITE,true,dp(3)));
      val modeButtons=listOf("AUTO" to "tryb_auto","RĘCZNY" to "tryb_reczny","BEZPIECZNA" to "tryb_bezpieczny")
      modeButtons.forEach{(t,c)->
        val isActive=(when(d?.optInt("tryb_serwa")){1->"AUTO";2->"RĘCZNY";3->"BEZPIECZNA";else->""})==t
        val b=action(t,if(isActive) C.cyan else C.surface2,if(isActive) C.bg else Color.WHITE,0)
        b.setOnClickListener{sendCommand(c);Toast.makeText(this@MainActivity,"Tryb: $t",Toast.LENGTH_SHORT).show()}
        box.addView(b,lp(-1,48,7))
      };
      // Fix: Firmware sends degrees (0-180 for klapa, 0-90 for syberek), convert to percent
      val klapaDeg = d?.optInt("klapa",0)?:0
      val klapaPct = (klapaDeg * 100 / 180).coerceIn(0,100)
      addServoSlider(box,"Klapa",klapaPct,"klapa");
      val syberkaDeg = d?.optInt("syberka",0)?:0
      val syberkaPct = (syberkaDeg * 100 / 90).coerceIn(0,100)
      addServoSlider(box,"Syberek",syberkaPct,"syberek");
      showSheet("Serwo",box)
    }
    private fun addServoSlider(box:LinearLayout,name:String,initial:Int,command:String){
      val seek=SeekBar(this).apply{max=100;progress=initial.coerceIn(0,100)}
      val valTxt=label("$name: ${seek.progress}%",12.5f,Color.WHITE,true,dp(9))
      var lastSent=-1
      // Auto-send on release — no extra button needed (Home Assistant / Tado / Netatmo pattern)
      seek.setOnSeekBarChangeListener(object:SeekBar.OnSeekBarChangeListener{
        override fun onProgressChanged(s:SeekBar?,p:Int,f:Boolean){valTxt.text="$name: $p%"}
        override fun onStartTrackingTouch(s:SeekBar?){}
        override fun onStopTrackingTouch(s:SeekBar?){
          val v=seek.progress
          if(v!=lastSent){
            lastSent=v
            sendCommand("$command $v")
            Toast.makeText(this@MainActivity,"$name → $v%",Toast.LENGTH_SHORT).show()
          }
        }
      })
      box.addView(valTxt);box.addView(seek)
    }
    private fun showMixerMenu(){val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL};listOf("WŁ." to "mieszadlo_wl","WYŁ." to "mieszadlo_wyl","AUTO" to "mieszadlo_auto").forEach{(t,c)->val b=action(t,C.surface2,Color.WHITE,0);b.setOnClickListener{sendCommand(c)};box.addView(b,lp(-1,48,7))};showSheet("Mieszadło",box)}
    private fun showSmokeMenu(){
        val d=lastStatusData
        val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(4),dp(4),dp(4),dp(10))}
        box.addView(label("ODCZYT ADC",9.5f,C.textDim,true))
        box.addView(label(d?.optInt("dym",0)?.toString() ?: "—",20f,Color.WHITE,true,dp(3)))
        box.addView(label(if(d?.optBoolean("dym_alarm") == true)"ALARM AKTYWNY" else if(d?.optBoolean("dym_wlaczony") == true)"Aktywny — brak alarmu" else "Czujnik wyłączony (piec zimny)",11f,if(d?.optBoolean("dym_alarm")==true)C.err else if(d?.optBoolean("dym_wlaczony")==true)C.live else C.textDim,false,dp(4)))

        box.addView(label("PRÓG ALARMU ADC",9f,C.textDim,true,dp(10)))
        val progRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val progInp=EditText(this).apply{setText((d?.optInt("progAlarmDym",2000)?:2000).toString());setTextColor(Color.WHITE);setBackgroundColor(0x22FFFFFF.toInt());setPadding(dp(10),dp(8),dp(10),dp(8));inputType=android.text.InputType.TYPE_CLASS_NUMBER}
        progRow.addView(progInp,LinearLayout.LayoutParams(0,-2,1f))
        val progBtn=action("USTAW",C.cyan,C.bg,0);progBtn.setOnClickListener{sendCommand("ustaw progAlarmDym "+progInp.text.toString())};progRow.addView(progBtn,lp(-1,42))
        box.addView(progRow,lp(-1,-2,4))

        box.addView(label("TEMP. AKTYWACJI CZUJNIKA °C",9f,C.textDim,true,dp(10)))
        val tempRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL}
        val tempInp=EditText(this).apply{setText((d?.optInt("dymProgTemp",40)?:40).toString());setTextColor(Color.WHITE);setBackgroundColor(0x22FFFFFF.toInt());setPadding(dp(10),dp(8),dp(10),dp(8));inputType=android.text.InputType.TYPE_CLASS_NUMBER}
        tempRow.addView(tempInp,LinearLayout.LayoutParams(0,-2,1f))
        val tempBtn=action("USTAW",C.cyan,C.bg,0);tempBtn.setOnClickListener{sendCommand("ustaw dymProgTemp "+tempInp.text.toString())};tempRow.addView(tempBtn,lp(-1,42))
        box.addView(tempRow,lp(-1,-2,4))

        box.addView(label("TRYB PRACY",9f,C.textDim,true,dp(10)))
        val segRow=LinearLayout(this).apply{orientation=LinearLayout.HORIZONTAL}
        val curTryb=d?.optInt("dymTrybPracy",2)?:2
        mapOf(0 to "Impulsowo",1 to "Ciągle",2 to "Auto").forEach{(v,n)->
            val b=action(n,if(v==curTryb)C.cyan else C.surface2,if(v==curTryb)C.bg else Color.WHITE,0)
            b.setOnClickListener{sendCommand("ustaw dymTrybPracy $v")};segRow.addView(b,LinearLayout.LayoutParams(0,-1,1f).apply{marginStart=dp(4)})
        }
        box.addView(segRow,lp(-1,42,6))

        box.addView(label("AKCJE",9f,C.textDim,true,dp(14)))
        listOf("WYCISZ BRZĘCZYK" to "wycisz","RESET ALARMÓW" to "reset_alarmow").forEach{(t,c)->
            val b=action(t,0xFF3A1820.toInt(),0xFFFFB2BE.toInt(),0);b.setOnClickListener{sendCommand(c)};box.addView(b,lp(-1,48,6))
        }
        showSheet("Czujnik dymu",box)
    }
    private fun showClockMenu(){val box=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL};box.addView(label("CZAS TELEFONU",9.5f,C.textDim,true));box.addView(label(SimpleDateFormat("dd.MM.yyyy HH:mm:ss",Locale.US).format(Date()),20f,Color.WHITE,true,dp(3)));box.addView(label("Sesja zapamiętana bez hasła",12f,C.textDim,false,dp(10)));showSheet("Data i czas",box)}
    private fun showModule(name: String) {
        when (name) {
            "Logi" -> showLogsMenu()
            "Terminal" -> showTerminalMenu()
            "OTA" -> showOtaMenu()
            "Sesja" -> showSessionMenu()
            else -> Toast.makeText(this, name, Toast.LENGTH_SHORT).show()
        }
    }

    private fun showWeatherMenu() {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val location = label("51.066389° N · 21.509167° E · Open-Meteo", 9.0f, C.textDim, false)
        body.addView(location)
        val current = label("Pobieram warunki…", 22f, Color.WHITE, true, dp(10))
        body.addView(current)
        val details = label("Temperatura · odczuwalna · wiatr · zachmurzenie · opad", 9.6f, C.textDim, false, dp(4))
        body.addView(details)
        val forecast = label("Prognoza godzinowa pojawi się po odczycie.", 10.0f, C.text, false, dp(14))
        body.addView(forecast)
        val reload = action("ODŚWIEŻ", C.surface2, Color.WHITE, 0)
        reload.setOnClickListener {
            current.text = "Pobieram warunki…"
            executor.execute {
                try {
                    val json = getPublicJson("https://api.open-meteo.com/v1/forecast?latitude=51.066389&longitude=21.509167&current=temperature_2m,apparent_temperature,precipitation,cloud_cover,wind_speed_10m,wind_gusts_10m,wind_direction_10m,uv_index&hourly=temperature_2m,precipitation_probability&forecast_days=2&timezone=Europe%2FWarsaw")
                    val c = json?.optJSONObject("current")
                    val temp = c?.optDouble("temperature_2m", Double.NaN) ?: Double.NaN
                    val feels = c?.optDouble("apparent_temperature", Double.NaN) ?: Double.NaN
                    val wind = c?.optDouble("wind_speed_10m", Double.NaN) ?: Double.NaN
                    val gust = c?.optDouble("wind_gusts_10m", Double.NaN) ?: Double.NaN
                    val cloud = c?.optDouble("cloud_cover", Double.NaN) ?: Double.NaN
                    val rain = c?.optDouble("precipitation", Double.NaN) ?: Double.NaN
                    val uv = c?.optDouble("uv_index", Double.NaN) ?: Double.NaN
                    val time = c?.optString("time", "—") ?: "—"
                    runOnUiThread {
                        current.text = if (temp.isFinite()) String.format(Locale.US, "%.1f °C", temp) else "—"
                        details.text = "Odczuwalna ${if (feels.isFinite()) String.format(Locale.US, "%.1f °C", feels) else "—"}  ·  wiatr ${if (wind.isFinite()) String.format(Locale.US, "%.0f km/h", wind) else "—"}  ·  porywy ${if (gust.isFinite()) String.format(Locale.US, "%.0f km/h", gust) else "—"}"
                        forecast.text = "Chmury ${if (cloud.isFinite()) String.format(Locale.US, "%.0f%%", cloud) else "—"}  ·  opad ${if (rain.isFinite()) String.format(Locale.US, "%.1f mm", rain) else "—"}  ·  UV ${if (uv.isFinite()) String.format(Locale.US, "%.1f", uv) else "—"}  ·  ${time.replace('T', ' ')}"
                    }
                } catch (e: Exception) {
                    runOnUiThread { current.text = "Brak połączenia"; details.text = e.message ?: "Błąd Open-Meteo" }
                }
            }
        }
        body.addView(reload, lp(-1, 46, 12))
        showSheet("Pogoda", body)
        reload.performClick()
    }

    private fun showLogsMenu() {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val age = if (lastStatusAtMs > 0L) ((System.currentTimeMillis() - lastStatusAtMs) / 1000L) else Long.MAX_VALUE
        body.addView(label("DIAGNOSTYKA APLIKACJI", 9f, C.cyan, true))
        body.addView(label("Połączenie: ${if (lastStatusOnline) "LIVE" else "OFFLINE"}", 16f, Color.WHITE, true, dp(5)))
        body.addView(label("Ostatni status: ${if (age == Long.MAX_VALUE) "brak" else "$age s temu"}", 10f, C.textDim, false, dp(3)))
        body.addView(label("Historia: ${tempChart.snapshot().size} próbek", 10f, C.textDim, false, dp(3)))
        body.addView(label("Sesja jest przechowywana szyfrowanie przez Android Keystore. Pełne logi sterownika pozostają po stronie pieca.", 10.2f, C.text, false, dp(12)))
        lastStatusData?.optString("ip")?.takeIf { it.isNotBlank() }?.let { ip ->
            val open = action("OTWÓRZ LOKALNY PANEL", C.surface2, Color.WHITE, 0)
            open.setOnClickListener { openLocalPanel(ip) }
            body.addView(open, lp(-1, 46, 10))
        }
        showSheet("Logi", body)
    }

    private fun showTerminalMenu() {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val meta = label("Remote Terminal · Firebase diagnostics", 9.3f, C.textDim, false)
        body.addView(meta)
        val console = TextView(this).apply {
            text = "Ładowanie ostatnich komunikatów…"
            textSize = 9.0f; setTextColor(C.text); typeface = Typeface.MONOSPACE
            setPadding(dp(10), dp(10), dp(10), dp(10)); background = rounded(0xFF07131F.toInt(), 14, C.border)
        }
        body.addView(console, lp(-1, 260, 10))
        val refresh = action("ODŚWIEŻ TERMINAL", C.surface2, Color.WHITE, 0)
        refresh.setOnClickListener {
            val saved = authStore.load()
            val uid = saved?.localId
            if (uid.isNullOrBlank()) { console.text = "Brak localId sesji Firebase."; return@setOnClickListener }
            executor.execute {
                try {
                    val base = "${FirebaseDevConfig.DATABASE_URL}/piec/devices/${java.net.URLEncoder.encode(uid, "UTF-8")}/telemetry/diagnostics"
                    val sessions = firebase.getJson("$base.json?shallow=true", ensureToken(false) ?: "")
                    val keys = sessions?.keys()?.asSequence()?.filter { it.matches(Regex("\\d+")) }?.toList()?.sortedByDescending { it.toLongOrNull() ?: 0L } ?: emptyList()
                    val session = keys.firstOrNull()
                    if (session == null) { runOnUiThread { console.text = "Brak sesji diagnostycznej." }; return@execute }
                    val chunks = firebase.getJson("$base/$session/chunks.json", ensureToken(false) ?: "")
                    val out = StringBuilder("SESJA $session\n")
                    val chunkList = mutableListOf<JSONObject>()
                    if (chunks != null) {
                        val iterator = chunks.keys()
                        while (iterator.hasNext()) {
                            val key = iterator.next()
                            chunks.optJSONObject(key)?.let { chunkList += it }
                        }
                    }
                    chunkList.sortBy { it.optLong("seq", 0L) }
                    val start = (chunkList.size - 80).coerceAtLeast(0)
                    for (i in start until chunkList.size) {
                        out.append(chunkList[i].optString("data", "")).append('\n')
                    }
                    runOnUiThread { console.text = out.toString().ifBlank { "Sesja bez danych." } }
                } catch (e: Exception) { runOnUiThread { console.text = "Błąd Terminala: ${e.message}" } }
            }
        }
        body.addView(refresh, lp(-1, 46, 8))
        showSheet("Terminal", body)
        refresh.performClick()
    }

    private fun showOtaMenu() {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(label("AKTUALIZACJA FIRMWARE", 9f, C.cyan, true))
        body.addView(label("Wersja aplikacji 0.29.0", 17f, Color.WHITE, true, dp(5)))
        body.addView(label("Bezpośrednie OTA wymaga połączenia z panelem pieca. Aplikacja nie oznacza aktualizacji jako wykonanej bez potwierdzenia.", 10.2f, C.text, false, dp(6)))
        lastStatusData?.optString("ip")?.takeIf { it.isNotBlank() }?.let { ip ->
            val open = action("OTWÓRZ PANEL PIECA", C.surface2, Color.WHITE, 0)
            open.setOnClickListener { openLocalPanel(ip) }
            body.addView(open, lp(-1, 46, 12))
        }
        showSheet("OTA", body)
    }

    private fun showSessionMenu() {
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val s = authStore.load()
        body.addView(label("AKTYWNA SESJA", 9f, C.cyan, true))
        body.addView(label(s?.email ?: "—", 15f, Color.WHITE, true, dp(5)))
        val age = if (lastStatusAtMs > 0L) ((System.currentTimeMillis() - lastStatusAtMs) / 1000L) else -1L
        body.addView(label("Status: ${if (lastStatusOnline) "LIVE" else "OFFLINE"}  ·  ostatni odczyt: ${if (age >= 0) "$age s" else "—"}", 10.2f, C.textDim, false, dp(4)))
        body.addView(label("Hasło nie jest zapisywane. Refresh token jest przechowywany szyfrowanie.", 10.2f, C.text, false, dp(12)))
        val out = action("WYLOGUJ Z TEGO URZĄDZENIA", 0x2A4A1820, 0xFFFFA7B4.toInt(), 0)
        out.setOnClickListener { dismissSheet(); logout() }
        body.addView(out, lp(-1, 46, 10))
        showSheet("Sesja", body)
    }

    private fun openLocalPanel(ip: String) {
        runCatching { startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse("http://$ip/"))) }
            .onFailure { Toast.makeText(this, "Nie można otworzyć panelu: ${it.message}", Toast.LENGTH_SHORT).show() }
    }

    private fun getPublicJson(url: String): JSONObject? {
        val c = URL(url).openConnection() as HttpURLConnection
        c.requestMethod = "GET"; c.connectTimeout = 10000; c.readTimeout = 15000
        val stream = if (c.responseCode in 200..299) c.inputStream else c.errorStream
        val text = stream?.use { input -> BufferedReader(InputStreamReader(input, Charsets.UTF_8)).use { it.readText() } } ?: ""
        if (c.responseCode !in 200..299) throw IllegalStateException("HTTP ${c.responseCode}")
        return if (text.isBlank() || text == "null") null else JSONObject(text)
    }

    // Fix: Command rate limiting — prevent spamming
    private var lastCommandAtMs = 0L
    private val commandCooldownMs = 2000L

    private fun sendCommand(command:String){
        // Fix: Rate limit commands to prevent flooding Firebase/ESP32
        val now = System.currentTimeMillis()
        if (now - lastCommandAtMs < commandCooldownMs) {
            val remaining = (commandCooldownMs - (now - lastCommandAtMs)) / 1000
            Toast.makeText(this, "Poczekaj ${remaining}s przed następną komendą", Toast.LENGTH_SHORT).show()
            return
        }
        lastCommandAtMs = now
        // Commands that change safety thresholds or inject fake sensor values need an explicit confirmation.
        val risky=command.startsWith("ustaw ")||command.startsWith("symuluj")
        if(!risky){dispatchCommand(command);return}
        android.app.AlertDialog.Builder(this)
            .setTitle("Potwierdź polecenie")
            .setMessage("To polecenie zmienia zachowanie sterownika:\n\n$command\n\nWysłać?")
            .setNegativeButton("Anuluj",null)
            .setPositiveButton("Wyślij"){_,_->dispatchCommand(command)}
            .show()
    }

    private fun dispatchCommand(command:String){
        val body=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        body.addView(label("Polecenie sterujące",8.8f,C.textDim2,true))
        body.addView(label(command,15f,Color.WHITE,true,dp(3)))
        val status=label("PRZYGOTOWANIE",12.2f,C.cyan,true,dp(16))
        val detail=label("Komenda zostanie wysłana do skrzynki sterownika.",10f,C.textDim,false,dp(4))
        body.addView(status)
        body.addView(detail)
        val timeline=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(0,dp(14),0,0)}
        fun step(name:String,value:String="OCZEKUJE"):TextView{val row=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.HORIZONTAL;gravity=Gravity.CENTER_VERTICAL;setPadding(dp(10),dp(8),dp(10),dp(8));background=rounded(C.surface2,14,C.border)};row.addView(label("•",15f,C.textDim2,true),lp(24,24));val cp=LinearLayout(this@MainActivity).apply{orientation=LinearLayout.VERTICAL};cp.addView(label(name,10.7f,Color.WHITE,true));val v=label(value,8.7f,C.textDim2,true,dp(2));cp.addView(v);row.addView(cp,LinearLayout.LayoutParams(0,-2,1f).apply{marginStart=dp(8)});timeline.addView(row,lp(-1,-2,6));return v}
        val s1=step("WYSŁANO")
        val s2=step("OCZEKIWANIE NA ACK")
        val s3=step("WYNIK")
        body.addView(timeline)
        val done=action("ZAMKNIJ",C.surface2,C.text,0).apply{setOnClickListener{dismissSheet()}}
        body.addView(done,lp(-1,46,9))
        showSheet("Polecenie",body)
        commandExecutor.execute{
            try{
                runOnUiThread{status.text="WYSYŁANIE…";s1.text="AKTYWNE";s1.setTextColor(C.cyan);detail.text="Łączę z /piec/cmd…"}
                val token=ensureToken(false)?:throw IllegalStateException("Brak sesji")
                val id=commands.send(token,command)
                runOnUiThread{status.text="OCZEKIWANIE NA ACK";s1.text="OK";s1.setTextColor(C.live);s2.text="AKTYWNE";s2.setTextColor(C.cyan);detail.text="cmdId: $id"}
                var ack:FirebaseCommandRest.Ack?=null
                val started=System.currentTimeMillis()
                while(ack==null&&System.currentTimeMillis()-started<30000){
                    Thread.sleep(1200)
                    ack=runCatching{commands.readAck(ensureToken(false)?:token,id)}.getOrNull()
                }
                runOnUiThread{
                    if(ack?.ok==true){
                        status.text="ACK POTWIERDZONE"
                        status.setTextColor(C.live)
                        s1.text="OK";s1.setTextColor(C.live)
                        s2.text="OK";s2.setTextColor(C.live)
                        s3.text="POTWIERDZONE";s3.setTextColor(C.live)
                        detail.text="Sterownik potwierdził przyjęcie polecenia."
                        root.performHapticFeedback(HapticFeedbackConstants.CONFIRM)
                        // Success flash animation
                        status.animate().alpha(0.4f).setDuration(100).withEndAction {
                            status.animate().alpha(1f).setDuration(200).start()
                        }.start()
                        // Auto-close after success (like Home Assistant / Tado / Netatmo)
                        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({
                            dismissSheet()
                        }, 1100)
                    } else{
                        status.text=if(ack!=null)"POLECENIE ODRZUCONE" else "BRAK POTWIERDZENIA"
                        status.setTextColor(C.warn)
                        s2.text=if(ack!=null)"OK" else "TIMEOUT";s2.setTextColor(C.warn)
                        s3.text=if(ack!=null)"ODRZUCONE" else "NIE POTWIERDZONO";s3.setTextColor(C.warn)
                        detail.text=if(ack!=null)"Sterownik odrzucił polecenie: ${ack?.error?:"brak opisu błędu"}" else "Nie pokazuję sukcesu bez ACK."
                        root.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)
                    }
                }
            }catch(e:Exception){runOnUiThread{status.text="BŁĄD";status.setTextColor(C.err);s1.text="BŁĄD";s1.setTextColor(C.err);s2.text="ANULOWANE";s3.text="NIE WYKONANO";s3.setTextColor(C.err);detail.text=e.message?:"Nie udało się wysłać polecenia.";root.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS)}}
        }
    }

    private fun chartReadout(chart: ProfessionalTelemetryChartView, empty: String): String {
        val lines = chart.readouts()
        return if (lines.isEmpty()) empty else lines.take(4).joinToString("   ·   ")
    }

    private fun chartMode(chart: ProfessionalTelemetryChartView)=chart.currentMode()
    private fun shortMode(chart: ProfessionalTelemetryChartView)=when(chart.currentMode()){ProfessionalTelemetryChartView.Mode.SPLIT->"WIDOK: PODZIELONE";ProfessionalTelemetryChartView.Mode.COMMON->"WIDOK: WSPÓLNA";ProfessionalTelemetryChartView.Mode.NORMALIZED->"WIDOK: 0–100%";ProfessionalTelemetryChartView.Mode.FROM_START->"WIDOK: OD STARTU";ProfessionalTelemetryChartView.Mode.XY->"WIDOK: XY"}

    private fun tempSeries()=listOf(
        ProfessionalTelemetryChartView.Series("zewn","Zewn.",0,false,.1,"°C", C.blue),
        ProfessionalTelemetryChartView.Series("bojler","Bojler",1,false,.1,"°C", C.orange),
        ProfessionalTelemetryChartView.Series("ogrz","Ogrz.",2,false,.1,"°C", C.red),
        ProfessionalTelemetryChartView.Series("ogrzsr","Ogrz. śr.",3,false,.1,"°C", C.yellow),
        ProfessionalTelemetryChartView.Series("powrot","Powrót",4,false,.1,"°C", C.violet),
        ProfessionalTelemetryChartView.Series("panel","Panel",5,false,.1,"°C", C.yellow),
        ProfessionalTelemetryChartView.Series("pokoj","Pokój",6,false,.1,"°C", C.purple),
        ProfessionalTelemetryChartView.Series("trociny","Trociny",7,false,.1,"°C", C.coral),
        ProfessionalTelemetryChartView.Series("rh","Wilgotność",8,false,.1,"%", 0xFF64C7FF.toInt()),
        ProfessionalTelemetryChartView.Series("cis","Ciśnienie",9,false,1.0,"hPa", C.green)
    )
    private fun servoSeries()=listOf(
        ProfessionalTelemetryChartView.Series("servo_flap","Klapa",null,true,1.0,"%", C.pink),
        ProfessionalTelemetryChartView.Series("servo_damper","Syberek",null,true,1.0,"%", C.green)
    )

    private fun valueFor(d:JSONObject?,key:String,u:String)=d?.optDouble(key,Double.NaN)?.takeIf{it.isFinite()}?.let{"${String.format(Locale.US,"%.1f",it)} $u"} ?: "—"
    private fun numberFrom(d:JSONObject?,k:String,f:Double)=d?.optDouble(k,Double.NaN)?.takeIf{it.isFinite()}?:f
    private fun sliderValue(sb:SeekBar,min:Float,step:Float)=String.format(Locale.US,"%.1f",min+sb.progress*step)
    private fun formatSlider(p:Int,min:Float,step:Float,u:String)=if(step>=1f)"${(min+p*step).toInt()} $u" else "${String.format(Locale.US,"%.1f",min+p*step)} $u"

    private fun edit(hintText:String,password:Boolean)=EditText(this).apply{hint=hintText;setTextColor(Color.WHITE);setHintTextColor(C.textDim2);inputType=if(password)0x81 else 33;isSingleLine=true;background=rounded(C.bg,14,C.border);setPadding(dp(15),0,dp(15),0);textSize=13.5f}
    private fun statusCell(name:String,value:String):Pair<View,TextView>{val cell=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(11),dp(8),dp(9),dp(8));background=rounded(C.surface2,15,C.border)};cell.addView(label(name,8.4f,C.textDim,true));val v=label(value,14.5f,Color.WHITE,true,dp(3)).apply{isSingleLine=true;ellipsize=TextUtils.TruncateAt.END};cell.addView(v);return cell to v}
    private fun pill(text:String,bg:Int,fg:Int,w:Int=0)=label(text,9.8f,fg,true).apply{gravity=Gravity.CENTER;background=rounded(bg,20,C.border);if(w>0)minimumWidth=dp(w);setPadding(dp(8),0,dp(8),0)}
    private fun selector(text:String,on:Boolean,w:Int=0)=chartChip(text,on).apply{minimumWidth=if(w>0)dp(w) else 0}
    private fun updateSelector(v:TextView,on:Boolean){v.isSelected=on;v.setTextColor(if(on)C.cyan else C.textDim);v.background=if(on)rounded(0x2236D8F3,13,0xB036D8F3.toInt()) else rounded(C.surface2,13,C.border)}
    private fun iconAction(t:String)=action(t,C.surface2,C.text,0)
    private fun iconButton(iconType: NativeIconView.Icon, description: String, on: () -> Unit): FrameLayout = FrameLayout(this).apply {
        contentDescription = description
        background = pressableBackground(C.surface2, C.border, 13)
        isClickable = true; isFocusable = true
        foreground = RippleDrawable(ColorStateList.valueOf(0x38FFFFFF), null, null)
        setOnTouchListener { v,e -> when(e.actionMasked){android.view.MotionEvent.ACTION_DOWN->{v.animate().scaleX(.94f).scaleY(.94f).setDuration(70).start();false};android.view.MotionEvent.ACTION_UP,android.view.MotionEvent.ACTION_CANCEL->{v.animate().scaleX(1f).scaleY(1f).setDuration(110).start();false};else->false} }
        setOnClickListener { performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY); on() }
        addView(NativeIconView(this@MainActivity).apply { icon = iconType; tint = C.text; active = false }, FrameLayout.LayoutParams(-1, -1))
    }
    private fun action(t: String, bg: Int, fg: Int, w: Int = 0): TextView = TextView(this).apply { text=t;textSize=10.0f;setTextColor(fg);typeface=Typeface.create("sans-serif",Typeface.BOLD);gravity=Gravity.CENTER;background=pressableBackground(bg,C.border,14);foreground=RippleDrawable(ColorStateList.valueOf(0x3AFFFFFF),null,null);isClickable=true;isFocusable=true;minimumWidth=if(w>0)dp(w) else 0;minHeight=dp(46);setPadding(dp(10),0,dp(10),0);setOnTouchListener{v,e->when(e.actionMasked){android.view.MotionEvent.ACTION_DOWN->{v.animate().scaleX(.97f).scaleY(.97f).setDuration(70).start();false};android.view.MotionEvent.ACTION_UP,android.view.MotionEvent.ACTION_CANCEL->{v.animate().scaleX(1f).scaleY(1f).setDuration(110).start();v.performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY);false};else->false}}}

    private fun label(t: String, size: Float, color: Int, bold: Boolean, top: Int = 0): TextView = TextView(this).apply {
        text=t; textSize=size; setTextColor(color)
        typeface=Typeface.create("sans-serif",if(bold)Typeface.BOLD else Typeface.NORMAL)
        includeFontPadding=false
        if(top>0)setPadding(0,top,0,0)
    }
    private fun rounded(color:Int,r:Int,stroke:Int)=GradientDrawable().apply{
        setColor(color); cornerRadius=dp(r).toFloat()
        if(stroke!=0)setStroke(dp(1),stroke)
    }
    private fun blendColor(base:Int, overlay:Int, amount:Float):Int {
        val a=Color.alpha(base); val r=(Color.red(base)+(Color.red(overlay)-Color.red(base))*amount).roundToInt().coerceIn(0,255)
        val g=(Color.green(base)+(Color.green(overlay)-Color.green(base))*amount).roundToInt().coerceIn(0,255)
        val b=(Color.blue(base)+(Color.blue(overlay)-Color.blue(base))*amount).roundToInt().coerceIn(0,255)
        return Color.argb(a,r,g,b)
    }
    private fun pressableBackground(color:Int,stroke:Int,r:Int): StateListDrawable = StateListDrawable().apply {
        addState(intArrayOf(android.R.attr.state_pressed), rounded(blendColor(color, Color.WHITE, 0.08f), r, stroke))
        addState(intArrayOf(android.R.attr.state_selected), rounded(blendColor(color, Color.WHITE, 0.06f), r, stroke))
        addState(intArrayOf(), rounded(color, r, stroke))
    }

    private fun roundedGradient(start: Int, end: Int, stroke1: Int, stroke2: Int, r: Int): GradientDrawable {
        return GradientDrawable(GradientDrawable.Orientation.TL_BR, intArrayOf(start, end)).apply {
            cornerRadius = dp(r).toFloat()
            setStroke(dp(1), stroke1)
        }
    }

    private fun navBg(on:Boolean)=rounded(
        if(on) Color.argb(34, 38, 211, 241) else Color.TRANSPARENT,
        15,
        if(on) Color.argb(175, 38, 211, 241) else Color.argb(22, 82, 118, 145)
    )
    private fun installInsets(shell:View){shell.setOnApplyWindowInsetsListener{_,insets->if(Build.VERSION.SDK_INT>=30){val b=insets.getInsets(WindowInsets.Type.systemBars());shell.setPadding(0,b.top,0,b.bottom)}else shell.setPadding(0,insets.systemWindowInsetTop,0,insets.systemWindowInsetBottom);insets};shell.requestApplyInsets()}
    private fun lp(w:Int,h:Int,margin:Int=0)=LinearLayout.LayoutParams(if(w<0)w else dp(w),if(h<0)h else dp(h)).apply{if(margin>0)setMargins(0,dp(margin),0,0)}
    /** Width/height in dp plus a START margin - for children of HORIZONTAL rows (lp() margin is a TOP margin). */
    private fun lpS(w:Int,h:Int,start:Int)=LinearLayout.LayoutParams(dp(w),dp(h)).apply{marginStart=dp(start)}
    private fun lp(w:Int,h:Int,weight:Float)=LinearLayout.LayoutParams(0,if(h<0)h else dp(h),weight)
    private fun dp(v:Int)=(v*resources.displayMetrics.density).toInt()
    private fun dp(v:Float)=v*resources.displayMetrics.density
    private fun fmtTs(ts:Long)=SimpleDateFormat("dd.MM HH:mm",Locale.US).apply{timeZone=TimeZone.getTimeZone("Europe/Warsaw")}.format(Date(ts*1000))
    private fun kindFor(id:TileId)=DashboardTileView.Kind.values()[id.ordinal]
    private fun accentFor(id:TileId)=intArrayOf(C.orange, C.red, C.orange, C.yellow, C.purple, C.green, C.cyan, C.cyan, C.pink, C.green, C.err, C.orange, 0xFFB7C4D4.toInt())[id.ordinal]

    override fun onStart() {
        super.onStart()
        appVisible = true
        // Fix: Battery drain — restart animations when returning to foreground
        heroBoiler.startAnimations()
        tileViews.values.forEach { it.startAnimations() }
        // Refresh immediately on return instead of showing old values as STALE for a few seconds.
        if (pollingStarted && !sessionNeedsLogin) executor.execute { pollStatusOnce() }
    }
    override fun onStop() {
        appVisible = false
        // Fix: Battery drain — stop all animations when app goes to background
        heroBoiler.stopAnimations()
        tileViews.values.forEach { it.stopAnimations() }
        super.onStop()
    }
    override fun onDestroy() {
        dismissSheet()
        // Fix: Memory leak — shutdown external service thread pools
        WeatherService.shutdown()
        TelemetryCache.shutdown()
        scheduler.shutdownNow()
        executor.shutdownNow()
        historyExecutor.shutdownNow()
        commandExecutor.shutdownNow()
        super.onDestroy()
    }

    private object C {
        // Premium palette — deeper blues, refined accents
        const val bg=0xFF060D18.toInt(); const val top=0xFF071421.toInt(); const val nav=0xFF081421.toInt()
        const val surface=0xFF0C1829.toInt(); const val surface2=0xFF111F35.toInt(); const val hero=0xFF0B2032.toInt()
        const val border=0x36506D84; const val borderStrong=0x4A5E829B
        const val text=0xFFE6F0F7.toInt(); const val textDim=0xFF8EA6BA.toInt(); const val textDim2=0xFF5A7A99.toInt()
        const val cyan=0xFF00D4F5.toInt(); const val accent=0xFFFF9F43.toInt(); const val accentSoft=0x26FFA23F
        const val live=0xFF4ADE80.toInt(); const val liveBg=0x1C4ADE80; const val warn=0xFFFBBF24.toInt(); const val err=0xFFFF5F78.toInt()
        // Chart palette
        const val blue=0xFF55D7FF.toInt(); const val red=0xFFFF5E67.toInt(); const val orange=0xFFFFB04A.toInt()
        const val yellow=0xFFFFD166.toInt(); const val violet=0xFFB28CFF.toInt(); const val pink=0xFFFF6FC7.toInt()
        const val green=0xFF45D98B.toInt(); const val purple=0xFFA879FF.toInt(); const val coral=0xFFFF8B67.toInt()
    }
    companion object { private const val TOKEN_REFRESH_SKEW_MS=2*60*1000L }
}
