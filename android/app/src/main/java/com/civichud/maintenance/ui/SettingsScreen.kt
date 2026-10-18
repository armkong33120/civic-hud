package com.civichud.maintenance.ui

import android.content.Intent
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import com.civichud.maintenance.BuildConfig
import com.civichud.maintenance.domain.*
import java.time.Year

@Composable
internal fun SettingsScreen(state:TrackerUiState, actions:TrackerActions, onDevices:()->Unit, onCalibrate:()->Unit) {
    var profile by remember { mutableStateOf(false) }
    var feedback by remember { mutableStateOf(false) }
    var deletion by remember { mutableStateOf(false) }
    val context=LocalContext.current
    Column(Modifier.fillMaxSize().verticalScroll(rememberScrollState()).testTag("settings_list"), verticalArrangement=Arrangement.spacedBy(16.dp)) {
        SettingsSection("รถของฉัน", "ข้อมูลรถและการเดินทางอยู่ในเครื่อง ใช้งานได้แม้ไม่มีอินเทอร์เน็ต") {
            Text(state.vehicle?.name ?: "รถของฉัน",style=MaterialTheme.typography.titleLarge)
            Text(state.vehicle?.let { listOf(it.manufacturer,it.model,it.modelYear?.toString().orEmpty(),it.registration,it.fuelType).filter(String::isNotBlank).joinToString(" · ") }.orEmpty())
            OutlinedButton(onClick={profile=true},modifier=Modifier.fillMaxWidth().testTag("edit_vehicle")){Text("แก้ไขข้อมูลรถ")}
            Button(onClick=onDevices,modifier=Modifier.fillMaxWidth().testTag("settings_connect")){Text("เลือกอะแดปเตอร์ Bluetooth")}
            OutlinedButton(onClick=onCalibrate,modifier=Modifier.fillMaxWidth().testTag("settings_calibrate")){Text("ปรับเทียบเลขไมล์จริง")}
            TextButton(onClick=actions.onStop,modifier=Modifier.testTag("settings_stop")){Text("หยุดติดตาม")}
        }
        SettingsSection("บัญชีของคุณ", "เข้าสู่ระบบเป็นทางเลือก ข้อมูลรถยังไม่ซิงก์ขึ้นคลาวด์ในรุ่นนี้") {
            if(state.account.signedIn) {
                Text(state.account.displayName.ifBlank { "เข้าสู่ระบบแล้ว" },style=MaterialTheme.typography.titleMedium)
                Text("ช่องทางที่ผูก: ${state.account.linkedProviders.joinToString().ifBlank { "ดูในระบบบัญชี" }}")
                OutlinedButton(onClick=actions.onLinkAccount,modifier=Modifier.testTag("link_account")){Text("ผูกช่องทางเข้าสู่ระบบเพิ่มเติม")}
                TextButton(onClick=actions.onSignOut){Text("ออกจากระบบบนเครื่องนี้")}
                TextButton(onClick={deletion=true},modifier=Modifier.testTag("delete_account")){Text("ขอลบบัญชี",color=MaterialTheme.colorScheme.error)}
            } else {
                Text("Google · LINE · Facebook · อีเมล",style=MaterialTheme.typography.titleMedium)
                Text(if(state.account.configured) "เลือกช่องทางหรือสร้างบัญชีในหน้าบริการบัญชี" else "กำลังเตรียมระบบบัญชี รอผู้ดูแลลงทะเบียนผู้ให้บริการและตั้งค่า")
                Button(onClick=actions.onLogin,enabled=state.account.configured && !state.account.busy,modifier=Modifier.fillMaxWidth().testTag("account_login")){Text(if(state.account.busy) "กำลังเปิด…" else "เข้าสู่ระบบ / สร้างบัญชี")}
                Text("ใช้ต่อโดยไม่เข้าสู่ระบบได้",style=MaterialTheme.typography.bodySmall)
            }
        }
        SettingsSection("หน้าตาและการเคลื่อนไหว", "ปรับให้สบายตาทั้งกลางวันและกลางคืน") {
            Row(horizontalArrangement=Arrangement.spacedBy(6.dp)) {
                listOf("system" to "ตามเครื่อง","light" to "สว่าง","dark" to "มืด").forEach { (value,label) ->
                    FilterChip(selected=state.themeMode==value,onClick={actions.onAppearance(value,state.reduceMotion)},label={Text(label)},modifier=Modifier.testTag("theme_$value"))
                }
            }
            Row(Modifier.fillMaxWidth(),horizontalArrangement=Arrangement.SpaceBetween) {
                Text("ลดการเคลื่อนไหว",Modifier.weight(1f).padding(top=12.dp))
                Switch(checked=state.reduceMotion,onCheckedChange={actions.onAppearance(state.themeMode,it)},modifier=Modifier.testTag("reduce_motion"))
            }
        }
        SettingsSection("ช่วยกันพัฒนาแอป", "เสนอไอเดีย แจ้งปัญหา หรือบอกสิ่งที่อยากให้ปรับปรุง") {
            Button(onClick={feedback=true},modifier=Modifier.fillMaxWidth().testTag("open_feedback")){Text("เขียนข้อเสนอแนะ")}
            Text("ยังไม่มีปลายทางรับข้อมูลอัตโนมัติ ข้อความจะเก็บในเครื่องก่อน คุณเลือกแชร์ผ่านแอปอื่นได้",style=MaterialTheme.typography.bodySmall)
            state.feedback.forEach { entry ->
                HorizontalDivider()
                Text(entry.title,style=MaterialTheme.typography.titleSmall,modifier=Modifier.testTag("feedback_${entry.id}"))
                Text(if(entry.status=="SENT") "ระบบรับแล้ว · ${entry.receiptId}" else "บันทึกในเครื่อง · ยังไม่ได้ส่ง",style=MaterialTheme.typography.labelMedium)
                Row {
                    TextButton(onClick={
                        val text=buildString {appendLine("[${entry.category}] ${entry.title}");appendLine(entry.message);if(entry.replyEmail.isNotBlank())appendLine("ติดต่อกลับ: ${entry.replyEmail}");if(entry.includeDiagnostics)appendLine("Android ${android.os.Build.VERSION.RELEASE} · แอป ${BuildConfig.VERSION_NAME}")}
                        context.startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_TEXT,text),"แชร์ข้อเสนอแนะ"))
                    },modifier=Modifier.testTag("share_feedback_${entry.id}")){Text("แชร์ข้อความ")}
                    TextButton(onClick={actions.onDeleteFeedback(entry.id)},modifier=Modifier.testTag("delete_feedback_${entry.id}")){Text("ลบฉบับนี้")}
                }
            }
        }
        if(BuildConfig.DEBUG) SettingsSection("เครื่องมือทดสอบ", "ข้อมูลจำลองแยกจากรถจริง") {
            OutlinedButton(onClick=actions.onStartSimulation,modifier=Modifier.testTag("start_simulation")){Text("เริ่มโหมดจำลอง")}
            Text("ปัจจุบัน: ${if(state.simulation) "จำลอง" else "รถจริง"}")
        }
        Text("เลขไมล์ประมาณคำนวณเฉพาะช่วงที่ได้รับความเร็วจากรถ โปรดเทียบกับหน้าปัดเป็นระยะ",style=MaterialTheme.typography.bodySmall)
        Spacer(Modifier.height(20.dp))
    }
    if(profile) VehicleProfileDialog(state.vehicle,{actions.onSaveProfile(it);profile=false},{profile=false})
    if(feedback) FeedbackDialog({actions.onFeedback(it);feedback=false},{feedback=false})
    if(deletion) AlertDialog(onDismissRequest={deletion=false},title={Text("ขอลบบัญชี?")},text={Text("ส่งคำขอลบบัญชีและข้อมูลบนคลาวด์ไปยังระบบ ข้อมูลรถในเครื่องยังคงอยู่ ระบบอาจให้ยืนยันตัวตนอีกครั้ง")},confirmButton={TextButton(onClick={actions.onDeleteAccount();deletion=false}){Text("ยืนยันคำขอ")}},dismissButton={TextButton(onClick={deletion=false}){Text("ยกเลิก")}})
}

