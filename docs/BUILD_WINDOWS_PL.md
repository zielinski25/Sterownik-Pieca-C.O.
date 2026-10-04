# BUILD WINDOWS — v0.10.3 DEV

### 1. Otwórz projekt
Folder:
`C:\Users\Rafcio\Downloads\sterownik-co-android_v0.10.3-DEV`

### 2. Gradle
`File → Settings → Build, Execution, Deployment → Build Tools → Gradle`

- Distribution: `Wrapper`
- Gradle JDK: `GRADLE_LOCAL_JAVA_HOME` / JetBrains Runtime Android Studio

### 3. Synchronizacja
`File → Sync Project with Gradle Files`

### 4. Pierwszy build
Dopiero po zielonej synchronizacji:
`Build → Make Project`

Nie uruchamiaj jeszcze produkcyjnego Firebase.

### 5. Znane komunikaty (nie są błędami)
- `The daemon has terminated unexpectedly on startup attempt #1 with error code: 0. The daemon process output: 1. Kotlin compile daemon is ready`
  — fałszywy błąd Kotlin Gradle Plugin 2.0.x (KT-72530). Build kończy się sukcesem („Gradle build finished”,
  „Install successfully finished”), tylko Build Output pokazuje go na czerwono. Od v0.30.8 `gradle.properties`
  ustawia `kotlin.compiler.execution.strategy=in-process`, co usuwa komunikat. Prawdziwy błąd kompilacji
  zawsze ma postać `e: file:///…/Plik.kt:linia:kolumna …` i wtedy build kończy się „failed”.
