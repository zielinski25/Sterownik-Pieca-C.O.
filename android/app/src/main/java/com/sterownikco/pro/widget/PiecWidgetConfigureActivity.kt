package com.sterownikco.pro.widget

import android.app.Activity
import android.appwidget.AppWidgetManager
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Checkbox
import androidx.compose.material3.CheckboxDefaults
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.sterownikco.pro.ui.theme.Pal
import com.sterownikco.pro.ui.theme.SterownikTheme

/** Konfiguracja uruchamiana przez launcher przy dodawaniu lub edycji widgetu. */
class PiecWidgetConfigureActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setResult(Activity.RESULT_CANCELED)

        val appWidgetId = intent?.getIntExtra(
            AppWidgetManager.EXTRA_APPWIDGET_ID,
            AppWidgetManager.INVALID_APPWIDGET_ID
        ) ?: AppWidgetManager.INVALID_APPWIDGET_ID
        if (appWidgetId == AppWidgetManager.INVALID_APPWIDGET_ID) {
            finish()
            return
        }

        val setup = PiecWidget.setupForWidget(this, appWidgetId)
        if (setup == null) {
            finish()
            return
        }
        val initialSelection = PiecWidget.selectedSensors(this, appWidgetId)

        setContent {
            SterownikTheme {
                WidgetConfigureScreen(
                    setup = setup,
                    initialSelection = initialSelection,
                    onSave = { sensorKeys ->
                        PiecWidget.saveSelectedSensors(this@PiecWidgetConfigureActivity, appWidgetId, sensorKeys)
                        // Launcher nie wysyła ACTION_APPWIDGET_UPDATE po konfiguracji.
                        PiecWidget.renderWidget(this@PiecWidgetConfigureActivity, appWidgetId)
                        val result = Intent().putExtra(AppWidgetManager.EXTRA_APPWIDGET_ID, appWidgetId)
                        setResult(Activity.RESULT_OK, result)
                        finish()
                    },
                    onCancel = {
                        setResult(Activity.RESULT_CANCELED)
                        finish()
                    }
                )
            }
        }
    }
}

