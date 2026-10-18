plugins { id("com.android.library"); kotlin("android"); kotlin("kapt") }
android { namespace="com.civichud.maintenance.data"; compileSdk=36
 defaultConfig { minSdk=26 }
 compileOptions { sourceCompatibility=JavaVersion.VERSION_17; targetCompatibility=JavaVersion.VERSION_17 }
}
kotlin { jvmToolchain(17) }
dependencies {
 testImplementation("junit:junit:4.13.2")
 implementation(project(":domain"))
 implementation("androidx.room:room-runtime:2.8.3")
 implementation("androidx.room:room-ktx:2.8.3")
 kapt("androidx.room:room-compiler:2.8.3")
 implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.10.2")
}
