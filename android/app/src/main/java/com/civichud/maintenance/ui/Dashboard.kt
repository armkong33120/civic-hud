package com.civichud.maintenance.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.getValue
import androidx.compose.runtime.Composable
import androidx.compose.animation.*
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.luminance
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.civichud.maintenance.domain.*
import java.text.NumberFormat
import java.util.Locale
import kotlin.math.abs

internal fun number(value: Double, digits: Int = 1): String = NumberFormat.getNumberInstance(Locale.US).apply {
    minimumFractionDigits = digits; maximumFractionDigits = digits
}.format(value)

internal fun statusLabel(status: TrackingStatus): String = when (status) {
    TrackingStatus.STOPPED -> "หยุดติดตาม"
    TrackingStatus.SCANNING -> "กำลังค้นหาอะแดปเตอร์"
    TrackingStatus.CONNECTING -> "กำลังเชื่อมต่อ"
    TrackingStatus.INITIALIZING -> "กำลังเตรียม OBD-II"
    TrackingStatus.CONNECTED -> "เชื่อมต่อแล้ว"
    TrackingStatus.DATA_STALE -> "ข้อมูลความเร็วขาดช่วง"
    TrackingStatus.DISCONNECTED -> "การเชื่อมต่อขาดหาย"
    TrackingStatus.RECONNECT_WAIT -> "รอเชื่อมต่อใหม่"
}

@Composable
internal fun Dashboard(
    state: TrackerUiState, onDevices: () -> Unit, onCalibrate: () -> Unit, onStop: () -> Unit,
    onAddRule: () -> Unit, onEditRule: (MaintenanceRule) -> Unit,
    onDeleteRule: (MaintenanceRule) -> Unit, onService: (MaintenanceRule) -> Unit,
) {
    val reduced = LocalReducedMotion.current
    LazyColumn(Modifier.fillMaxSize().testTag("dashboard_list"), verticalArrangement = Arrangement.spacedBy(12.dp), contentPadding = PaddingValues(bottom = 20.dp)) {
        item {
            Card(Modifier.fillMaxWidth(), colors = CardDefaults.cardColors(containerColor = Color(0xFF123E32), contentColor = Color.White)) {
                Column(Modifier.background(Brush.linearGradient(listOf(Color(0xFF123E32), Color(0xFF205847)))).padding(22.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(state.vehicle?.let { listOf(it.manufacturer,it.model,it.modelYear?.toString().orEmpty()).filter(String::isNotBlank).joinToString(" · ").ifBlank { it.name } } ?: "รถของฉัน", style = MaterialTheme.typography.labelLarge, color=Color(0xFFB7D7CA))
                    Text("เลขไมล์ประมาณ", style = MaterialTheme.typography.titleMedium)
                    AnimatedContent(targetState=state.vehicle?.let { number(it.estimatedOdometerKm) } ?: "—", transitionSpec={ fadeIn(tween(if(reduced) 0 else 180)) togetherWith fadeOut(tween(if(reduced) 0 else 120)) }, label="odometer") { value ->
                        Text(value, style=MaterialTheme.typography.displayMedium, fontWeight=FontWeight.Bold, modifier=Modifier.testTag("estimated_odometer"))
                    }
                    Text("กิโลเมตร · ${if (state.vehicle?.isCalibrated == true) "ปรับเทียบแล้ว" else "ยังไม่ได้ปรับเทียบ"}", modifier = Modifier.testTag("calibration_status"))
                    if (state.vehicle?.isCalibrated != true) {
                        Text("กรุณากรอกเลขไมล์จากหน้าปัดรถก่อนใช้แผนดูแลรถ", style = MaterialTheme.typography.bodyMedium)
                    }
                    OutlinedButton(onClick = onCalibrate, colors=ButtonDefaults.outlinedButtonColors(contentColor=Color.White), modifier = Modifier.testTag("calibrate_odometer")) { Text("ปรับเทียบเลขไมล์จริง") }
                }
            }
        }
        item {
            Card(Modifier.fillMaxWidth()) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(if (state.simulation) "แหล่งข้อมูล: โหมดจำลอง (ทดสอบ)" else "แหล่งข้อมูล: OBD-II Bluetooth", modifier = Modifier.testTag("data_source"), style = MaterialTheme.typography.titleSmall)
                    Text(statusLabel(state.telemetry.status), modifier = Modifier.testTag("tracking_status"), style = MaterialTheme.typography.titleMedium)
                    if (state.telemetry.detail.isNotBlank()) Text(state.telemetry.detail, style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("tracking_detail"))
                    if (state.telemetry.retryAttempt > 0) Text("พยายามเชื่อมต่อครั้งที่ ${state.telemetry.retryAttempt}", style = MaterialTheme.typography.bodySmall)
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(12.dp)) {
                        Metric("ความเร็ว", state.telemetry.speedKmh?.let { "${number(it, 0)} กม./ชม." } ?: "— กม./ชม.", "speed_value", Modifier.weight(1f))
                        Metric("ระยะทางรอบนี้", "${number(state.telemetry.sessionDistanceKm, 3)} กม.", "session_distance", Modifier.weight(1f))
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        Button(onClick = onDevices, modifier = Modifier.weight(1f).testTag("select_adapter")) { Text("เชื่อมต่อรถ") }
                        OutlinedButton(onClick = onStop, modifier = Modifier.testTag("stop_tracking")) { Text("หยุด") }
                    }
                }
            }
        }
        item {
            val due = state.rules.count { it.status == MaintenanceStatus.OVERDUE }
            val soon = state.rules.count { it.status == MaintenanceStatus.APPROACHING }
            if(state.vehicle?.isCalibrated == true) Surface(color=MaterialTheme.colorScheme.surfaceVariant, shape=RoundedCornerShape(16.dp)) {
                Text("${state.rules.size} รายการดูแล  ·  ถึงกำหนด $due  ·  ใกล้ถึงกำหนด $soon", Modifier.fillMaxWidth().padding(14.dp), style=MaterialTheme.typography.labelLarge)
            }
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
                Text("แผนดูแลรถ", style = MaterialTheme.typography.titleLarge)
                TextButton(onClick = onAddRule, enabled = state.vehicle?.isCalibrated == true, modifier = Modifier.testTag("add_rule")) { Text("เพิ่มรายการ") }
            }
            if (state.rules.isNotEmpty()) Text("ครบกำหนดเมื่อถึงระยะทางหรือวัน อย่างใดอย่างหนึ่ง", style = MaterialTheme.typography.bodySmall)
        }
        if (state.rules.isEmpty()) item { Text("ยังไม่มีรายการดูแลรถ ปรับเทียบเลขไมล์แล้วเพิ่มรายการที่ต้องการ") }
        items(state.rules, key = { it.rule.id }) { result ->
            MaintenanceCard(result, state.vehicle?.isCalibrated == true, { onEditRule(result.rule) }, { onDeleteRule(result.rule) }, { onService(result.rule) })
        }
    }
}

