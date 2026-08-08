# flood_reto4_icscert.py v5 — Reto 4 Paso BT-3
# Kit de Robotica Educativa en Ciberseguridad v2.0
# ESCOM / CEDOC 2025
#
# CORREGIDO — [fecha de esta sesion] — PAUSA_ENTRE_MENSAJES configurable:
#   Con PAYLOAD="detener" y sin pausa (o pausa de 1ms), este script
#   alcanza ~480-500 mensajes/seg reales contra el broker ICSCERT.
#   Con RATE_LIMIT=20 en el firmware, esos 20 espacios de cada
#   ventana de 1 segundo se agotan en los primeros ~40ms de cada
#   segundo. Un comando manual publicado desde MQTT Explorer (por
#   ejemplo "explorar" durante el Paso BT-3) llega en un instante
#   aleatorio dentro de ese segundo, asi que su probabilidad de caer
#   dentro de esos primeros 40ms es de solo ~4% (20 de ~500) -- casi
#   siempre pierde la carrera contra el propio flood por ese cupo.
#   PAUSA_ENTRE_MENSAJES baja el ritmo del flood a un valor mas bajo
#   (por defecto ~100 msg/seg) para que la ventana de aceptacion siga
#   demostrando el rate limiting (80% de los mensajes del flood se
#   siguen rechazando: Rech: sigue subiendo) pero le da al comando
#   manual del estudiante una probabilidad razonable (~20% por
#   intento) de colarse dentro de la ventana de 20 aceptados.

import paho.mqtt.client as mqtt
import ssl
import time
import threading

IP_BROKER    = "192.168.10.105"
PUERTO       = 8883
TOPIC        = "robot/armonia/cmd"
PAYLOAD      = '{"accion":"detener"}'
DURACION_SEG = 30
USUARIO      = "robot01"
CONTRASENA   = "T0k3n_Segur0!"
CA_CERT      = r"C:\mosquitto\certs\ca.crt"

# NUEVO -- ajustar este valor segun lo que se quiera demostrar:
#   0.001 (1ms)  -> ~500 msg/seg -- flood maximo, casi imposible que
#                   un comando manual se cuele (bueno para medir el
#                   techo real de la red, no para el Paso BT-3)
#   0.01  (10ms) -> ~90-100 msg/seg -- RECOMENDADO para BT-3: sigue
#                   mostrando rechazo (Rech sube) y da ~20% de chance
#                   por intento a un comando manual
#   0.02  (20ms) -> ~45-50 msg/seg -- aun mas facil de intentar
#                   (~40% de chance por intento), rate limiting
#                   todavia visible (menos de la mitad se acepta)
PAUSA_ENTRE_MENSAJES = 0.01

# ── Contador compartido entre hilos ───────────────────────────
contador = 0
corriendo = True

def hilo_flood(cliente):
    """Hilo dedicado SOLO al flood — no bloquea el hilo principal."""
    global contador, corriendo
    inicio = time.time()
    fallos_consecutivos = 0
    while corriendo and (time.time() - inicio < DURACION_SEG):
        try:
            info = cliente.publish(TOPIC, PAYLOAD)
            # rc=0 (MQTT_ERR_SUCCESS) significa que se encolo bien.
            # Cualquier otro codigo indica que algo fallo silenciosamente
            # (por ejemplo, socket caido) sin lanzar excepcion.
            if info.rc != 0:
                fallos_consecutivos += 1
                if fallos_consecutivos == 1 or fallos_consecutivos % 50 == 0:
                    print(f"\n[AVISO] publish() devolvio rc={info.rc} (fallo #{fallos_consecutivos})")
            else:
                fallos_consecutivos = 0
            contador += 1
        except Exception as e:
            print(f"\n[ERROR EN HILO FLOOD] {type(e).__name__}: {e}")
            print("Intentando reconectar...")
            try:
                cliente.reconnect()
                print("Reconectado exitosamente. Continuando el flood.")
            except Exception as e2:
                print(f"[ERROR] Reconexion fallida: {type(e2).__name__}: {e2}")
                time.sleep(1)
        time.sleep(PAUSA_ENTRE_MENSAJES)

    if not corriendo:
        print("\n[HILO FLOOD] Detenido manualmente.")
    elif time.time() - inicio >= DURACION_SEG:
        print("\n[HILO FLOOD] Termino por duracion completa (normal).")

