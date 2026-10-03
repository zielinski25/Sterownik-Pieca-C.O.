$ErrorActionPreference = 'Stop'
$required = @(
 '.\build.gradle.kts', '.\app\build.gradle.kts', '.\gradle\wrapper\gradle-wrapper.properties',
 '.\gradlew', '.\gradlew.bat', '.\app\src\main\AndroidManifest.xml',
 '.\app\src\main\java\pl\sterownikco\dev\FirebaseAuthRest.kt',
 '.\app\src\main\java\pl\sterownikco\dev\SecureSessionStore.kt',
 '.\app\src\main\java\pl\sterownikco\dev\MainActivity.kt'
)
foreach ($f in $required) { if (!(Test-Path $f)) { throw "BRAK: $f" } }
$app = Get-Content '.\app\build.gradle.kts' -Raw
if ($app -match 'org\.jetbrains\.kotlin\.android') { throw 'Nie powinno byc starego pluginu Kotlin Android.' }
Write-Host 'v0.10.5 STATIC CHECK: PASS'
