package com.civichud.maintenance.ui

import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.animation.core.tween
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.platform.testTag
import androidx.compose.ui.unit.dp
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.semantics.disabled
import com.civichud.maintenance.R

/** Guest access is a deliberate session choice, never a successful account login. */
@Composable
fun AppEntry(state:TrackerUiState, actions:TrackerActions) {
    var offline by rememberSaveable { mutableStateOf(false) }
    LaunchedEffect(state.account.signedIn) { if(state.account.signedIn) offline=false }
    TrackerTheme(state.themeMode,state.reduceMotion) {
        AnimatedContent(targetState=state.account.signedIn || offline,transitionSpec={
            fadeIn(tween(if(state.reduceMotion) 0 else 180)) togetherWith fadeOut(tween(if(state.reduceMotion) 0 else 120))
        },label="entry") { entered ->
            if(entered) TrackerScreen(state,actions)
            else AccountWelcome(state,actions,{offline=true})
        }
    }
}

@Composable
private fun AccountWelcome(state:TrackerUiState,actions:TrackerActions,onOffline:()->Unit) {
    var signup by rememberSaveable { mutableStateOf(false) }
    val snackbars=remember { SnackbarHostState() }
    LaunchedEffect(state.message) {
        if(state.message.isNotBlank()) { snackbars.showSnackbar(state.message);actions.onClearMessage() }
    }
    Scaffold(modifier=Modifier.automationRoot(),snackbarHost={SnackbarHost(snackbars)}) { padding ->
        Column(Modifier.fillMaxSize().padding(padding).verticalScroll(rememberScrollState()).padding(horizontal=28.dp,vertical=20.dp).testTag("account_welcome"),
            verticalArrangement=Arrangement.spacedBy(12.dp)) {
            Row(verticalAlignment=Alignment.CenterVertically,horizontalArrangement=Arrangement.spacedBy(14.dp)) {
                Icon(painterResource(R.drawable.ic_tracker_mark),contentDescription="โลโก้ดูแลรถ",tint=androidx.compose.ui.graphics.Color.Unspecified,modifier=Modifier.size(56.dp).testTag("brand_logo"))
                Column {
                    Text("ดูแลรถ",style=MaterialTheme.typography.headlineMedium)
                    Text("ทุกระยะทาง มีแผนดูแล",style=MaterialTheme.typography.bodySmall,color=MaterialTheme.colorScheme.primary)
                }
            }
            Row(horizontalArrangement=Arrangement.spacedBy(10.dp)) {
                FilterChip(selected=!signup,onClick={signup=false},label={Text("เข้าสู่ระบบ")},modifier=Modifier.testTag("entry_login_tab"))
                FilterChip(selected=signup,onClick={signup=true},label={Text("สมัครสมาชิก")},modifier=Modifier.testTag("entry_signup_tab"))
            }
            Text(if(signup) "เริ่มต้นบัญชีของคุณ" else "ยินดีต้อนรับกลับ",style=MaterialTheme.typography.headlineSmall,modifier=Modifier.testTag("entry_heading"))
            Text(if(signup) "สมัครด้วยบัญชีที่คุณใช้ หรืออีเมลของคุณ" else "เลือกช่องทางเข้าสู่ระบบของคุณ",style=MaterialTheme.typography.bodyMedium)
            // Provider-specific routing is not registered yet; do not imply these are working OAuth buttons.
            listOf("google" to "Google","line" to "LINE","facebook" to "Facebook","email" to "อีเมล").forEach { (id,name) ->
                OutlinedCard(modifier=Modifier.fillMaxWidth().testTag("entry_provider_$id").semantics { disabled() }) {
                    Row(Modifier.fillMaxWidth().heightIn(min=52.dp).padding(horizontal=16.dp,vertical=8.dp),horizontalArrangement=Arrangement.SpaceBetween,verticalAlignment=Alignment.CenterVertically) {
                        Text("${if(signup) "สมัครด้วย" else "เข้าสู่ระบบด้วย"} $name")
                        Text("รอเปิดใช้",style=MaterialTheme.typography.labelSmall)
                    }
                }
            }
            if(state.account.configured) {
                Button(onClick=actions.onLogin,enabled=!state.account.busy,modifier=Modifier.fillMaxWidth().testTag("entry_broker_login")) {
                    Text(if(state.account.busy) "กำลังเปิด…" else if(signup) "เปิดบริการสมัครสมาชิก" else "เปิดบริการบัญชี")
                }
                Text("เลือกช่องทางที่เปิดใช้หรือสมัครสมาชิกในหน้าบริการบัญชี",style=MaterialTheme.typography.bodySmall)
            } else {
                Surface(color=MaterialTheme.colorScheme.surfaceVariant,shape=MaterialTheme.shapes.medium) {
                    Text("ระบบบัญชีกำลังเตรียมเปิดให้ใช้งาน ระหว่างนี้เริ่มดูแลรถแบบออฟไลน์ได้",Modifier.padding(14.dp),style=MaterialTheme.typography.bodySmall)
                }
            }
            OutlinedButton(onClick=onOffline,modifier=Modifier.fillMaxWidth().heightIn(min=52.dp).testTag("continue_offline")) { Text("เข้าใช้งานออฟไลน์ก่อน") }
            Text("ข้อมูลรถเก็บในมือถือ การเข้าใช้ออฟไลน์ไม่ถือว่าเข้าสู่ระบบ และยังไม่ซิงก์ข้อมูลขึ้นคลาวด์",style=MaterialTheme.typography.bodySmall,color=MaterialTheme.colorScheme.onSurfaceVariant)
            Spacer(Modifier.height(8.dp))
        }
    }
}
