plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "com.clawdmeter.widget"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.clawdmeter.widget"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "1.0"
    }

    // Release signing. The keystore and its passwords live in
    // ~/.gradle/gradle.properties (mode 600), never in this repo — it is a real
    // signing key, and anyone holding it can publish updates that install over
    // yours. Absent those properties the release build simply stays unsigned,
    // so a fresh clone still builds.
    val storeFilePath = findProperty("CLAWD_STORE_FILE") as String?
    signingConfigs {
        if (storeFilePath != null && file(storeFilePath).exists()) {
            create("release") {
                storeFile = file(storeFilePath)
                storePassword = findProperty("CLAWD_STORE_PASSWORD") as String?
                keyAlias = findProperty("CLAWD_KEY_ALIAS") as String?
                keyPassword = findProperty("CLAWD_KEY_PASSWORD") as String?
            }
        }
    }

    buildTypes {
        release {
            // No shrinking: the app is a few hundred KB and R8 rules are one
            // more thing to get wrong for zero benefit at this size.
            isMinifyEnabled = false
            signingConfig = signingConfigs.findByName("release")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    // The only non-trivial dependency. Needed because a widget's own
    // updatePeriodMillis floor is 30 min and is not honoured in Doze; WorkManager
    // is what actually gets a periodic refresh to run reliably.
    implementation("androidx.work:work-runtime-ktx:2.9.1")
}
