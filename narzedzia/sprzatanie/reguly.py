# -*- coding: utf-8 -*-
"""
Reguly sprzatania dysku - jedno miejsce, ktore edytujesz.

Nic tutaj nie usuwa plikow. Reguly mowia skanerowi:
  - co jest smieciem (kategorie i progi wieku/rozmiaru),
  - co jest nietykalne (NIGDY),
  - jaka jest domyslna akcja dla znaleziska.

Poziom ryzyka:
  1 - odnawialne (cache, temp, dumps) - po skasowaniu system sam odtworzy
  2 - odtwarzalne recznie (stare instalki, node_modules, duplikaty)
  3 - dane uzytkownika (duze pliki, dokumenty) - domyslnie TYLKO raport
"""

from pathlib import Path

# ----------------------------------------------------------------- progi
PROG_INSTALKI_DNI = 30        # instalki .exe/.msi w Pobranych starsze niz tyle dni
PROG_POBRANE_DNI = 180        # stare archiwa/obrazy w Pobranych
PROG_DUZY_MB = 500            # "duzy plik"
PROG_PROJEKT_DNI = 30         # node_modules/.venv/target nieuzywane tyle dni
PROG_DUPLIKAT_MB = 2          # ponizej tego nie porownujemy hashy (za tanio)

# Rozszerzenia, ktore w Pobranych traktujemy jako jednorazowke.
EXT_INSTALKI = {".exe", ".msi", ".msix", ".appx"}
EXT_ARCHIWA = {".zip", ".7z", ".rar", ".iso", ".img", ".tar", ".gz", ".bz2", ".xz"}
EXT_MEDIA = {".mkv", ".mp4", ".avi", ".mov", ".wmv"}
EXT_LOGOW = {".log", ".dmp", ".etl", ".old", ".bak", ".tmp", ".chk"}

# Katalogi smieciowe wzgledem %LOCALAPPDATA% (glob, * = dowolny profil/przegladarka).
CACHE_W_LOCALAPPDATA = [
    "Temp",
    "CrashDumps",
    "Google/Chrome/User Data/*/Cache",
    "Google/Chrome/User Data/*/Code Cache",
    "Google/Chrome/User Data/*/GPUCache",
    "Google/Chrome/User Data/*/Service Worker/CacheStorage",
    "Microsoft/Edge/User Data/*/Cache",
    "Microsoft/Edge/User Data/*/Code Cache",
    "Microsoft/Edge/User Data/*/GPUCache",
    "Microsoft/Edge/User Data/*/Service Worker/CacheStorage",
    "BraveSoftware/Brave-Browser/User Data/*/Cache",
    "Mozilla/Firefox/Profiles/*/cache2",
    "Microsoft/Windows/INetCache",
    "Microsoft/Windows/Explorer/thumbcache_*.db",
    "Microsoft/Teams/*/Cache",
    "Microsoft/Teams/*/Code Cache",
    "Microsoft/OneDrive/Logs",
    "Discord/Cache",
    "Discord/Code Cache",
    "Slack/Cache",
    "Slack/Code Cache",
    "pip/Cache",
    "npm-cache/_cacache",
    "Yarn/Cache",
    ".cache/pip",
    "uv/cache",
    "Temp/*_MEI*",              # rozpakowane bundly PyInstaller
]

# Katalogi smieciowe wzgledem katalogu domowego (grupa cache_i_temp).
CACHE_W_HOME = [
    ".cache/pip",
    ".cache/uv",
    ".npm/_cacache",
    ".gradle/caches",
    ".m2/repository/.cache",
    ".vscode/extensions/.obsolete",
]

# v8/v9: cache deweloperskie - OSOBNA grupa (cache_deweloperskie), NIE
# cache_i_temp. To pobrane artefakty (toolchainy, crate'y), czesto po kilka GB.
# v9: wydzielone z CACHE_W_HOME, bo cache() lapal je do cache_i_temp.
CACHE_DEV_W_HOME = [
    ".platformio",              # PlatformIO: toolchainy ESP32/Arduino (czesto GB)
    ".cargo/registry/cache",    # Cargo: pobrane crate'y (Rust)
    ".cargo/git",               # Cargo: zaleznosci git
    ".cache/pnpm",              # pnpm store
    ".cache/pre-commit",        # pre-commit: pobrane srodowiska
    # CELOWO POMINIETE: .android (ma debug.keystore!), .ollama (modele AI),
    # .cache/huggingface (modele) - to NIE sa cache, to narzedzia uzytkownika.
]

# Katalogi, ktore sa artefaktem builda - odtwarzalne komenda (npm i / cmake).
KATALOGI_BUILDA = {"node_modules", "__pycache__", ".venv", "venv", "target",
                   "dist", "build", ".pytest_cache", ".mypy_cache", ".ruff_cache",
                   ".next", ".nuxt", ".turbo", ".parcel-cache"}

