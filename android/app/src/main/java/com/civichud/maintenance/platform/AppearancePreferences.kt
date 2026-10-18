package com.civichud.maintenance.platform

import android.content.Context
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.asStateFlow

data class Appearance(val themeMode: String = "system", val reduceMotion: Boolean = false)
class AppearancePreferences(context: Context) {
    private val writeMutex = Mutex()
    private val preferences = context.getSharedPreferences("appearance", Context.MODE_PRIVATE)
    private val mutable = MutableStateFlow(Appearance(preferences.getString("theme", "system") ?: "system", preferences.getBoolean("reduceMotion", false)))
    val state = mutable.asStateFlow()
    suspend fun save(themeMode: String, reduceMotion: Boolean) = writeMutex.withLock { withContext(Dispatchers.IO) {
        require(themeMode in setOf("system", "light", "dark"))
        check(preferences.edit().putString("theme", themeMode).putBoolean("reduceMotion", reduceMotion).commit()) { "บันทึกการแสดงผลไม่สำเร็จ" }
        mutable.value = Appearance(themeMode, reduceMotion)
    } }
}
