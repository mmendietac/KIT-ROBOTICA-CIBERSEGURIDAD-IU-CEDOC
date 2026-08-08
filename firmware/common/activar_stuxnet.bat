@echo off
title Mosquitto STUXNET — Puerto 1883 — VUL-1 VUL-2 activas
color 0C
echo.
echo ============================================================
echo   BROKER STUXNET — Puerto 1883 SIN TLS
echo   Kit de Robotica Educativa en Ciberseguridad v2.0
echo   ESCOM / CEDOC 2025
echo ============================================================
echo.
echo   VUL-1 activa: tráfico MQTT legible con Wireshark
echo   VUL-2 activa: cualquiera puede conectar sin credenciales
echo.
echo   Usar en: Retos 1, 2, 3 y 4 Red Team
echo   NO cerrar esta ventana durante el reto.
echo ============================================================

:: Verificar que se ejecuta como administrador
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Ejecutar como Administrador.
    echo Clic derecho en el archivo ^> "Ejecutar como administrador"
    echo.
    pause
    exit /b 1
)

:: Cerrar cualquier instancia de Mosquitto activa
echo.
echo [1/3] Cerrando broker anterior...
taskkill /f /im mosquitto.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo       OK

:: Verificar que el archivo de configuracion existe
echo [2/3] Verificando configuracion...
if not exist "C:\mosquitto\mosquitto_stuxnet.conf" (
    echo.
    echo [ERROR] No se encuentra C:\mosquitto\mosquitto_stuxnet.conf
    echo Copiar el archivo mosquitto_stuxnet.conf a C:\mosquitto\
    echo.
    pause
    exit /b 1
)
echo       OK — C:\mosquitto\mosquitto_stuxnet.conf encontrado

:: Crear directorio de logs si no existe
if not exist "C:\mosquitto\logs" mkdir "C:\mosquitto\logs"

echo [3/3] Iniciando broker STUXNET...
echo.
echo ============================================================
echo   Puerto 1883 activo — allow_anonymous true
echo   Listo para Retos 1, 2, 3 y 4 Red Team
echo   Para detener: cerrar esta ventana o Ctrl+C
echo ============================================================
echo.

:: Bucle de reinicio automatico
:reiniciar
echo [%TIME%] Iniciando broker STUXNET...
"C:\Program Files\mosquitto\mosquitto.exe" -c C:\mosquitto\mosquitto_stuxnet.conf -v
echo.
echo [%TIME%] El broker se detuvo. Reiniciando en 3 segundos...
timeout /t 3 /nobreak >nul
goto reiniciar
