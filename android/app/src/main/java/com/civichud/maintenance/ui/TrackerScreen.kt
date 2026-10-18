package com.civichud.maintenance.ui

import androidx.compose.animation.*
import androidx.compose.animation.core.tween
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
import androidx.compose.ui.res.stringResource
import com.civichud.maintenance.R
import com.civichud.maintenance.BuildConfig
import com.civichud.maintenance.domain.*

@Composable
fun TrackerScreen(state: TrackerUiState, actions: TrackerActions) {
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
    TrackerTheme(state.themeMode, state.reduceMotion) {
        Scaffold(
            modifier = Modifier.automationRoot(),
            snackbarHost = { SnackbarHost(snackbars, Modifier.testTag("runtime_message")) },
            bottomBar = {
                NavigationBar {
                    listOf("หน้าหลัก", "ประวัติ", "ตั้งค่า").forEachIndexed { i, label ->
                        NavigationBarItem(selected = tab == i, onClick = { tab = i },
                            colors=NavigationBarItemDefaults.colors(selectedIconColor=MaterialTheme.colorScheme.primary,selectedTextColor=MaterialTheme.colorScheme.onSurface,indicatorColor=MaterialTheme.colorScheme.primaryContainer,unselectedIconColor=MaterialTheme.colorScheme.onSurfaceVariant,unselectedTextColor=MaterialTheme.colorScheme.onSurfaceVariant),
                            icon = { NavigationGlyph(i) }, label = { Text(label) },
                            modifier = Modifier.testTag(listOf("tab_dashboard", "tab_history", "tab_settings")[i]))
                    }
                }
            }
        ) { padding ->
            Column(Modifier.fillMaxSize().padding(padding).padding(horizontal = 16.dp)) {
                Row(Modifier.fillMaxWidth().padding(vertical = 12.dp), horizontalArrangement = Arrangement.SpaceBetween) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.app_name), style = MaterialTheme.typography.headlineSmall)
                        Text(state.vehicle?.name?.takeIf { it.isNotBlank() } ?: "Smart Vehicle Tracker", style = MaterialTheme.typography.bodySmall)
                    }
                    AssistChip(onClick={tab=2},label={Text(if(state.simulation) "ทดสอบ" else "รถของฉัน")},modifier=Modifier.testTag("vehicle_settings"))
                }
                AnimatedContent(targetState=tab, transitionSpec={
                    fadeIn(tween(if(state.reduceMotion) 0 else 180)) togetherWith fadeOut(tween(if(state.reduceMotion) 0 else 120))
                },label="navigation") { selectedTab ->
                when (selectedTab) {
                    0 -> Dashboard(state, onDevices = { showDevices = true; actions.onRefreshDevices() },
                        onCalibrate = { showCalibration = true }, onStop = actions.onStop,
                        onAddRule = { addRule = true }, onEditRule = { ruleEditor = it },
                        onDeleteRule = { deleteRule = it }, onService = { serviceRule = it })
                    1 -> HistoryScreen(state.history)
                    else -> SettingsScreen(state,actions,{showDevices=true;actions.onRefreshDevices()},{showCalibration=true})
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
