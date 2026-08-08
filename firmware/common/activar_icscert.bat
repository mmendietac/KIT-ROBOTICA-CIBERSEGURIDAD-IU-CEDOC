@echo off
title Mosquitto ICSCERT — Puerto 8883 TLS
color 0A
echo.
echo ============================================================
echo   BROKER ICSCERT — Puerto 8883 TLS
echo   Kit de Robotica Educativa en Ciberseguridad v2.0
echo   ESCOM / CEDOC 2025
echo ============================================================
echo.
echo   TLS activo  — VUL-1 cerrada
echo   Auth activa — VUL-2 cerrada
echo   Usuario:    robot01   robot02
echo   Contrasena: T0k3n_Segur0!   T0k3n_02_S3gur0!
echo.
echo   Usar en: Blue Team Retos 1, 2, 3 y 4
echo   flood_reto4_icscert.py tambien conecta aqui (Reto 4 BT-4)
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

:: Cerrar instancia anterior
echo.
echo [1/5] Cerrando broker anterior...
taskkill /f /im mosquitto.exe >nul 2>&1
timeout /t 2 /nobreak >nul
echo       OK

:: Verificar archivo de configuracion
echo [2/5] Verificando configuracion...
if not exist "C:\mosquitto\mosquitto_icscert.conf" (
    echo [ERROR] No se encuentra C:\mosquitto\mosquitto_icscert.conf
    pause
    exit /b 1
)
echo       OK

:: Verificar certificados TLS
echo [3/5] Verificando certificados TLS...
if not exist "C:\mosquitto\certs\ca.crt" (
    echo [ERROR] No se encuentra C:\mosquitto\certs\ca.crt
    pause
    exit /b 1
)
if not exist "C:\mosquitto\certs\server.crt" (
    echo [ERROR] No se encuentra C:\mosquitto\certs\server.crt
    pause
    exit /b 1
)
if not exist "C:\mosquitto\certs\server.key" (
    echo [ERROR] No se encuentra C:\mosquitto\certs\server.key
    pause
    exit /b 1
)
echo       OK — ca.crt server.crt server.key encontrados

:: Verificar archivo de passwords
echo [4/5] Verificando passwords...
if not exist "C:\mosquitto\passwd" (
    echo [ERROR] No se encuentra C:\mosquitto\passwd
    echo.
    echo Crear el archivo con estos comandos en CMD como admin:
    echo   cd "C:\Program Files\mosquitto"
    echo   mosquitto_passwd -b C:\mosquitto\passwd robot01 T0k3n_Segur0!
    echo   mosquitto_passwd -b C:\mosquitto\passwd robot02 T0k3n_02_S3gur0!
    echo.
    pause
    exit /b 1
)
echo       OK

:: Crear directorio de logs si no existe
if not exist "C:\mosquitto\logs" mkdir "C:\mosquitto\logs"

echo [5/5] Iniciando broker ICSCERT...
echo.
echo ============================================================
echo   Puerto 8883 activo — TLS + autenticacion requerida
echo   Verificar: netstat -ano ^| findstr 8883
echo   Para detener: cerrar esta ventana
echo ============================================================
echo.

:: Bucle de reinicio automatico — si el flood lo derriba, resurge en 3s
:reiniciar
echo [%TIME%] Iniciando broker ICSCERT...
"C:\Program Files\mosquitto\mosquitto.exe" -c C:\mosquitto\mosquitto_icscert.conf -v
echo.
echo [%TIME%] El broker se detuvo. Reiniciando en 3 segundos...
timeout /t 3 /nobreak >nul
goto reiniciar
