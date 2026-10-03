# STEROWNIK CO — Konfiguracja projektu

Ten dokument zawiera instrukcje konfiguracji wszystkich kluczy API, credentials i parametrów wymaganych do uruchomienia aplikacji.

---

## 📁 Struktura plików konfiguracyjnych

```
app/src/main/
├── java/pl/sterownikco/dev/
│   ├── FirebaseDevConfig.kt      ← Konfiguracja Firebase
│   ├── WeatherService.kt          ← Koordynaty pogody (opcjonalnie)
│   └── MainActivity.kt            ← Główne ustawienia aplikacji
├── res/values/
│   ├── strings.xml               ← (opcjonalnie) zaszyte stringi
│   ├── colors.xml                ← Paleta kolorów UI
│   ── styles.xml                ← Style aplikacji
── AndroidManifest.xml            ← Permissions, package name
```

---

## 1. 🔥 Firebase Configuration

### 1.1 Firebase Console Setup

1. Wejdź na https://console.firebase.google.com
2. Wybierz projekt `STEROWNIK CO` (lub utwórz nowy)
3. Przejdź do **Project Settings** → **General** → **Your apps**
4. Dodaj aplikację Android z package: `pl.sterownikco.dev`

### 1.2 Google Services JSON

**Plik:** `app/google-services.json`

**Skąd pobrać:**
- Firebase Console → Project Settings → Twoja aplikacja Android → `google-services.json`

**Co zawiera:**
```json
{
  "project_info": {
    "project_number": "123456789012",
    "project_id": "sterownik-co-dev",
    "storage_bucket": "sterownik-co-dev.appspot.com"
  },
  "client": [
    {
      "client_info": {
        "mobilesdk_app_id": "1:123456789012:android:abcdef1234567890",
        "android_client_info": {
          "package_name": "pl.sterownikco.dev"
        }
      },
      "oauth_client": [...],
      "api_key": [
        {
          "current_key": "AIzaSyXXXXXXX-XXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
        }
      ],
      "services": {...}
    }
  ],
  "configuration_version": "1"
}
```

**UWAGA:** Ten plik NIE jest w repozytorium (gitignored). Musisz go dodać ręcznie.

### 1.3 FirebaseDevConfig.kt

**Plik:** `app/src/main/java/pl/sterownikco/dev/FirebaseDevConfig.kt`

**Lokalizacja w kodzie:**
```kotlin
object FirebaseDevConfig {
    const val FIREBASE_API_KEY = "TU_WSTAW_API_KEY_Z_GOOGLE_SERVICES_JSON"
    const val FIREBASE_AUTH_DOMAIN = "sterownik-co-dev.firebaseapp.com"
    const val FIREBASE_DATABASE_URL = "https://sterownik-co-dev-default-rtdb.europe-west1.firebasedatabase.app"
    const val FIREBASE_PROJECT_ID = "sterownik-co-dev"
    const val FIREBASE_STORAGE_BUCKET = "sterownik-co-dev.appspot.com"
    const val FIREBASE_MESSAGING_SENDER_ID = "123456789012"
    const val FIREBASE_APP_ID = "1:123456789012:android:abcdef1234567890"
}
```

**Co wstawić:**
- `FIREBASE_API_KEY` → z `google-services.json` → `client[0].api_key[0].current_key`
- `FIREBASE_AUTH_DOMAIN` → z Firebase Console → Authentication → Settings → Authorized domains
- `FIREBASE_DATABASE_URL` → z Firebase Console → Realtime Database → URL bazy
- `FIREBASE_PROJECT_ID` → z `google-services.json` → `project_info.project_id`
- `FIREBASE_STORAGE_BUCKET` → z `google-services.json` → `project_info.storage_bucket`
- `FIREBASE_MESSAGING_SENDER_ID` → z `google-services.json` → `project_info.project_number`
- `FIREBASE_APP_ID` → z `google-services.json` → `client[0].client_info.mobilesdk_app_id`

### 1.4 Firebase Realtime Database Rules

**Lokalizacja:** Firebase Console → Realtime Database → Rules

**Zalecane reguły produkcyjne:**
```json
{
  "rules": {
    "piec": {
      "status": {
        ".read": "auth != null",
        ".write": false
      },
      "cmd": {
        ".read": "auth != null",
        ".write": "auth != null"
      },
      "ack": {
        ".read": "auth != null",
        ".write": "auth != null"
      },
      "history": {
        "$year": {
          "$month": {
            "$day": {
              ".read": "auth != null",
              ".write": false
            }
          }
        }
      }
    }
  }
}
```