@Composable
private fun Metric(label: String, value: String, tag: String, modifier: Modifier) {
    Column(modifier) {
        Text(label, style = MaterialTheme.typography.bodySmall)
        Text(value, style = MaterialTheme.typography.titleMedium, modifier = Modifier.testTag(tag))
    }
}

@Composable
private fun MaintenanceCard(result: MaintenanceResult, calibrated: Boolean, onEdit: () -> Unit, onDelete: () -> Unit, onService: () -> Unit) {
    val dark = MaterialTheme.colorScheme.surface.luminance() < 0.4f
    val color = when (result.status) {
        MaintenanceStatus.NORMAL -> if (dark) Color(0xFF82DEA5) else GoodGreen
        MaintenanceStatus.APPROACHING -> if (dark) Color(0xFFFFD275) else WarningAmber
        MaintenanceStatus.OVERDUE -> if (dark) Color(0xFFFFA7AA) else DangerRed
    }
    val animatedColor by animateColorAsState(color, tween(if(LocalReducedMotion.current) 0 else 240), label="maintenance-color")
    val progress by animateFloatAsState(result.usedFraction.toFloat().coerceIn(0f,1f), tween(if(LocalReducedMotion.current) 0 else 280), label="maintenance-progress")
    val status = when (result.status) {
        MaintenanceStatus.NORMAL -> "ปกติ"
        MaintenanceStatus.APPROACHING -> "ใกล้ถึงกำหนด"
        MaintenanceStatus.OVERDUE -> "ถึงกำหนด / เกินกำหนด"
    }
    val rule = result.rule
    Card(Modifier.fillMaxWidth().testTag("maintenance_${rule.id}")) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text(rule.name, style = MaterialTheme.typography.titleLarge)
            Text(if (calibrated) status else "รอปรับเทียบเลขไมล์", color = if (calibrated) color else MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.semantics { contentDescription = "Maintenance status ${rule.id}: ${if (calibrated) status else "Await calibration"}" })
            LinearProgressIndicator(progress = { progress }, modifier = Modifier.fillMaxWidth().height(10.dp), color = animatedColor)
            result.remainingKm?.let { remaining -> Text(if (remaining >= 0) "เหลือ ${number(remaining)} กม." else "เกิน ${number(abs(remaining))} กม.") }
            result.remainingDays?.let { remaining -> Text(if (remaining >= 0) "เหลือ $remaining วัน" else "เกิน ${abs(remaining)} วัน") }
            Text(buildList {
                if (rule.intervalKm > 0) add("${number(rule.intervalKm, 0)} กม.")
                if (rule.intervalMonths > 0) add("${rule.intervalMonths} เดือน")
            }.joinToString(" หรือ ", prefix = "รอบดูแล: "), style = MaterialTheme.typography.bodySmall)
            Text("เริ่มนับ / บริการล่าสุด: ${number(rule.lastServiceOdometerKm)} กม. · ${rule.lastServiceDate}", style = MaterialTheme.typography.bodySmall)
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Button(onClick = onService, enabled = calibrated, modifier = Modifier.weight(1f).testTag("service_${rule.id}")) { Text("บันทึกบริการ") }
                TextButton(onClick = onEdit, modifier = Modifier.testTag("edit_rule_${rule.id}")) { Text("แก้ไข") }
                TextButton(onClick = onDelete, modifier = Modifier.testTag("delete_rule_${rule.id}")) { Text("ลบ") }
            }
        }
    }
}