@Composable
private fun WidgetConfigureScreen(
    setup: WidgetSetup,
    initialSelection: List<String>,
    onSave: (List<String>) -> Unit,
    onCancel: () -> Unit
) {
    var selectedKeys by remember { mutableStateOf(initialSelection.toSet()) }
    val selectedInDisplayOrder = setup.sensors.filter { it.key in selectedKeys }
    val canSave = selectedInDisplayOrder.isNotEmpty()

    Column(
        modifier = Modifier.fillMaxSize()
            .background(Pal.Bg)
            .systemBarsPadding()
    ) {
        Column(
            modifier = Modifier.weight(1f)
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 18.dp, vertical = 18.dp),
            verticalArrangement = Arrangement.spacedBy(14.dp)
        ) {
            Column(verticalArrangement = Arrangement.spacedBy(5.dp)) {
                Text(
                    "STEROWNIK CO  ·  WIDŻET",
                    color = Pal.Cyan,
                    fontSize = 11.sp,
                    fontWeight = FontWeight.ExtraBold,
                    letterSpacing = 1.1.sp
                )
                Text(
                    "Dostosuj widżet",
                    color = Pal.White,
                    fontSize = 25.sp,
                    fontWeight = FontWeight.ExtraBold
                )
                Text(
                    setup.title,
                    color = Pal.TextDim,
                    fontSize = 13.sp,
                    fontWeight = FontWeight.SemiBold
                )
            }

            Surface(
                modifier = Modifier.fillMaxWidth(),
                color = Pal.Hero,
                shape = RoundedCornerShape(16.dp),
                border = BorderStroke(1.dp, Pal.BorderStrong)
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(6.dp)
                ) {
                    Text(
                        "NA WIDŻECIE ZOBACZYSZ",
                        color = Pal.TextDim,
                        fontSize = 9.sp,
                        fontWeight = FontWeight.ExtraBold,
                        letterSpacing = 0.8.sp
                    )
                    Text(
                        selectedInDisplayOrder.joinToString("  ·  ") { it.label }.ifEmpty { "Wybierz co najmniej jeden czujnik" },
                        color = Pal.White,
                        fontSize = 14.sp,
                        fontWeight = FontWeight.Bold,
                        lineHeight = 20.sp
                    )
                    Text(
                        "Temperatury na pulpicie będą zaokrąglane do pełnych stopni.",
                        color = Pal.TextDim,
                        fontSize = 11.sp
                    )
                }
            }

            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(
                    "WYBIERZ CZUJNIKI  ·  ${selectedKeys.size}/${setup.maxSensors}",
                    color = Pal.TextDim,
                    fontSize = 10.sp,
                    fontWeight = FontWeight.ExtraBold,
                    letterSpacing = 0.7.sp
                )
                setup.sensors.forEach { sensor ->
                    val checked = sensor.key in selectedKeys
                    val canChange = checked || selectedKeys.size < setup.maxSensors
                    Surface(
                        modifier = Modifier.fillMaxWidth()
                            .clickable(enabled = canChange) {
                                selectedKeys = if (checked) selectedKeys - sensor.key else selectedKeys + sensor.key
                            },
                        color = if (checked) Pal.Surface2 else Pal.Surface,
                        shape = RoundedCornerShape(14.dp),
                        border = BorderStroke(
                            1.dp,
                            if (checked) Pal.Cyan.copy(alpha = 0.48f) else Pal.Border
                        )
                    ) {
                        Row(
                            modifier = Modifier.padding(start = 14.dp, end = 8.dp, top = 7.dp, bottom = 7.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Column(
                                modifier = Modifier.weight(1f).padding(vertical = 4.dp),
                                verticalArrangement = Arrangement.spacedBy(3.dp)
                            ) {
                                Text(
                                    sensor.label,
                                    color = if (checked) Pal.White else Pal.Text,
                                    fontSize = 13.sp,
                                    fontWeight = FontWeight.Bold
                                )
                                Text(sensor.description, color = Pal.TextDim, fontSize = 11.sp)
                            }
                            Checkbox(
                                checked = checked,
                                enabled = canChange,
                                onCheckedChange = { isChecked ->
                                    selectedKeys = if (isChecked) selectedKeys + sensor.key else selectedKeys - sensor.key
                                },
                                colors = CheckboxDefaults.colors(
                                    checkedColor = Pal.Cyan,
                                    uncheckedColor = Pal.TextDim2,
                                    checkmarkColor = Pal.Bg,
                                    disabledCheckedColor = Pal.Cyan.copy(alpha = 0.6f),
                                    disabledUncheckedColor = Pal.TextDim2.copy(alpha = 0.5f)
                                )
                            )
                        }
                    }
                }
                if (selectedKeys.size >= setup.maxSensors) {
                    Text(
                        "W tym wariancie możesz wyświetlić maksymalnie ${setup.maxSensors} czujniki.",
                        color = Pal.TextDim2,
                        fontSize = 10.sp
                    )
                }
            }
            Spacer(Modifier.height(2.dp))
        }

        Column(
            modifier = Modifier.fillMaxWidth()
                .background(Pal.Top)
                .padding(horizontal = 18.dp, vertical = 10.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Button(
                onClick = { onSave(selectedInDisplayOrder.map { it.key }) },
                enabled = canSave,
                modifier = Modifier.fillMaxWidth().height(50.dp),
                shape = RoundedCornerShape(14.dp),
                colors = ButtonDefaults.buttonColors(
                    containerColor = Pal.Cyan,
                    contentColor = Color(0xFF00131F),
                    disabledContainerColor = Pal.Surface2,
                    disabledContentColor = Pal.TextDim2
                )
            ) {
                Text("Zapisz i dodaj widżet", fontSize = 14.sp, fontWeight = FontWeight.ExtraBold)
            }
            TextButton(onClick = onCancel) {
                Text("Anuluj", color = Pal.TextDim, fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
            }
        }
    }
}