**Reguły deweloperskie (tylko do testów):**
```json
{
  "rules": {
    ".read": "auth != null",
    ".write": "auth != null"
  }
}
```

---

## 2. 🌤️ Open-Meteo Weather API

### 2.1 Konfiguracja lokalizacji

**Plik:** `app/src/main/java/pl/sterownikco/dev/WeatherService.kt`

**Domyślne koordynaty (Warszawa):**
```kotlin
fun fetchWeather(
    latitude: Double = 52.2297,
    longitude: Double = 21.0122,
    callback: Callback
)
```

**Jak zmienić lokalizację:**

Opcja 1 — Na sztywno w kodzie:
```kotlin
// W WeatherService.kt, linia ~45
val currentUrl = "https://api.open-meteo.com/v1/forecast?latitude=50.0647&longitude=19.9450&..."
// ↑ Zmień na swoje koordynaty (np. Kraków: 50.0647, 19.9450)
```

Opcja 2 — W MainActivity przy wywołaniu:
```kotlin
// W showWeatherMenu() lub gdziekolwiek wywołujesz WeatherService
WeatherService.fetchWeather(
    latitude = 50.0647,    // Szerokość geograficzna
    longitude = 19.9450,   // Długość geograficzna
    callback = object : WeatherService.Callback {
        override fun onSuccess(data: WeatherService.WeatherData) {
            // Obsługa danych
        }
        override fun onError(error: String) {
            // Obsługa błędu
        }
    }
)
```

### 2.2 Jak znaleźć koordynaty

1. Wejdź na https://www.open-meteo.com/
2. Wpisz nazwę miasta
3. Skopiuj latitude/longitude

Lub:
- Google Maps → prawy klik na lokalizację → pierwsze liczby to koordynaty
- Format: `50.0647, 19.9450` (szerokość, długość)

### 2.3 API Key

**Open-Meteo NIE wymaga API key** dla podstawowego użytku (do 10,000 requestów/dzień).

Jeśli potrzebujesz wyższych limitów:
- Wejdź na https://open-meteo.com/en/pricing
- Wygeneruj API key
- Dodaj jako parametr: `&apikey=TU_WSTAW_KEY`

---

## 3. 📱 Android Configuration

### 3.1 Package Name

**Plik:** `app/build.gradle.kts`

```kotlin
android {
    namespace = "pl.sterownikco.dev"
    
    defaultConfig {
        applicationId = "pl.sterownikco.dev"  // ← Musi pasować do Firebase
        minSdk = 26
        targetSdk = 36
        versionCode = 3005
        versionName = "0.30.5-UI-LAYOUT-FIX"
    }
}
```

**UWAGA:** `applicationId` musi być IDENTYCZNE z package name w Firebase Console.

### 3.2 Permissions

**Plik:** `app/src/main/AndroidManifest.xml`

```xml
<uses-permission android:name="android.permission.INTERNET" />
<uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />
```

Te permissions są już dodane. Jeśli dodajesz geolokację do pogody:
```xml
<uses-permission android:name="android.permission.ACCESS_FINE_LOCATION" />
<uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION" />
```

### 3.3 Build Configuration

**Plik:** `app/build.gradle.kts`

```kotlin
android {
    compileSdk = 36
    
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    
    kotlinOptions {
        jvmTarget = "17"
    }
}

dependencies {
    implementation("com.google.firebase:firebase-auth:22.3.0")
    implementation("com.google.firebase:firebase-database:20.3.0")
    implementation("androidx.core:core-ktx:1.12.0")
    // ... inne zależności
}
```

---

## 4. 🔐 Security & Credentials

### 4.1 Firebase Authentication

**Typ:** Email/Password

**Setup w Firebase Console:**
1. Authentication → Get Started
2. Sign-in method → Email/Password → Enable
3. (Opcjonalnie) Add domains do authorized

**Testowy użytkownik:**
- Firebase Console → Authentication → Users → Add user
- Email: `test@sterownikco.pl`
- Hasło: `test123456`

### 4.2 Command Token (ESP32)

**Lokalizacja w kodzie:** `MainActivity.kt` → `sendCommand()`

