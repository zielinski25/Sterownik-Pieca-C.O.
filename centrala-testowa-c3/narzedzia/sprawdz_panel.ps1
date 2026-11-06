# =============================================================================
#  sprawdz_panel.ps1 — gdzie jest sciana: WiFi czy HTTP?
#
#  Uruchom na laptopie/PC podlaczonym do WiFi "PiecCO_AP" (haslo KociolCO2026)
#  i wklej wynik. Skrypt nie zmienia niczego, tylko czyta.
#
#    powershell -ExecutionPolicy Bypass -File narzedzia\sprawdz_panel.ps1
#    powershell -ExecutionPolicy Bypass -File narzedzia\sprawdz_panel.ps1 -Panel 192.168.4.50
#
#  Co sprawdza:
#    1. czy ten PC jest w sieci symulatora (adres 192.168.4.x)
#    2. czy odpowiada symulator (brama 192.168.4.1) i co mowi /api/diag
#    3. czy odpowiada PANEL pod swoim statycznym adresem (domyslnie .50)
#    4. werdykt: panel nie w WiFi / panel w WiFi ale nie pyta / wszystko gra
# =============================================================================
param(
    [string]$Symulator = "192.168.4.1",
    [string]$Panel = "192.168.4.50"
)

$ErrorActionPreference = "Continue"

function Linia($tekst) { Write-Host $tekst }

Linia ""
Linia "=== 1. Ten komputer w sieci symulatora ==="
$moje = Get-NetIPAddress -AddressFamily IPv4 -ErrorAction SilentlyContinue |
        Where-Object { $_.IPAddress -like "192.168.4.*" }
if (-not $moje) {
    Linia "BRAK adresu 192.168.4.x — ten PC NIE jest w sieci PiecCO_AP."
    Linia "Podlacz sie do WiFi 'PiecCO_AP' (haslo KociolCO2026) i uruchom skrypt ponownie."
    exit 1
}
foreach ($m in $moje) { Linia ("  PC: {0}  (interfejs: {1})" -f $m.IPAddress, $m.InterfaceAlias) }

Linia ""
Linia "=== 2. Symulator ($Symulator) ==="
if (Test-Connection -ComputerName $Symulator -Count 3 -Quiet) {
    Linia "  ping OK"
} else {
    Linia "  ping NIE odpowiada — symulator nie zyje / inna siec / inny kanal"
}
$diag = $null
try {
    $diag = Invoke-RestMethod -Uri ("http://{0}/api/diag" -f $Symulator) -TimeoutSec 5
    Linia ("  /api/diag OK: wersja={0} uptime={1}s stacje_ap={2} zapytania={3} statusy={4}" -f `
           $diag.wersja, $diag.uptime_s, $diag.stacje_ap, $diag.zapytania, $diag.statusy)
    Linia ("  panel_fw='{0}'  czas_ok={1}  hist: krotka={2} dluga={3}" -f `
           $diag.panel_fw, $diag.czas_ok, $diag.hist_krotka, $diag.hist_dluga)
} catch {
    Linia ("  /api/diag NIE odpowiada: {0}" -f $_.Exception.Message)
}

Linia ""
Linia "=== 3. Panel ($Panel) ==="
$panelZyje = Test-Connection -ComputerName $Panel -Count 3 -Quiet
if ($panelZyje) {
    Linia "  ping OK — panel jest w sieci pod statycznym adresem"
} else {
    Linia "  ping NIE odpowiada — panel nie jest w sieci (albo ma inny adres)"
}
$arp = arp -a 2>$null | Select-String "192\.168\.4\."
if ($arp) {
    Linia "  tabla ARP (kto realnie odpowiada):"
    foreach ($a in $arp) { Linia ("    " + $a.ToString().Trim()) }
} else {
    Linia "  tabla ARP pusta dla 192.168.4.x"
}

Linia ""
Linia "=== WERDYKT ==="
$stacje = $null; if ($diag) { $stacje = [int]$diag.stacje_ap }
$zapytania = $null; if ($diag) { $zapytania = [int]$diag.zapytania }
if (-not $diag) {
    Linia "  Symulator nie odpowiada po HTTP — najpierw napraw to (czy wisi [HTTP] nasluchuje w konsoli?)."
} elseif ($stacje -eq 0) {
    Linia "  A. Panel w ogole nie wszedl do WiFi (stacje_ap=0)."
    Linia "     -> odlacz panel od zasilania na 5 s (pamiec RTC trzyma BSSID/kanał starej centrali),"
    Linia "     -> sprawdz, czy ekran panelu w ogole cos wyswietla (czy ma firmware panelu),"
    Linia "     -> sprawdz haslo w panelu: KociolCO2026."
} elseif ($zapytania -eq 0) {
    Linia ("  B. Panel JEST w sieci (stacje_ap={0}), ale nie wysyla zadnych zadan HTTP." -f $stacje)
    Linia "     -> zrzut Seriala panelu (USB, 115200): na czym staje httpPobierz()."
} else {
    Linia ("  C. Panel pyta serwer (zapytania={0}) — lacze dziala." -f $zapytania)
    Linia "     Jesli ekran dalej pusty, problem jest w panelu (parsowanie JSON / rysowanie)."
}
Linia ""
