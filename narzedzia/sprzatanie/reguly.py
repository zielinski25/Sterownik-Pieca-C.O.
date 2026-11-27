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

# Katalogi smieciowe wzgledem katalogu domowego.
CACHE_W_HOME = [
    ".cache/pip",
    ".cache/uv",
    ".npm/_cacache",
    ".gradle/caches",
    ".m2/repository/.cache",
    ".vscode/extensions/.obsolete",
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