Token komend jest zarządzany przez firmware ESP32. Android tylko przekazuje komendy do `/piec/cmd`.

**Security:**
- Komendy wymagają ważnego Firebase ID token
- Firmware weryfikuje token przed wykonaniem
- ACK system potwierdza wykonanie

### 4.3 Local Storage

**SharedPreferences keys:**
```kotlin
// Sesja użytkownika
"sterownik_session" → email, refreshToken, idToken, expiresAtMs, localId

// Logi
"sterownik_logs" → log_0, log_1, ..., log_499

// Cache telemetrii
"telemetry_cache" → records, last_update
```

**Lokalizacja:** `data/data/pl.sterownikco.dev/shared_prefs/`

---

## 5. 🌐 Network Configuration

### 5.1 ESP32 Firmware URL

**Lokalizacja w kodzie:** `FirebaseDevConfig.kt` lub hardcoded w `MainActivity.kt`

```kotlin
// Jeśli masz lokalny panel ESP32:
const val LOCAL_PANEL_URL = "http://192.168.1.100/"  // ← Zmień na IP swojego ESP32
```

### 5.2 Firebase RTDB Endpoints

```kotlin
// Status pieca (odczyt)
GET https://[DATABASE_URL]/piec/status.json?auth=[ID_TOKEN]

// Komendy (zapis)
POST https://[DATABASE_URL]/piec/cmd.json?auth=[ID_TOKEN]

// ACK komend (odczyt)
GET https://[DATABASE_URL]/piec/ack/[CMD_ID].json?auth=[ID_TOKEN]

// Historia telemetrii (odczyt)
GET https://[DATABASE_URL]/piec/history/[YYYY]/[MM]/[DD].json?auth=[ID_TOKEN]
```

---

## 6. 🎨 UI Customization

### 6.1 Kolory

**Plik:** `app/src/main/res/values/colors.xml`

```xml
<!-- Główna paleta -->
<color name="app_bg">#060D18</color>          <!-- Tło aplikacji -->
<color name="surface">#0C1829</color>          <!-- Karty -->
<color name="surface2">#111F35</color>         <!-- Karty 2 -->

<!-- Akcenty -->
<color name="cyan">#FF00D4F5</color>           <!-- Główny akcent -->
<color name="orange">#FFFF9F43</color>         <!-- Akcent 2 -->

<!-- Statusy -->
<color name="live">#FF4ADE80</color>           <!-- OK/LIVE -->
<color name="warn">#FFFBBF24</color>           <!-- Ostrzeżenie -->
<color name="err">#FFFF5F78</color>            <!-- Błąd/Alarm -->

<!-- Serie wykresów -->
<color name="series_blue">#FF55D7FF</color>    <!-- Zewn. -->
<color name="series_red">#FFFF5E67</color>     <!-- Ogrzewanie -->
<color name="series_orange">#FFFFB04A</color>  <!-- Bojler -->
```

### 6.2 Typografia

**Plik:** `app/src/main/java/pl/sterownikco/dev/MainActivity.kt`

```kotlin
// Rozmiary czcionek (w dp)
heroTemp = label("--.- °C", 46f, ...)        // Główna temperatura
title = label("STEROWNIK CO", 18.8f, ...)    // Tytuł
subtitle = label("CENTRALA • piec_co", 9.2f, ...)  // Podtytuł
```

---

## 7. 🧪 Testing & Debug

### 7.1 Logi

**Włączanie logów:**
```kotlin
// W MainActivity.kt
private const val DEBUG = true

if (DEBUG) {
    Log.d("SterownikCO", "Status: $data")
}
```

**Podgląd logów:**
```bash
adb logcat | grep SterownikCO
```

### 7.2 Testowe dane Firebase

**Struktura bazy:**
```json
{
  "piec": {
    "status": {
      "t_zewn": 15.5,
      "t_ogrz": 45.2,
      "t_bojler": 55.0,
      "t_panel": 25.0,
      "t_pokoj": 22.0,
      "cisnienie": 1013.25,
      "wilgotnosc": 45.0,
      "pompa": true,
      "tryb_serwa": 1,
      "klapa": 75,
      "syberka": 50,
      "mieszadloWlaczony": false,
      "dym_alarm": false,
      "alarm_ogrzewanie": false,
      "alarm_panel": false
    },
    "cmd": {},
    "ack": {},
    "history": {
      "2026": {
        "10": {
          "03": {
            "abc123-def456": {
              "ts": 1728000000,
              "seq": 1,
              "a": [15.5, 45.2, 55.0, 25.0, 22.0, 1013.25, 45.0, 0, 0, 0, 0],
              "k": 75.0,
              "s": 50.0,
              "state": 1,
              "q": 0,
              "sim": 0,
              "age": [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
            }
          }
        }
      }
    }
  }
}
```

