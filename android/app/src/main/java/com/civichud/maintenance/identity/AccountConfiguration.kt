package com.civichud.maintenance.identity

import java.net.URI

data class AccountConfiguration(val discoveryUrl:String,val clientId:String,val scope:String,val apiUrl:String) {
    val ready get()=httpsUri(discoveryUrl)!=null && httpsUri(apiUrl)?.rawQuery==null && httpsUri(apiUrl)!=null && clientId.isNotBlank() && scope.isNotBlank()
    fun permitsLink(value:String):Boolean {
        val uri=httpsUri(value)?:return false
        return uri.host.lowercase() in listOfNotNull(httpsUri(discoveryUrl)?.host,httpsUri(apiUrl)?.host).map { it.lowercase() }
    }
    private fun httpsUri(value:String):URI?=runCatching { URI(value).takeIf { it.scheme=="https" && !it.host.isNullOrBlank() && it.rawUserInfo==null && it.rawFragment==null && (it.port==-1 || it.port==443) } }.getOrNull()
}
