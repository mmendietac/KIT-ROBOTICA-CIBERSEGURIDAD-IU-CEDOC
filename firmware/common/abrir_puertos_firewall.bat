@echo off
title Abrir Puertos Mosquitto en Firewall Windows
color 0E
echo.
echo ============================================================
echo   CONFIGURACION INICIAL — Puertos Mosquitto
echo   Kit de Robotica Educativa en Ciberseguridad v2.0
echo   ESCOM / CEDOC 2025
echo ============================================================
echo.
echo   Este script se ejecuta UNA SOLA VEZ durante la instalacion.
echo   Abre los puertos 1883 y 8883 en el Firewall de Windows
echo   para que los robots y computadores de estudiantes puedan
echo   conectarse al broker Mosquitto.
echo.
echo   SIN este paso: TimeoutError al ejecutar flood_reto4.py
echo   y MQTT Explorer no puede conectar desde el computador
echo   del estudiante.
echo ============================================================

:: Verificar administrador
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Ejecutar como Administrador.
    echo Clic derecho ^> "Ejecutar como administrador"
    pause
    exit /b 1
)

echo.
echo [1/4] Abriendo puerto 1883 (STUXNET — sin TLS)...
netsh advfirewall firewall add rule ^
    name="Mosquitto MQTT 1883" ^
    dir=in action=allow protocol=TCP localport=1883 ^
    >nul 2>&1
if %errorlevel% equ 0 (echo       OK) else (echo       Ya existia o error — continuar)

echo [2/4] Abriendo puerto 8883 (ICSCERT — con TLS)...
netsh advfirewall firewall add rule ^
    name="Mosquitto MQTT 8883 TLS" ^
    dir=in action=allow protocol=TCP localport=8883 ^
    >nul 2>&1
if %errorlevel% equ 0 (echo       OK) else (echo       Ya existia o error — continuar)

echo [3/4] Verificando reglas creadas...
echo.
netsh advfirewall firewall show rule name="Mosquitto MQTT 1883" | findstr "Enabled"
netsh advfirewall firewall show rule name="Mosquitto MQTT 8883 TLS" | findstr "Enabled"

echo.
echo [4/4] Verificando puertos despues de abrir Mosquitto...
echo       (Ejecutar activar_stuxnet.bat primero para ver el 1883)
echo.
netstat -ano | findstr "1883"
netstat -ano | findstr "8883"

echo.
echo ============================================================
echo   Puertos configurados correctamente.
echo   Ahora ejecutar activar_stuxnet.bat o activar_icscert.bat.
echo ============================================================
echo.
pause
