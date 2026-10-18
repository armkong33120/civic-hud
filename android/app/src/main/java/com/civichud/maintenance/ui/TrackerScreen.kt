package com.civichud.maintenance.ui

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import com.civichud.maintenance.BuildConfig
import com.civichud.maintenance.domain.*

@Composable
fun TrackerScreen(state: TrackerUiState, actions: TrackerActions) {
    val systemDark = isSystemInDarkTheme()
    var dark by rememberSaveable { mutableStateOf(systemDark) }
    var tab by rememberSaveable { mutableStateOf(0) }
    var showDevices by remember { mutableStateOf(false) }
    var showCalibration by remember { mutableStateOf(false) }
    var ruleEditor by remember { mutableStateOf<MaintenanceRule?>(null) }
    var addRule by remember { mutableStateOf(false) }
    var serviceRule by remember { mutableStateOf<MaintenanceRule?>(null) }
    var deleteRule by remember { mutableStateOf<MaintenanceRule?>(null) }
    val snackbars = remember { SnackbarHostState() }
    LaunchedEffect(state.message) {
        if (state.message.isNotBlank()) {
            snackbars.showSnackbar(state.message, actionLabel = "ปิด", duration = SnackbarDuration.Long)
            actions.onClearMessage()
        }
    }
    TrackerTheme(dark) {
        Scaffold(
            modifier = Modifier.automationRoot(),
            snackbarHost = { SnackbarHost(snackbars, Modifier.testTag("runtime_message")) },
            bottomBar = {
                NavigationBar {
                    listOf("หน้าหลัก", "ประวัติ", "ตั้งค่า").forEachIndexed { i, label ->
                        NavigationBarItem(selected = tab == i, onClick = { tab = i },
                            icon = { Text(listOf("◉", "≡", "⚙")[i]) }, label = { Text(label) },
                            modifier = Modifier.testTag(listOf("tab_dashboard", "tab_history", "tab_settings")[i]))
                    }
                }
            }
        ) { padding ->
            Column(Modifier.fillMaxSize().padding(padding).padding(horizontal = 16.dp)) {
                Row(Modifier.fillMaxWidth().padding(vertical = 12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                    Column(Modifier.weight(1f)) {
                        Text("ดูแลรถ", style = MaterialTheme.typography.headlineSmall)
                        Text(state.vehicle?.name?.takeIf { it.isNotBlank() } ?: "Smart Vehicle Tracker", style = MaterialTheme.typography.bodySmall)
                    }
                    TextButton(onClick = { dark = !dark }, modifier = Modifier.testTag("toggle_theme")
                        .semantics { contentDescription = "Toggle day night theme" }) {
                        Text(if (dark) "โหมดกลางวัน" else "โหมดกลางคืน")
                    }
                }
                when (tab) {
                    0 -> Dashboard(state, onDevices = { showDevices = true; actions.onRefreshDevices() },
                        onCalibrate = { showCalibration = true }, onStop = actions.onStop,
                        onAddRule = { addRule = true }, onEditRule = { ruleEditor = it },
                        onDeleteRule = { deleteRule = it }, onService = { serviceRule = it })
                    1 -> HistoryScreen(state.history)
                    else -> Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                        Text("การเชื่อมต่อและค่ารถ", style = MaterialTheme.typography.titleLarge)
                        Text("แหล่งข้อมูล: ${if (state.simulation) "ข้อมูลจำลอง · ฐานข้อมูลแยก" else "OBD-II Bluetooth"}")
                        Button(onClick = { showDevices = true; actions.onRefreshDevices() }, modifier = Modifier.fillMaxWidth().testTag("settings_connect")) { Text("เลือกอะแดปเตอร์ Bluetooth") }
                        OutlinedButton(onClick = { showCalibration = true }, modifier = Modifier.fillMaxWidth().testTag("settings_calibrate")) { Text("ปรับเทียบเลขไมล์จริง") }
                        OutlinedButton(onClick = actions.onStop, modifier = Modifier.fillMaxWidth().testTag("settings_stop")) { Text("หยุดติดตาม") }
                        if (BuildConfig.DEBUG) {
                            OutlinedButton(onClick = actions.onStartSimulation, modifier = Modifier.fillMaxWidth().testTag("start_simulation")) { Text("เริ่มโหมดจำลอง (ทดสอบ)") }
                            Text("ข้อมูลจำลองเก็บแยกจากรถจริง ใช้ตรวจสอบหน้าจอและการแจ้งเตือน", style = MaterialTheme.typography.bodySmall)
                        }
                        Text("เลขไมล์ประมาณคำนวณจากความเร็วที่อ่านได้ขณะติดตาม โปรดปรับเทียบกับหน้าปัดรถก่อนใช้งาน และตรวจสอบเป็นระยะ", style = MaterialTheme.typography.bodyMedium)
                        Text("รายการใหม่เริ่มนับจากเลขไมล์และวันที่ตั้งค่า เป็นจุดเริ่มต้นการติดตาม ไม่ใช่ประวัติการเข้ารับบริการ", style = MaterialTheme.typography.bodyMedium)
                        Spacer(Modifier.height(16.dp))
                    }
                }
            }
        }
        if (showDevices) DeviceDialog(state.devices, actions.onRefreshDevices, {
            actions.onConnect(it); showDevices = false
        }, { showDevices = false })
        if (showCalibration) CalibrationDialog(state.vehicle?.estimatedOdometerKm, {
            actions.onCalibrate(it); showCalibration = false
        }, { showCalibration = false })
        if (addRule || ruleEditor != null) RuleDialog(ruleEditor, state.vehicle?.estimatedOdometerKm ?: 0.0,
            onSave = { actions.onSaveRule(it); addRule = false; ruleEditor = null },
            onDismiss = { addRule = false; ruleEditor = null })
        serviceRule?.let { rule -> ServiceDialog(rule, state, onSave = {
            actions.onLogService(rule.id, it); serviceRule = null
        }, onCapture = actions.onCaptureLocation, onDismiss = { serviceRule = null }) }
        deleteRule?.let { rule ->
            AlertDialog(modifier = Modifier.automationRoot(), onDismissRequest = { deleteRule = null }, title = { Text("ลบรายการดูแลรถ?") },
                text = { Text("ลบ ${rule.name} ออกจากรายการติดตาม ประวัติที่บันทึกไว้ยังคงอยู่") },
                confirmButton = { TextButton(onClick = { actions.onDeleteRule(rule.id); deleteRule = null }, modifier = Modifier.testTag("confirm_delete_rule")) { Text("ลบรายการ") } },
                dismissButton = { TextButton(onClick = { deleteRule = null }) { Text("ยกเลิก") } })
        }
    }
}
