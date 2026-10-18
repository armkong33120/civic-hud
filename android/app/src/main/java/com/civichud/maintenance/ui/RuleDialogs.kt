package com.civichud.maintenance.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import com.civichud.maintenance.domain.MaintenanceRule
import java.time.LocalDate

internal fun parseDecimal(value: String): Double? = value.trim().replace(",", "").map {
    if (it in '๐'..'๙') ('0'.code + (it.code - '๐'.code)).toChar() else it
}.joinToString("").toDoubleOrNull()?.takeIf { it.isFinite() }

@Composable
internal fun FormNumber(label: String, value: String, tag: String, onChange: (String) -> Unit, integer: Boolean = false) {
    OutlinedTextField(value, onChange, label = { Text(label) }, singleLine = true,
        keyboardOptions = KeyboardOptions(keyboardType = if (integer) KeyboardType.Number else KeyboardType.Decimal),
        modifier = Modifier.fillMaxWidth().testTag(tag))
}

@Composable
internal fun CalibrationDialog(estimate: Double?, onSave: (Double) -> Unit, onDismiss: () -> Unit) {
    var actual by remember { mutableStateOf("") }
    var error by remember { mutableStateOf(false) }
    AlertDialog(modifier = Modifier.automationRoot(), onDismissRequest = onDismiss, title = { Text("ปรับเทียบเลขไมล์จริง") }, text = {
        Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text("กรอกเลขไมล์จากหน้าปัดรถ ระบบจะเริ่มสะสมระยะทางต่อจากค่านี้ ประวัติบริการเดิมยังคงอยู่")
            if (estimate != null) Text("เลขไมล์ประมาณปัจจุบัน ${number(estimate)} กม.")
            FormNumber("เลขไมล์จริง (กม.)", actual, "calibration_input", { actual = it; error = false })
            if (error) Text("กรุณากรอกเลขไมล์เป็นตัวเลขมากกว่า 0", color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("calibration_error"))
        }
    }, confirmButton = {
        TextButton(onClick = { val value = parseDecimal(actual); if (value != null && value > 0) onSave(value) else error = true }, modifier = Modifier.testTag("confirm_calibration")) { Text("บันทึกเลขไมล์") }
    }, dismissButton = { TextButton(onClick = onDismiss) { Text("ยกเลิก") } })
}

@Composable
internal fun DeviceDialog(devices: List<PairedObdDevice>, onRefresh: () -> Unit, onConnect: (String) -> Unit, onDismiss: () -> Unit) {
    AlertDialog(modifier = Modifier.automationRoot(), onDismissRequest = onDismiss, title = { Text("เลือกอะแดปเตอร์ OBD-II") }, text = {
        Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Text("จับคู่อะแดปเตอร์ในตั้งค่า Bluetooth ของโทรศัพท์ก่อน แล้วเลือกอุปกรณ์ที่ต้องการเชื่อมต่อ")
            if (devices.isEmpty()) Text("ไม่พบอุปกรณ์ที่จับคู่ กดรีเฟรชหลังจับคู่หรืออนุญาต Bluetooth", modifier = Modifier.testTag("devices_empty"))
            devices.forEach { device ->
                OutlinedButton(onClick = { onConnect(device.address) }, modifier = Modifier.fillMaxWidth().testTag("adapter_${device.address}")) {
                    Column { Text(device.name.ifBlank { "อะแดปเตอร์ Bluetooth" }); Text(device.address, style = MaterialTheme.typography.bodySmall) }
                }
            }
        }
    }, confirmButton = { TextButton(onClick = onRefresh, modifier = Modifier.testTag("refresh_devices")) { Text("รีเฟรช") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("ปิด") } })
}