### 7.3 Symulacja czujników

**Z menu kafelka → Symulacja:**
- Zewnętrzna: -30°C do 50°C
- Bojler: 0°C do 160°C
- Pokój: -50°C do 50°C
- Ciśnienie: 970 hPa do 1040 hPa
- Wilgotność: 0% do 100%

---

## 8. 🚀 Build & Deploy

### 8.1 Development Build

```bash
# Sync Gradle
File → Sync Project with Gradle Files

# Build
Build → Make Project (Ctrl+F9)

# Run
Run → Run 'app' (Shift+F10)
```

### 8.2 Release Build

```bash
# Generate signed APK
Build → Generate Signed Bundle / APK
→ APK
→ Create new keystore (lub użyj istniejącego)
→ Release
→ v1 (Jar Signature) + v2 (Full APK Signature)
```

### 8.3 Version Bump

**Plik:** `app/build.gradle.kts`

```kotlin
defaultConfig {
    versionCode = 3006      // ← Zwiększ o 1
    versionName = "0.30.6"  // ← Nowa wersja
}
```

---

## 9. 📊 Monitoring & Analytics

### 9.1 Firebase Analytics (opcjonalne)

**Dodaj zależność:**
```kotlin
implementation("com.google.firebase:firebase-analytics:21.5.0")
```

**Inicjalizacja:**
```kotlin
// W MainActivity.onCreate()
val analytics = FirebaseAnalytics.getInstance(this)
analytics.setUserId("user123")
analytics.logEvent("dashboard_viewed", Bundle())
```

### 9.2 Crash Reporting

**Dodaj zależność:**
```kotlin
implementation("com.google.firebase:firebase-crashlytics:18.6.0")
```

Firebase Crashlytics automatycznie raportuje crash'e.

---

## 10. 🔧 Troubleshooting

### 10.1 Firebase Connection Failed

**Sprawdź:**
1. `google-services.json` jest w `app/`
2. `applicationId` w `build.gradle.kts` pasuje do Firebase
3. Internet permission w `AndroidManifest.xml`
4. Firebase Database Rules pozwalają na odczyt

### 10.2 Weather Not Loading

**Sprawdź:**
1. Koordynaty są poprawne (nie 0,0)
2. Internet działa
3. Open-Meteo API nie jest zablokowane (firewall)

**Debug:**
```kotlin
Log.e("WeatherService", "Error: ${e.message}")
```

### 10.3 Logs Not Saving

**Sprawdź:**
1. SharedPreferences nie są wyczyszczone
2. Max limit 500 logów nie jest osiągnięty

**Clear logs:**
```kotlin
LogStorage.clear(context)
```

---

## 11.  Checklist przed deployment

- [ ] `google-services.json` dodany do `app/`
- [ ] Firebase Database Rules ustawione (produkcyjne)
- [ ] `versionCode` i `versionName` zaktualizowane
- [ ] Test na fizycznym urządzeniu
- [ ] Test komend ACK
- [ ] Test długiej sesji (24h+)
- [ ] Test offline (brak sieci)
- [ ] Security audit (brak hardcoded credentials)
- [ ] Performance test (scroll, wykresy)
- [ ] Koordynaty pogody ustawione na docelowe

---

## 12.  Przydatne linki

- **Firebase Console:** https://console.firebase.google.com
- **Open-Meteo API:** https://open-meteo.com/en/docs
- **Android Studio Docs:** https://developer.android.com/studio
- **Kotlin Docs:** https://kotlinlang.org/docs/home.html
- **Material Design:** https://m3.material.io

---

##  Wsparcie

W przypadku problemów:
1. Sprawdź logi (`adb logcat`)
2. Weryfikuj Firebase Console
3. Testuj na fizycznym urządzeniu
4. Sprawdź network requests (Android Studio Profiler)

---

**Ostatnia aktualizacja:** 2026-10-03  
**Wersja projektu:** 0.30.5-UI-LAYOUT-FIX  
**Autor:** Arena.ai Agent Mode
