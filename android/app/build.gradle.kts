plugins { id("com.android.application"); kotlin("android"); id("org.jetbrains.kotlin.plugin.compose") }
android {
 namespace="com.civichud.maintenance"; compileSdk=36
 defaultConfig { applicationId="com.civichud.maintenance"; minSdk=26; targetSdk=36; versionCode=4; versionName="0.3.1" }
 defaultConfig {
  manifestPlaceholders["appAuthRedirectScheme"] = "com.civichud.maintenance"
  listOf("AUTH_DISCOVERY_URL", "AUTH_CLIENT_ID", "AUTH_SCOPE", "ACCOUNT_API_URL").forEach { key ->
   val value = providers.gradleProperty(key).orNull.orEmpty()
   buildConfigField("String", key, "\"" + value.replace("\\", "\\\\").replace("\"", "\\\"") + "\"")
  }
 }
 buildFeatures { compose=true; buildConfig=true }
 compileOptions { sourceCompatibility=JavaVersion.VERSION_17; targetCompatibility=JavaVersion.VERSION_17 }
 buildTypes { release { isMinifyEnabled=false } }
}
kotlin { jvmToolchain(17) }
dependencies {
 implementation("net.openid:appauth:0.11.1")
 implementation(project(":domain")); implementation(project(":data"))
 implementation(platform("androidx.compose:compose-bom:2025.10.01"))
 implementation("androidx.compose.material3:material3")
 implementation("androidx.compose.ui:ui-tooling-preview")
 debugImplementation("androidx.compose.ui:ui-tooling")
 implementation("androidx.activity:activity-compose:1.11.0")
 implementation("androidx.lifecycle:lifecycle-runtime-compose:2.9.4")
 implementation("androidx.lifecycle:lifecycle-viewmodel-compose:2.9.4")
 implementation("androidx.core:core-ktx:1.17.0")
 implementation("androidx.work:work-runtime-ktx:2.10.5")
 implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
 testImplementation("junit:junit:4.13.2")
}
