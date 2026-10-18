package com.civichud.maintenance.identity

import android.content.Context
import android.content.Intent
import android.net.Uri
import com.civichud.maintenance.BuildConfig
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import net.openid.appauth.*
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

data class AccountUiState(
    val configured: Boolean = false,
    val busy: Boolean = false,
    val accountId: String = "",
    val displayName: String = "",
    val linkedProviders: List<String> = emptyList(),
) { val signedIn get() = accountId.isNotBlank() }

/** Public-client code flow with PKCE. Tokens stay in memory; no account is inferred from decoded JWT claims. */
class AccountClient(context: Context) {
    private val service = AuthorizationService(context)
    private val discovery = BuildConfig.AUTH_DISCOVERY_URL
    private val api = BuildConfig.ACCOUNT_API_URL.trimEnd('/')
    private val configuration=AccountConfiguration(discovery,BuildConfig.AUTH_CLIENT_ID,BuildConfig.AUTH_SCOPE,api)
    private val configured = configuration.ready
    private val mutableState = MutableStateFlow(AccountUiState(configured = configured))
    val state = mutableState.asStateFlow()
    private var accessToken: String? = null

    suspend fun authorizationIntent(): Intent {
        check(configured) { "ระบบบัญชียังรอการตั้งค่าผู้ให้บริการ" }
        mutableState.value = mutableState.value.copy(busy = true)
        try {
            val config = suspendCancellableCoroutine<AuthorizationServiceConfiguration> { continuation ->
                AuthorizationServiceConfiguration.fetchFromUrl(Uri.parse(discovery)) { result, error ->
                    if (continuation.isActive) {
                        if (result != null) continuation.resume(result)
                        else continuation.resumeWithException(IllegalStateException(error?.errorDescription ?: "โหลดระบบบัญชีไม่สำเร็จ"))
                    }
                }
            }
            val request = AuthorizationRequest.Builder(config, BuildConfig.AUTH_CLIENT_ID,
                ResponseTypeValues.CODE, Uri.parse("com.civichud.maintenance:/oauth2redirect"))
                .setScope("openid profile email ${BuildConfig.AUTH_SCOPE}").build()
            return service.getAuthorizationRequestIntent(request)
        } finally { mutableState.value = mutableState.value.copy(busy = false) }
    }

    suspend fun completeAuthorization(intent: Intent?) {
        if (intent == null) return
        val response = AuthorizationResponse.fromIntent(intent)
        val error = AuthorizationException.fromIntent(intent)
        check(response != null && error == null) { "ยกเลิกหรือเข้าสู่ระบบไม่สำเร็จ" }
        mutableState.value = mutableState.value.copy(busy = true)
        try {
            val token = suspendCancellableCoroutine<TokenResponse> { continuation ->
                service.performTokenRequest(response.createTokenExchangeRequest()) { result, failure ->
                    if (continuation.isActive) {
                        if (result != null && failure == null) continuation.resume(result)
                        else continuation.resumeWithException(IllegalStateException("ยืนยันบัญชีไม่สำเร็จ"))
                    }
                }
            }.accessToken ?: error("ไม่ได้รับสิทธิ์เข้าถึงบัญชี")
            // The API validates issuer, audience, signature and expiry, then returns its canonical account ID.
            val me = request("GET", "/me", token)
            val id = me.getString("accountId").trim()
            require(id.isNotEmpty()) { "ข้อมูลบัญชีไม่ครบ" }
            accessToken = token
            val providers = me.optJSONArray("linkedProviders")
            mutableState.value = AccountUiState(configured, false, id, me.optString("displayName"),
                if (providers == null) emptyList() else (0 until providers.length()).map { providers.getString(it) })
        } finally { mutableState.value = mutableState.value.copy(busy = false) }
    }

    /** Server must require recent authentication + a second identity proof; never merge on matching email. */
    suspend fun linkingUri(): Uri {
        val result = request("POST", "/account/link-sessions", requireNotNull(accessToken), JSONObject())
        val uri = Uri.parse(result.getString("authorizationUrl"))
        require(configuration.permitsLink(uri.toString())) { "ปลายทางผูกบัญชีไม่ถูกต้อง" }
        return uri
    }
    suspend fun requestDeletion() {
        val result = request("DELETE", "/account", requireNotNull(accessToken))
        require(result.optString("receiptId").isNotBlank()) { "เซิร์ฟเวอร์ยังไม่ยืนยันรับคำขอ" }
        signOut()
    }
    fun signOut() { accessToken = null; mutableState.value = AccountUiState(configured) }

    private suspend fun request(method: String, path: String, token: String, body: JSONObject? = null): JSONObject = withContext(Dispatchers.IO) {
        val connection = URL(api + path).openConnection() as HttpURLConnection
        try {
            connection.requestMethod = method
            connection.instanceFollowRedirects = false
            connection.connectTimeout = 15_000; connection.readTimeout = 15_000
            connection.setRequestProperty("Authorization", "Bearer $token")
            connection.setRequestProperty("Accept", "application/json")
            if (body != null) {
                connection.doOutput = true; connection.setRequestProperty("Content-Type", "application/json")
                connection.outputStream.use { it.write(body.toString().toByteArray(Charsets.UTF_8)) }
            }
            check(connection.responseCode in 200..299) { "ระบบบัญชีตอบกลับไม่สำเร็จ (${connection.responseCode}) กรุณาเข้าสู่ระบบใหม่" }
            val text = connection.inputStream.bufferedReader().use { it.readText() }
            require(text.length <= 65_536)
            JSONObject(text)
        } finally { connection.disconnect() }
    }
}
