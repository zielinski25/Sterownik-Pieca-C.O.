package com.sterownikco.pro.core

import java.util.Locale

/** Builds readable Polish labels for confirmations without exposing command identifiers or secrets. */
internal object CommandRedactor {
    private data class SettingLabel(
        val name: String,
        val unitSuffix: String = "",
        val choices: Map<String, String> = emptyMap(),
        val isBoolean: Boolean = false,
        val enabledValue: String = "Włączone",
        val disabledValue: String = "Wyłączone"
    )

    private val whitespace = Regex("\\s+")

    private val settingLabels = mapOf(
        "wybor" to SettingLabel(
            "Tryb pracy pompy",
            choices = mapOf("1" to "Trociniak", "2" to "Kopciuch", "3" to "Automatyczny")
        ),
        "czason" to SettingLabel("Czas pracy pompy", " min"),
        "czasoff" to SettingLabel("Czas postoju pompy", " min"),
        "tempon" to SettingLabel("Temperatura załączenia pompy", " °C"),
        "tempoff" to SettingLabel("Temperatura wyłączenia pompy", " °C"),
        "antystopwlaczony" to SettingLabel(
            "Funkcja Antystop", isBoolean = true, enabledValue = "Włączona", disabledValue = "Wyłączona"
        ),
        "antystopdni" to SettingLabel("Czas bezruchu przed uruchomieniem Antystopu", " dni"),
        "trybserwa" to SettingLabel(
            "Tryb serwa",
            choices = mapOf("1" to "Automatyczny", "2" to "Ręczny", "3" to "Bezpieczna")
        ),
        "tempzadservo" to SettingLabel("Temperatura zadana serwa", " °C"),
        "histerservo" to SettingLabel("Histereza serwa", " °C"),
        "skokklapy" to SettingLabel("Skok klapy", "%"),
        "skoksyberka" to SettingLabel("Skok syberka", "%"),
        "odchyltemp" to SettingLabel("Odchylenie przyspieszające", " °C"),
        "mnoznik" to SettingLabel("Mnożnik korekty kroku", "×"),
        "mieszadlowlaczony" to SettingLabel("Mieszadło", isBoolean = true),
        "mieszadloczason" to SettingLabel("Czas pracy mieszadła", " s"),
        "mieszadloczasoff" to SettingLabel("Przerwa pracy mieszadła", " min"),
        "progalarmtemp" to SettingLabel("Próg alarmu przegrzania", " °C"),
        "progalarmdym" to SettingLabel("Próg alarmu czujnika dymu", " ADC"),
        "dymprogtemp" to SettingLabel("Temperatura aktywacji czujnika dymu", " °C"),
        "dymtrybpracy" to SettingLabel(
            "Tryb czujnika dymu",
            choices = mapOf("0" to "Impulsowy", "1" to "Ciągły")
        ),
        "dymczason" to SettingLabel("Czas pracy czujnika dymu", " s"),
        "dymczasoff" to SettingLabel("Przerwa między pomiarami czujnika dymu", " s"),
        "dymczasstabilizacji" to SettingLabel("Czas stabilizacji czujnika dymu", " s")
    )

    private val fixedCommandLabels = mapOf(
        "pompa_wl" to "Pompa CO: ręczne włączenie",
        "pompa_wyl" to "Pompa CO: ręczne wyłączenie",
        "pompa_auto" to "Pompa CO: powrót do automatyki",
        "mieszadlo_wl" to "Mieszadło: ręczne włączenie",
        "mieszadlo_wyl" to "Mieszadło: ręczne wyłączenie",
        "mieszadlo_auto" to "Mieszadło: powrót do automatyki",
        "tg_test" to "Wysłanie testowego raportu Telegram",
        "rtc_sync" to "Synchronizacja zegara centrali",
        "update" to "Aktualizacja oprogramowania centrali",
        "update_panel" to "Aktualizacja panelu pokojowego LCD",
        "ota_upload" to "Aktualizacja oprogramowania z pliku"
    )

    private val dlogCategoryLabels = mapOf(
        "system" to "system",
        "boot" to "uruchamianie",
        "sensor" to "czujniki",
        "wifi" to "sieć Wi-Fi",
        "firebase" to "połączenie z Firebase",
        "telemetry" to "telemetria",
        "historia" to "historia pomiarów",
        "spool" to "kolejka logów",
        "ota" to "aktualizacje sterownika",
        "terminal" to "terminal",
        "alarm" to "alarmy",
        "servo" to "serwa",
        "web" to "panel WWW",
        "weather" to "pogoda",
        "sd" to "karta pamięci",
        "memory" to "pamięć",
        "watchdog" to "nadzór pracy sterownika",
        "logi" to "pozostałe logi",
        "scheduler" to "harmonogram",
        "pompa" to "pompa",
        "telegram" to "Telegram"
    )

    private val dlogLevelLabels = mapOf(
        "trace" to "najbardziej szczegółowy",
        "debug" to "diagnostyczny",
        "info" to "informacyjny",
        "warn" to "ostrzeżenia",
        "err" to "błędy"
    )