@Composable
private fun SettingsSection(title:String,subtitle:String,content:@Composable ColumnScope.()->Unit) {
    Card(Modifier.fillMaxWidth()) { Column(Modifier.padding(18.dp),verticalArrangement=Arrangement.spacedBy(8.dp)) {
        Text(title,style=MaterialTheme.typography.titleLarge)
        Text(subtitle,style=MaterialTheme.typography.bodySmall,color=MaterialTheme.colorScheme.onSurfaceVariant)
        Spacer(Modifier.height(2.dp));content()
    } }
}

@Composable
private fun VehicleProfileDialog(vehicle:Vehicle?,save:(VehicleProfileDraft)->Unit,dismiss:()->Unit) {
    var name by rememberSaveable { mutableStateOf(vehicle?.name.orEmpty()) }
    var make by rememberSaveable { mutableStateOf(vehicle?.manufacturer.orEmpty()) }
    var model by rememberSaveable { mutableStateOf(vehicle?.model.orEmpty()) }
    var year by rememberSaveable { mutableStateOf(vehicle?.modelYear?.toString().orEmpty()) }
    var registration by rememberSaveable { mutableStateOf(vehicle?.registration.orEmpty()) }
    var fuel by rememberSaveable { mutableStateOf(vehicle?.fuelType.orEmpty()) }
    val valid=name.isNotBlank() && (year.isBlank() || year.toIntOrNull() in 1886..Year.now().value+2)
    AlertDialog(modifier=Modifier.automationRoot(),onDismissRequest=dismiss,title={Text("ข้อมูลรถของคุณ")},text={
        Column(Modifier.verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(10.dp)) {
            FormText(name,{name=it.take(120)},"ชื่อรถ / ชื่อเล่น *","vehicle_name")
            FormText(make,{make=it.take(80)},"ยี่ห้อ","vehicle_make")
            FormText(model,{model=it.take(80)},"รุ่นรถ","vehicle_model")
            FormText(year,{year=it.take(4)},"ปี ค.ศ. (ถ้ามี)","vehicle_year")
            FormText(registration,{registration=it.take(40)},"ทะเบียน (ถ้ามี)","vehicle_registration")
            FormText(fuel,{fuel=it.take(40)},"เชื้อเพลิง เช่น เบนซิน / LPG","vehicle_fuel")
            Text("แก้ข้อมูลรถไม่เปลี่ยนเลขไมล์หรือประวัติบริการ",style=MaterialTheme.typography.bodySmall)
        }
    },confirmButton={TextButton(onClick={save(VehicleProfileDraft(name,make,model,year.toIntOrNull(),registration,fuel))},enabled=valid,modifier=Modifier.testTag("save_vehicle")){Text("บันทึก")}},dismissButton={TextButton(onClick=dismiss){Text("ยกเลิก")}})
}
@Composable
private fun FeedbackDialog(save:(FeedbackDraft)->Unit,dismiss:()->Unit) {
    var category by rememberSaveable { mutableStateOf("เสนอไอเดีย") }
    var title by rememberSaveable { mutableStateOf("") }
    var message by rememberSaveable { mutableStateOf("") }
    var email by rememberSaveable { mutableStateOf("") }
    var diagnostics by rememberSaveable { mutableStateOf(false) }
    val valid=title.isNotBlank() && message.isNotBlank() && (email.isBlank() || android.util.Patterns.EMAIL_ADDRESS.matcher(email.trim()).matches())
    AlertDialog(modifier=Modifier.automationRoot(),onDismissRequest=dismiss,title={Text("ข้อเสนอแนะของคุณ")},text={
        Column(Modifier.verticalScroll(rememberScrollState()),verticalArrangement=Arrangement.spacedBy(10.dp)) {
            Row(horizontalArrangement=Arrangement.spacedBy(6.dp)){listOf("เสนอไอเดีย","แจ้งปัญหา").forEach { FilterChip(selected=category==it,onClick={category=it},label={Text(it)}) }}
            FormText(title,{title=it.take(160)},"หัวข้อ *","feedback_title")
            OutlinedTextField(message,{message=it.take(10000)},label={Text("รายละเอียด *")},minLines=3,modifier=Modifier.fillMaxWidth().testTag("feedback_message"))
            FormText(email,{email=it.take(254)},"อีเมลติดต่อกลับ (ถ้ามี)","feedback_email")
            Row { Checkbox(diagnostics,{diagnostics=it});Text("แนบเวอร์ชันแอปและ Android เมื่อแชร์ ไม่แนบตำแหน่งหรือประวัติรถ",style=MaterialTheme.typography.bodySmall) }
            Text("บันทึกในเครื่องก่อน แล้วเลือกแชร์ได้จากหน้าตั้งค่า",style=MaterialTheme.typography.bodySmall)
        }
    },confirmButton={TextButton(onClick={save(FeedbackDraft(category,title,message,email,diagnostics))},enabled=valid,modifier=Modifier.testTag("save_feedback")){Text("บันทึกข้อความ")}},dismissButton={TextButton(onClick=dismiss){Text("ยกเลิก")}})
}
@Composable
private fun FormText(value:String,change:(String)->Unit,label:String,tag:String) {
    OutlinedTextField(value,change,label={Text(label)},singleLine=true,modifier=Modifier.fillMaxWidth().testTag(tag))
}
