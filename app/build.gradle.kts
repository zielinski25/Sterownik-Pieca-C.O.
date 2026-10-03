plugins {
    id("com.android.application")
}

android {
    namespace = "pl.sterownikco.dev"
    compileSdk = 35

    defaultConfig {
        applicationId = "pl.sterownikco.dev"
        minSdk = 26
        targetSdk = 35
        versionCode = 3005
        versionName = "0.30.5-UI-LAYOUT-FIX"
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
}
