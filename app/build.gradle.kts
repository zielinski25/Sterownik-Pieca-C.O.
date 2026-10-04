plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

android {
    namespace = "pl.sterownikco.dev"
    compileSdk = 35

    defaultConfig {
        applicationId = "pl.sterownikco.dev"
        minSdk = 26
        targetSdk = 35
        versionCode = 3007
        versionName = "0.30.7-SIM-FILTER-FIX"
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
    implementation("org.jetbrains.kotlin:kotlin-stdlib:2.0.21")
}
