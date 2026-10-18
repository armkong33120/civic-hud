package com.civichud.maintenance.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.civichud.maintenance.domain.ServiceLog
import java.time.Instant
import java.time.ZoneId
import java.time.format.DateTimeFormatter
import java.util.Locale

@Composable
internal fun HistoryScreen(history: List<ServiceLog>) {
    LazyColumn(Modifier.fillMaxSize().testTag("history_list"), verticalArrangement = Arrangement.spacedBy(12.dp), contentPadding = PaddingValues(bottom = 20.dp)) {
        item {
            Text("ประวัติบริการ", style = MaterialTheme.typography.titleLarge)
            Text("รายการที่ยืนยันบันทึกไว้ พร้อมรายละเอียดสถานที่และค่าใช้จ่าย", style = MaterialTheme.typography.bodySmall)
        }
        if (history.isEmpty()) item {
            Card(Modifier.fillMaxWidth()) {
                Text("ยังไม่มีประวัติบริการ เมื่อเข้ารับบริการแล้ว ให้บันทึกจากรายการในหน้าหลัก", Modifier.padding(16.dp).testTag("history_empty"))
            }
        }
        items(history.sortedByDescending { it.performedAt }, key = { it.id }) { entry ->
            Card(Modifier.fillMaxWidth().testTag("history_${entry.id}")) {
                Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                    Text(entry.itemName, style = MaterialTheme.typography.titleLarge)
                    Text(DateTimeFormatter.ofPattern("dd MMM yyyy HH:mm", Locale.forLanguageTag("th-TH"))
                        .format(Instant.ofEpochMilli(entry.performedAt).atZone(ZoneId.systemDefault())), style = MaterialTheme.typography.bodySmall)
                    Text("เลขไมล์ประมาณ ${number(entry.odometerKm)} กม.", style = MaterialTheme.typography.titleMedium)
                    Metadata("สถานที่", entry.locationText)
                    if (entry.latitude != null && entry.longitude != null) {
                        Text("GPS: ${number(entry.latitude!!, 6)}, ${number(entry.longitude!!, 6)}", style = MaterialTheme.typography.bodySmall)
                    }
                    Metadata("ช่าง / อู่", entry.mechanicName)
                    Metadata("เบอร์ติดต่อ", entry.phone)
                    Metadata("ค่าใช้จ่าย", entry.cost?.let { "${number(it, 2)} บาท" } ?: "")
                    Metadata("หมายเหตุ", entry.notes)
                }
            }
        }
    }
}