@Composable
internal fun RuleDialog(existing: MaintenanceRule?, currentKm: Double, onSave: (MaintenanceRule) -> Unit, onDismiss: () -> Unit) {
    var name by remember(existing?.id) { mutableStateOf(existing?.name ?: "") }
    var intervalKm by remember(existing?.id) { mutableStateOf(existing?.intervalKm?.toString() ?: "") }
    var months by remember(existing?.id) { mutableStateOf(existing?.intervalMonths?.toString() ?: "") }
    var warningKm by remember(existing?.id) { mutableStateOf(existing?.warningKm?.toString() ?: "500") }
    var warningDays by remember(existing?.id) { mutableStateOf(existing?.warningDays?.toString() ?: "14") }
    var error by remember { mutableStateOf("") }
    AlertDialog(modifier = Modifier.automationRoot(), onDismissRequest = onDismiss, title = { Text(if (existing == null) "เพิ่มรายการดูแลรถ" else "แก้ไขรายการดูแลรถ") }, text = {
        Column(Modifier.verticalScroll(rememberScrollState()).testTag("rule_form"), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            OutlinedTextField(name, { name = it }, label = { Text("ชื่อรายการ") }, singleLine = true, modifier = Modifier.fillMaxWidth().testTag("rule_name"))
            Text("กำหนดระยะทางหรือจำนวนเดือนอย่างน้อยหนึ่งค่า ใส่ 0 หรือเว้นว่างเพื่อไม่ใช้เงื่อนไขนั้น", style = MaterialTheme.typography.bodySmall)
            FormNumber("รอบระยะทาง (กม.)", intervalKm, "rule_interval_km", { intervalKm = it })
            FormNumber("รอบเวลา (เดือน)", months, "rule_interval_months", { months = it }, true)
            FormNumber("เตือนล่วงหน้า (กม.)", warningKm, "rule_warning_km", { warningKm = it })
            FormNumber("เตือนล่วงหน้า (วัน)", warningDays, "rule_warning_days", { warningDays = it }, true)
            Text(if (existing == null) "เริ่มนับจาก ${number(currentKm)} กม. และวันนี้ เป็นจุดเริ่มติดตาม ไม่ใช่การบันทึกบริการ" else "การแก้ไขรอบไม่เปลี่ยนจุดเริ่มนับหรือประวัติบริการ", style = MaterialTheme.typography.bodySmall)
            if (error.isNotBlank()) Text(error, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("rule_error"))
        }
    }, confirmButton = {
        TextButton(onClick = {
            val km = if (intervalKm.isBlank()) 0.0 else parseDecimal(intervalKm)
            val monthDouble = if (months.isBlank()) 0.0 else parseDecimal(months)
            val warnKm = if (warningKm.isBlank()) 0.0 else parseDecimal(warningKm)
            val dayDouble = if (warningDays.isBlank()) 0.0 else parseDecimal(warningDays)
            when {
                name.isBlank() -> error = "กรุณากรอกชื่อรายการ"
                km == null || km < 0 || monthDouble == null || monthDouble < 0 || monthDouble > 1200 || monthDouble % 1.0 != 0.0 -> error = "ระยะทางต้องไม่ติดลบ เดือนต้องเป็นจำนวนเต็ม 0–1200"
                km == 0.0 && monthDouble == 0.0 -> error = "กรุณากำหนดระยะทางหรือเดือนให้มากกว่า 0"
                warnKm == null || warnKm < 0 || dayDouble == null || dayDouble < 0 || dayDouble > 36500 || dayDouble % 1.0 != 0.0 -> error = "ค่าเตือนต้องไม่ติดลบ และวันต้องเป็นจำนวนเต็มไม่เกิน 36500"
                else -> onSave(MaintenanceRule(
                    id = existing?.id ?: 0L, name = name.trim(), intervalKm = km,
                    intervalMonths = monthDouble.toInt(), lastServiceOdometerKm = existing?.lastServiceOdometerKm ?: currentKm,
                    lastServiceDate = existing?.lastServiceDate ?: LocalDate.now(), warningKm = warnKm,
                    warningDays = dayDouble.toLong(), cycle = existing?.cycle ?: 0L,
                ))
            }
        }, modifier = Modifier.testTag("save_rule")) { Text("บันทึก") }
    }, dismissButton = { TextButton(onClick = onDismiss) { Text("ยกเลิก") } })
}
