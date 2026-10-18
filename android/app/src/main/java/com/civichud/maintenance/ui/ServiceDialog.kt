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
import com.civichud.maintenance.domain.ServiceDraft

@Composable
internal fun ServiceDialog(rule: MaintenanceRule, state: TrackerUiState, onSave: (ServiceDraft) -> Unit, onCapture: () -> Unit, onDismiss: () -> Unit) {
    var locationText by remember(rule.id) { mutableStateOf("") }
    var mechanic by remember(rule.id) { mutableStateOf("") }
    var phone by remember(rule.id) { mutableStateOf("") }
    var cost by remember(rule.id) { mutableStateOf("") }
    var notes by remember(rule.id) { mutableStateOf("") }
    var captured by remember(rule.id) { mutableStateOf<CapturedLocation?>(null) }
    var requested by remember(rule.id) { mutableStateOf(false) }
    var observedBusy by remember(rule.id) { mutableStateOf(false) }
    var oldLocation by remember(rule.id) { mutableStateOf<CapturedLocation?>(null) }
    var confirmation by remember(rule.id) { mutableStateOf(false) }
    var draft by remember(rule.id) { mutableStateOf<ServiceDraft?>(null) }
    var error by remember(rule.id) { mutableStateOf("") }

    // Form text is deliberately keyed only by the selected rule, so GPS updates
    // cannot recreate the form and erase entries the user has already typed.
    LaunchedEffect(state.gpsBusy, state.gpsLocation, requested) {
        if (requested && state.gpsBusy) observedBusy = true
        val location = state.gpsLocation
        if (requested && !state.gpsBusy && location != null && (observedBusy || location != oldLocation)) {
            captured = location
            if (locationText.isBlank()) locationText = location.address
            requested = false
            observedBusy = false
        }
    }

    AlertDialog(modifier = Modifier.automationRoot(), onDismissRequest = onDismiss, title = { Text(if (confirmation) "ยืนยันการบันทึกบริการ" else "บันทึกบริการ · ${rule.name}") }, text = {
        Column(Modifier.verticalScroll(rememberScrollState()).testTag("service_form"), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Text(rule.name, style = MaterialTheme.typography.titleMedium)
            Text("เลขไมล์ประมาณ ${state.vehicle?.estimatedOdometerKm?.let { number(it) } ?: "—"} กม.")
            if (confirmation) {
                Text("เมื่อยืนยัน ระบบจะบันทึกประวัติและเริ่มรอบดูแลใหม่จากเลขไมล์และเวลาปัจจุบัน")
                Metadata("สถานที่", locationText)
                captured?.let { Text("GPS: ${number(it.latitude, 6)}, ${number(it.longitude, 6)}") }
                Metadata("ช่าง / อู่", mechanic)
                Metadata("เบอร์ติดต่อ", phone)
                Metadata("ค่าใช้จ่าย", draft?.cost?.let { "${number(it, 2)} บาท" } ?: "")
                Metadata("หมายเหตุ", notes)
            } else {
                Text("รายละเอียดทั้งหมดด้านล่างเป็นข้อมูลเพิ่มเติม ไม่จำเป็นต้องเปิด GPS", style = MaterialTheme.typography.bodySmall)
                OutlinedTextField(locationText, { locationText = it }, label = { Text("สถานที่ / ที่อยู่ (ไม่บังคับ)") }, modifier = Modifier.fillMaxWidth().testTag("service_location"))
                OutlinedButton(onClick = {
                    oldLocation = state.gpsLocation; requested = true; observedBusy = false; onCapture()
                }, enabled = !state.gpsBusy, modifier = Modifier.fillMaxWidth().testTag("capture_location")) {
                    Text(if (state.gpsBusy) "กำลังรับตำแหน่ง…" else "ใช้ตำแหน่ง GPS ปัจจุบัน")
                }
                captured?.let { location ->
                    Text("แนบ GPS: ${number(location.latitude, 6)}, ${number(location.longitude, 6)}", style = MaterialTheme.typography.bodySmall, modifier = Modifier.testTag("captured_location"))
                    TextButton(onClick = { captured = null; requested = false }, modifier = Modifier.testTag("remove_location")) { Text("ไม่แนบพิกัด GPS") }
                }
                OutlinedTextField(mechanic, { mechanic = it }, label = { Text("ชื่อช่าง / อู่ (ไม่บังคับ)") }, singleLine = true, modifier = Modifier.fillMaxWidth().testTag("service_mechanic"))
                OutlinedTextField(phone, { phone = it }, label = { Text("เบอร์ติดต่อ (ไม่บังคับ)") }, singleLine = true,
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Phone), modifier = Modifier.fillMaxWidth().testTag("service_phone"))
                FormNumber("ค่าใช้จ่าย (บาท / ไม่บังคับ)", cost, "service_cost", { cost = it; error = "" })
                OutlinedTextField(notes, { notes = it }, label = { Text("หมายเหตุ (ไม่บังคับ)") }, minLines = 2, modifier = Modifier.fillMaxWidth().testTag("service_notes"))
                if (error.isNotBlank()) Text(error, color = MaterialTheme.colorScheme.error, modifier = Modifier.testTag("service_error"))
            }
        }
    }, confirmButton = {
        TextButton(onClick = {
            if (confirmation) {
                draft?.let(onSave)
            } else {
                val parsedCost = if (cost.isBlank()) null else parseDecimal(cost)
                if (cost.isNotBlank() && (parsedCost == null || parsedCost < 0)) {
                    error = "ค่าใช้จ่ายต้องเป็นตัวเลขตั้งแต่ 0 ขึ้นไป หรือเว้นว่าง"
                } else {
                    draft = ServiceDraft(captured?.latitude, captured?.longitude, locationText.trim(), mechanic.trim(), phone.trim(), parsedCost, notes.trim())
                    confirmation = true
                }
            }
        }, enabled = !state.gpsBusy, modifier = Modifier.testTag(if (confirmation) "confirm_service" else "review_service")) {
            Text(if (confirmation) "ยืนยันบันทึก" else "ตรวจสอบและบันทึก")
        }
    }, dismissButton = {
        TextButton(onClick = { if (confirmation) confirmation = false else onDismiss() }, modifier = Modifier.testTag(if (confirmation) "edit_service" else "cancel_service")) {
            Text(if (confirmation) "กลับไปแก้ไข" else "ยกเลิก")
        }
    })
}

@Composable
internal fun Metadata(label: String, value: String) {
    Text("$label: ${value.ifBlank { "ไม่ได้ระบุ" }}", style = MaterialTheme.typography.bodyMedium)
}