# ── Conexion ──────────────────────────────────────────────────
def _on_disconnect(client, userdata, *args):
    print(f"\n[DESCONEXION] El cliente se desconecto del broker. Detalles: {args}")

try:
    cliente = mqtt.Client()
    cliente.username_pw_set(USUARIO, CONTRASENA)
    cliente.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLSv1_2)
    cliente.tls_insecure_set(False)
    cliente.on_disconnect = _on_disconnect
    cliente.connect(IP_BROKER, PUERTO, 60)
    cliente.loop_start()
    time.sleep(1)  # esperar conexion TLS establecida
except Exception as e:
    print(f"\nERROR: {e}")
    print("Verificar: activar_icscert.bat activo + ca.crt en C:\\mosquitto\\certs\\")
    print("Puertos 1883 y 8883 abiertos en el firewall del docente.")
    exit(1)

# ── Encabezado ────────────────────────────────────────────────
print("")
print("╔══════════════════════════════════════════════════════╗")
print("║   flood_reto4_icscert.py — Kit Ciberseguridad v2.0  ║")
print("╠══════════════════════════════════════════════════════╣")
print(f"║  Objetivo : {IP_BROKER}:{PUERTO}                    ║")
print(f"║  TLS      : ON  |  Auth: {USUARIO:<28}║")
print(f"║  Duracion : {DURACION_SEG} segundos                            ║")
print(f"║  Pausa    : {PAUSA_ENTRE_MENSAJES*1000:.0f}ms entre mensajes (~{1/PAUSA_ENTRE_MENSAJES:.0f} msg/seg objetivo)     ║")
print("╠══════════════════════════════════════════════════════╣")
print("║  DURANTE EL FLOOD enviar desde MQTT Explorer:        ║")
print('║  {"accion":"explorar"}                               ║')
print("║  El robot debe reaccionar en ALGUNO de los intentos  ║")
print("║  (no necesariamente el primero -- sigue compitiendo  ║")
print("║  por el cupo de RATE_LIMIT=20/seg contra este flood) ║")
print("╚══════════════════════════════════════════════════════╝")
print("")
print("Flood activo... (Ctrl+C para detener antes)")
print("")

# ── Flood en hilo separado ────────────────────────────────────
inicio_total = time.time()
t = threading.Thread(target=hilo_flood, args=(cliente,))
t.start()

# Mostrar progreso cada 5 segundos
ultimo_reporte = time.time()
while t.is_alive():
    time.sleep(0.1)
    if time.time() - ultimo_reporte >= 5:
        elapsed = time.time() - inicio_total
        rate = contador / elapsed if elapsed > 0 else 0
        print(f"  {elapsed:.0f} seg — {contador} mensajes — {rate:.0f} msgs/seg")
        ultimo_reporte = time.time()

corriendo = False
t.join()

# ── Fin ───────────────────────────────────────────────────────
cliente.loop_stop()
cliente.disconnect()

duracion_real = time.time() - inicio_total
print("")
print("═══════════ ESTADISTICAS DEL FLOOD ═════════════")
print(f"  Mensajes enviados  : {contador}")
print(f"  Duracion real      : {duracion_real:.1f} seg")
print(f"  Mensajes/segundo   : {contador/duracion_real:.0f}")
print(f"  Puerto             : {PUERTO} (TLS)")
print("═════════════════════════════════════════════════")
print("")
print("Anotar los valores en la tabla BT-5 del cuadernillo.")