# --------------------------------------------------------------- nietykalne
# Sciezki, w ktorych NIC nie ruszamy automatycznie - nawet jesli wygladaja
# jak smiec. Folder w chmurze = usuniecie lokalne usunie plik u wszystkich.
NIGDY_FRAGMENTY = [
    "/onedrive",
    "\\onedrive",
    "/dropbox",
    "\\dropbox",
    "/google drive",
    "\\google drive",
    "/google_drive",
    "\\google_drive",
    "/icloud drive",
    "\\icloud drive",
    "/.git/",
    "\\.git\\",
    "/windows/system32",
    "\\windows\\system32",
    "/windows/winsxs",
    "\\windows\\winsxs",
    "/program files",
    "\\program files",
    "/steamapps",
    "\\steamapps",
    "/appdata/roaming",
    "\\appdata\\roaming",
]

# Katalogi, ktorych skaner nie wchodzi (dane aplikacji, ogromne i wrazliwe).
POMIN_KATALOGI = {"$recycle.bin", "system volume information", "windows",
                  "program files", "program files (x86)", "programdata",
                  "appdata",  # wchodzimy selektywnie, patrz CACHE_W_LOCALAPPDATA
                  }

# Katalogi, ktore wolno skanowac w AppData (reszta pomijana).
APPADATA_DOZWOLONE = {"temp", "cache", "caches", "code cache", "gpucache",
                      "crashdumps", "inetcache", "logs", "cachedata",
                      "cachestorage", "user data", "profiles", "explorer"}

# -------------------------------------------------------------- kwarantanna
# Domyslne miejsce przenoszenia. Ten sam dysk = przenoszenie jest natychmiastowe.
def katalog_kwarantanny(home: Path) -> Path:
    return home / "AppData" / "Local" / "SprzatanieDysku" / "kwarantanna"


def katalog_raportow(home: Path) -> Path:
    return home / "AppData" / "Local" / "SprzatanieDysku" / "raporty"


# ---------------------------------------------------------------- ollama
OLLAMA_URL = "http://127.0.0.1:11434"
# 4750U = 8 rdzeni CPU, 16 GB RAM, brak VRAM dla modelu.
# 7b-q4 to ~4.7 GB w RAM i ok. 4-6 tok/s - wystarczy do oceny listy plikow.
MODEL_DOMYSLNY = "qwen2.5:7b-instruct-q4_K_M"
MODEL_SZYBKI = "qwen2.5:3b-instruct-q4_K_M"

# ------------------------------------------------------- v8: nowe kategorie
PROG_CACHE_DEV_MB = 10        # cache deweloperskie: tylko gdy >10 MB
PROG_STARE_KATALOGI_DNI = 365  # katalogi nieuzywane rok+ (ryzyko 3, raport)
PROG_OLD_WINDOWS_GB = 2       # Windows.old: tylko gdy >2 GB

# v8: rekomendacje systemowe (komendy PowerShell, ktorych skrypt NIE wykona -
# wymagaja admina / sa ryzykowne; agent je tylko pokazuje w raporcie).
REKOMENDACJE_SYSTEMOWE = [
    {
        "nazwa": "Czyszczenie komponentow Windows (WinSxS)",
        "komenda": "Dism /Online /Cleanup-Image /StartComponentCleanup",
        "efekt": "zwykle 3-8 GB",
        "uwagi": "Wymaga PowerShell jako Administrator. Trwa 10-30 min, "
                 "nie przerywaj. Po tym nie da sie odinstalowac "
                 "aktualizacji Windows.",
    },
    {
        "nazwa": "Punkty przywracania (VSS)",
        "komenda": "vssadmin list shadowstorage",
        "efekt": "zwykle 5-15 GB",
        "uwagi": "Sprawdz, ile miejsca zajmuja punkty przywracania. "
                 "Zmniejsz limit w Ochrona systemu (Panel sterowania).",
    },
    {
        "nazwa": "Hibernacja (hiberfil.sys)",
        "komenda": "powercfg /h off",
        "efekt": "ok. 16 GB (rozmiar RAM)",
        "uwagi": "Wymaga admina. Wylacza hibernacje (szybki start tez). "
                 "Jesli nie uzywasz hibernacji - czysty zysk.",
    },
    {
        "nazwa": "Plik strony (pagefile.sys)",
        "komenda": "System -> Zaawansowane -> Wydajnosc -> Ustawienia "
                   "-> Zaawansowane -> Pamiec wirtualna",
        "efekt": "zalezy od RAM (16-32 GB)",
        "uwagi": "Przenies na inny dysk lub zmniejsz. NIE wylaczaj "
                 "completely przy malej RAM.",
    },
    {
        "nazwa": "Katalog Windows.old",
        "komenda": "Narzędzie do czyszczenia dysku -> Poprzednie instalacje",
        "efekt": "zwykle 10-30 GB",
        "uwagi": "Agent sam wykryje Windows.old w raporcie (kategoria "
                 "windows_old) i przeniesie do kwarantanny - ale najpierw "
                 "sprawdz, czy nie potrzebujesz powrotu do starego Windowsa.",
    },
    {
        "nazwa": "Katalog Windows/Temp",
        "komenda": "Narzędzie do czyszczenia dysku (cleanmgr) -> "
                   "Pliki tymczasowe Windows",
        "efekt": "zwykle 0,5-3 GB",
        "uwagi": "Agent nie wchodzi do C:/Windows (zabezpieczenie). "
                 "cleanmgr to oficjalna droga.",
    },
]
