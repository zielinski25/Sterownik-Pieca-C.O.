# [v1.5.4 OTA-VSCODE] Haslo do wgrywania przez WiFi (espota) brane z src/secrets.h,
# zeby nie lezalo w platformio.ini (ten plik jest w gicie; secrets.h nie jest).
# Skrypt "post" srodowiska [env:esp32dev_wifi]: po tym, jak platforma ustawi
# polecenie espota.py, dopisuje do niego --auth=<SECRET_OTA_HASLO>.
# Nic nie robi przy zwyklym budowaniu (bez celu upload) ani w innych srodowiskach.
import os
import re
import sys

Import("env")  # noqa: F821 - zmienna wstrzykiwana przez PlatformIO/SCons


def haslo_z_secrets():
    sciezka = os.path.join(env.subst("$PROJECT_DIR"), "src", "secrets.h")  # noqa: F821
    if not os.path.isfile(sciezka):
        return None, "brak pliku src/secrets.h (skopiuj src/secrets.h.example i wpisz hasla)"
    with open(sciezka, encoding="utf-8", errors="replace") as f:
        tresc = f.read()
    m = re.search(r'^\s*#\s*define\s+SECRET_OTA_HASLO\s+"((?:[^"\\]|\\.)*)"', tresc, re.M)
    if not m:
        return None, "w src/secrets.h nie ma #define SECRET_OTA_HASLO \"...\""
    haslo = bytes(m.group(1), "utf-8").decode("unicode_escape")
    if not haslo:
        return None, "SECRET_OTA_HASLO jest puste"
    return haslo, None


if env.subst("$UPLOAD_PROTOCOL") == "espota":  # noqa: F821
    haslo, blad = haslo_z_secrets()
    if blad:
        if "upload" in COMMAND_LINE_TARGETS:  # noqa: F821
            sys.stderr.write("ota_haslo.py: %s - wgrywanie przez WiFi sie nie uda\n" % blad)
    else:
        env.Append(UPLOADERFLAGS=["--auth=" + haslo])  # noqa: F821
        print("ota_haslo.py: haslo OTA z src/secrets.h dopisane do espota (-i %s)" % env.subst("$UPLOAD_PORT"))  # noqa: F821