    fun isSensitive(command: String): Boolean {
        val verb = command.trimStart().takeWhile { !it.isWhitespace() }
        return verb.equals("tg_config", ignoreCase = true)
    }

    /** Returns only a human-facing label; unknown commands never fall back to raw command text. */
    fun forDisplay(command: String): String {
        val trimmed = command.trim()
        if (trimmed.isEmpty()) return ""
        if (isSensitive(trimmed)) return "Konfiguracja Telegrama"

        val parts = trimmed.split(whitespace)
        return when (parts.first().lowercase(Locale.ROOT)) {
            "ustaw" -> displaySetting(parts)
            "klapa" -> displayServoPosition("klapy", parts.getOrNull(1))
            "syberek" -> displayServoPosition("syberka", parts.getOrNull(1))
            "symuluj" -> displaySensorSimulation(parts)
            "symuluj_stop" -> displaySensorSimulationStop(parts.getOrNull(1))
            "diag" -> displayDiagnostic(parts)
            else -> fixedCommandLabels[parts.first().lowercase(Locale.ROOT)] ?: "Polecenie sterownika"
        }
    }

    private fun displaySetting(parts: List<String>): String {
        val key = parts.getOrNull(1)?.lowercase(Locale.ROOT) ?: return "Zmiana ustawień sterownika"
        val setting = settingLabels[key] ?: return "Zmiana ustawień sterownika"
        val rawValue = parts.getOrNull(2) ?: return setting.name
        val value = rawValue.lowercase(Locale.ROOT)
        val displayValue = setting.choices[value] ?: when {
            setting.choices.isNotEmpty() -> rawValue.toIntOrNull()?.toString() ?: return setting.name
            setting.isBoolean -> when (value) {
                "1", "true", "on", "wl", "wlaczony", "włączony" -> setting.enabledValue
                "0", "false", "off", "wyl", "wylaczony", "wyłączony" -> setting.disabledValue
                else -> return setting.name
            }
            setting.unitSuffix.isNotEmpty() -> {
                val number = rawValue.toDoubleOrNull()
                if (number == null || !number.isFinite()) return setting.name
                rawValue + setting.unitSuffix
            }
            else -> return setting.name
        }
        return "${setting.name}: $displayValue"
    }

    private fun displayServoPosition(name: String, rawValue: String?): String {
        val value = rawValue?.toIntOrNull()
        return if (value != null) {
            "Pozycja $name: $value%"
        } else {
            "Ustawienie pozycji $name"
        }
    }

    private fun displaySensorSimulation(parts: List<String>): String {
        val sensorId = parts.getOrNull(1)
        val sensor = SensorDef.ALL.firstOrNull { it.id.equals(sensorId, ignoreCase = true) }
            ?: return "Symulacja czujnika"
        val reading = parts.getOrNull(2) ?: return "Symulacja: ${sensor.name}"
        val number = reading.toDoubleOrNull()
        if (number == null || !number.isFinite()) return "Symulacja: ${sensor.name}"
        val duration = parts.getOrNull(3)?.toIntOrNull()
        val value = "$reading ${sensor.unit}".trim()
        return "Symulacja: ${sensor.name} — $value" + (duration?.let { " · $it min" } ?: "")
    }

    private fun displaySensorSimulationStop(sensorId: String?): String {
        val sensor = SensorDef.ALL.firstOrNull { it.id.equals(sensorId, ignoreCase = true) }
        return sensor?.let { "Wyłączenie symulacji: ${it.name}" } ?: "Wyłączenie symulacji czujnika"
    }

    private fun displayDiagnostic(parts: List<String>): String {
        val area = parts.getOrNull(1)?.lowercase(Locale.ROOT)
        if (area == "status") return "Sprawdzenie stanu zdalnych logów"
        if (area != "remote") return "Diagnostyka sterownika"

        return when (parts.getOrNull(2)?.lowercase(Locale.ROOT)) {
            "on" -> "Zdalny odbiór logów: włączony"
            "off" -> "Zdalny odbiór logów: wyłączony"
            "level" -> {
                val level = parts.getOrNull(3)?.lowercase(Locale.ROOT)
                val levelLabel = level?.let { dlogLevelLabels[it] }
                levelLabel?.let { "Szczegółowość logów: $it" } ?: "Zmiana szczegółowości logów"
            }
            "cat" -> {
                val category = parts.getOrNull(3)?.lowercase(Locale.ROOT)
                val state = parts.getOrNull(4)?.lowercase(Locale.ROOT)
                val categoryLabel = category?.let { dlogCategoryLabels[it] } ?: "wybranej kategorii"
                when (state) {
                    "on", "1", "true" -> "Logi z kategorii $categoryLabel: włączone"
                    "off", "0", "false" -> "Logi z kategorii $categoryLabel: wyłączone"
                    else -> "Ustawienie kategorii logów"
                }
            }
            else -> "Ustawienia zdalnych logów"
        }
    }
}
