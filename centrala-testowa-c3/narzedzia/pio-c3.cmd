@echo off
rem ===========================================================================
rem  PlatformIO z OSOBNYM katalogiem rdzenia (core dir) dla tego projektu.
rem
rem  Po co: na tym PC sa DWIE platformy espressif32 —
rem    * espressif32 @ 6.9.0      (ten projekt; wymaga tool-esptoolpy 1.40501.0)
rem    * espressif32 @ 55.3.38    (pioarduino, prawdziwa centrala S3, Arduino 3.3.8;
rem                                wymaga tool-esptoolpy 5.2.0)
rem  PlatformIO trzyma JEDNA wersje pakietu na nazwe w
rem  %USERPROFILE%\.platformio\packages\tool-esptoolpy — wiec oba projekty
rem  podmieniaja sobie ten sam katalog. Build trafia w moment podmiany i pada:
rem    can't open file '...\packages\tool-esptoolpy\esptool.py': [Errno 2]
rem
rem  Ten skrypt daje projektowi wlasny core dir, wiec tamten katalog nie jest
rem  juz wspoldzielony i nic sie nie podmienia.
rem
rem  Uzycie (PowerShell, w katalogu centrala-testowa-c3):
rem    .\narzedzia\pio-c3.cmd run -t upload
rem    .\narzedzia\pio-c3.cmd run -e esp32 -t upload
rem    .\narzedzia\pio-c3.cmd run -e esp32s3 -t upload
rem    .\narzedzia\pio-c3.cmd device monitor
rem
rem  Uwaga: pierwszy build pobierze platforme + toolchain (~350 MB, 10-20 min).
rem  Katalog core dir lezy POZA repo: %USERPROFILE%\.platformio-centrala-c3
rem ===========================================================================
setlocal
if "%PLATFORMIO_CORE_DIR%"=="" set PLATFORMIO_CORE_DIR=%USERPROFILE%\.platformio-centrala-c3
echo [pio-c3] PLATFORMIO_CORE_DIR=%PLATFORMIO_CORE_DIR%
pio %*
