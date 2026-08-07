// ════════════════════════════════════════════════════════════════════
// Kit_Robotica_Segura_V2.ino — Sketch principal
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// CORRECCIONES — 08/07/2026:
//   [NUEVO] Esquive HC-SR04 integrado en loop() — Reto 4 pedagógico
//   [NUEVO] PIN_HC_TRIG=GPIO3 / PIN_HC_ECHO=GPIO14 (sin conflicto cámara)
//   [NUEVO] AIN2=GPIO47 / BIN2=GPIO2 — retroceso real en TB6612FNG
//   [NUEVO] PIN_STBY=GPIO21 (GPIO16=CAM_Y9 — conflicto cámara)
//   [NUEVO] I2C_SDA=GPIO38 / I2C_SCL=GPIO1 (GPIO8/9=CAM_Y4/Y3)
//   Conservado: procesarComandoJSON() con millis() — no bloquea
//   Conservado: reiniciarI2C() después de iniciarCamara()
//   Conservado: modoEmergencia() con STBY LOW/HIGH
//   Conservado: doble mqttClient.loop() para baja latencia
//
// COMPACTACIÓN — [fecha de esta sesión]:
//   [NUEVO] setLEDs() ahora recibe EstadoLed (enum en config.h) en vez
//   de un String — ver LED_ROJO_RAPIDO más abajo.
//   [NUEVO] reiniciarI2C() ya NO se llama incondicionalmente: solo
//   corre cuando la cámara realmente se inicializó (MODO_STUXNET +
//   CAMARA_ACTIVA=1), que es el único caso donde esp_camera_init()
//   toca el bus I2C. En los demás modos/retos se ahorran ~100-150ms
//   de arranque (Wire.end + 2×delay(50) + oled.begin de nuevo) que
//   antes se gastaban sin necesidad.
//
// CORRECCIÓN — [fecha de esta sesión] — PATRULLAJE AUTÓNOMO (Reto 4):
//   [BUG] La versión anterior del bloque de esquive SOLO reaccionaba
//   cuando aparecía un obstáculo: detener() → retroceder() →
//   girarDerecha() → fin. Nunca había un avanzar() en ningún punto
//   de loop() fuera de un comando MQTT explícito — así que, sin que
//   alguien enviara un comando manualmente, el robot se quedaba
//   completamente quieto y el sensor solo lo hacía retroceder/girar
//   una vez por evento, sin retomar la marcha ("los motores no se
//   mueven" reportado en consola, con distancias muy cercanas y
//   repetidas — el robot no se estaba alejando del obstáculo).
//   [FIX] Se agrega patrullaje autónomo: el robot avanza por defecto
//   en cuanto termina setup() (o en cuanto el camino queda libre tras
//   esquivar), y la maniobra ahora SÍ retoma avanzar() al final.
//   Variable _patrullando evita reemitir avanzar() en cada ciclo de
//   150ms mientras ya está en marcha, y respeta comandos MQTT
//   manuales (p. ej. el "detener" del Paso BT-3): tras un comando
//   MQTT el robot queda detenido y el patrullaje NO lo reinicia por
//   su cuenta — solo se reactiva cuando el propio sensor dispara una
//   maniobra de esquive (así no compite con la prueba de BT-3, que
//   necesita que el robot se quede detenido tras recibir "detener").
//
// FUNCIÓN PEDAGÓGICA DEL HC-SR04 EN EL RETO 4:
//
//   MODO_STUXNET bajo flood (Red Team):
//     _callbackMensaje() se llama ~500 veces/seg sin rate limiting.
//     El bloque de patrullaje/esquive en loop() no puede ejecutarse
//     a tiempo. El robot, que ya venía avanzando, sigue avanzando
//     con el último estado de motores que tenía — y CHOCA contra el
//     obstáculo porque nunca llega a revisar el sensor a tiempo.
//     El DoS es físicamente visible — el robot no puede esquivar.
//
//   MODO_ICSCERT bajo flood (Blue Team):
//     RATE_LIMIT=20 descarta 480 msgs/seg — solo procesa 20.
//     El loop() recupera su cadencia normal (~150ms por ciclo).
//     El robot detecta el obstáculo, ejecuta la maniobra de esquive
//     y retoma su recorrido — la patrulla continúa.
//     La DISPONIBILIDAD está protegida — el servicio sigue operando.
// ════════════════════════════════════════════════════════════════════

#include "config.h"
#include <WiFi.h>
#include "security.h"
#include "motores.h"
#include "sensores.h"
#include "display.h"
#include "camara.h"
#include <ArduinoJson.h>

extern int mensajesRechazados;
extern int mensajesRecibidos;   // usado por el watchdog de estancamiento TLS/TCP (ver loop())

unsigned long ultimoCmdMs  = 0;
unsigned long ultimoOledMs = 0;

// Watchdog de estancamiento TLS/TCP — ver bloque correspondiente en loop().
static unsigned long _ultimoDiagMs        = 0;
static int           _segundosSinMensajes = 0;

// Variables de detección y patrullaje — solo en Retos 1-4
#if CAMARA_ACTIVA == 0
static unsigned long _ultimoSensorMs = 0;
static bool          _detenido       = false;   // RENOMBRADO de _esquivando — Reto 4 rediseñado
static bool          _patrullando    = false;
#define DISTANCIA_DETENCION_CM 20.0f   // RENOMBRADO de DISTANCIA_ESQUIVE_CM
#define INTERVALO_SENSOR_MS    150
#define VELOCIDAD_PATRULLA     VELOCIDAD_DEFECTO   // reutiliza config.h (180)
#endif

void procesarComandoJSON(String json);


// ════════════════════════════════════════════════════════════════════
// setup()
// ════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("========================================");
  Serial.print("Kit Robotica Segura v2.0 — modo: ");
  #ifdef MODO_ICSCERT
    Serial.println("MODO_ICSCERT (seguro)");
  #else
    Serial.println("MODO_STUXNET (vulnerable)");
  #endif
  Serial.println("========================================");

  // 1. Display — primero para mostrar errores
  //    SDA=GPIO38 / SCL=GPIO1 — sin conflicto con camara
  iniciarDisplay();

  // 2. Motores
  //    PWMA=GPIO39 / AIN1=GPIO40 / AIN2=GPIO47
  //    PWMB=GPIO41 / BIN1=GPIO42 / BIN2=GPIO2
  //    STBY=GPIO21 — AIN2 y BIN2 habilitan retroceso real
  iniciarMotores();

  // 3. Sensores
  //    TRIG=GPIO3 / ECHO=GPIO14 — sin conflicto con camara
  iniciarSensores();

  // 4. WiFi
  iniciarWiFi();

  // 5. Camara
  //    MODO_STUXNET + CAMARA_ACTIVA=1 → stream sin auth (VUL-OI02)
  //    MODO_STUXNET + CAMARA_ACTIVA=0 → sin efecto (Retos 1-4)
  //    MODO_ICSCERT                   → sin efecto (OI-02 activo)
  iniciarCamara();

  // 6. Reiniciar I2C — SOLO si la cámara se inicializó de verdad.
  //    Es el único escenario (MODO_STUXNET + CAMARA_ACTIVA=1) donde
  //    esp_camera_init() toca el bus I2C. En cualquier otro modo/reto
  //    la cámara ni siquiera se inicializa, así que reiniciar el bus
  //    Wire es puro tiempo de arranque perdido (~100-150ms).
  #if defined(MODO_STUXNET) && (CAMARA_ACTIVA == 1)
    reiniciarI2C();
  #endif

  // 7. MQTT
  iniciarMQTT();

  // 8. Log URL camara
  #if defined(MODO_STUXNET) && (CAMARA_ACTIVA == 1)
  if (camaraActiva()) {
    Serial.println("[CAM] Stream: http://" + WiFi.localIP().toString() + "/stream");
    Serial.println("[CAM] Foto  : http://" + WiFi.localIP().toString() + "/foto");
    Serial.println("[CAM] VUL-OI02 activa — sin autenticacion.");
  }
  #endif

  // 9. Primera actualizacion del OLED
  {
    String ip     = WiFi.localIP().toString();
    String estado = mqttClient.connected() ? "OK" : "RECON";
    actualizarOLED(ip, estado, 0);
    Serial.println("[OLED] MQTT:" + estado + " IP:" + ip);
  }

  ultimoCmdMs  = millis();
  ultimoOledMs = millis();

  Serial.println("[SETUP] Completo. Entrando al loop.");
  Serial.println("========================================");
}


// ════════════════════════════════════════════════════════════════════
// loop()
// ════════════════════════════════════════════════════════════════════
void loop() {

  // 1. Mantener WiFi + MQTT
  bool conectado = mantenerConexion();

  // 2. Drenar completamente el cliente MQTT — no solo un paquete
  //    (ver drenarMQTT() en security.cpp — diagnóstico de flood sostenido)
  drenarMQTT();

  // 3. Resetear Fail-Safe timer mientras MQTT este conectado
  if (conectado) {
    ultimoCmdMs = millis();
  }

  // 4. Fail-Safe — PTI-09: motores paran en <500ms si se pierde MQTT
  if (millis() - ultimoCmdMs > TIMEOUT_MQTT_MS) {
    modoEmergencia();
    setLEDs(LED_ROJO_RAPIDO);
  }

  // ═══════════════════════════════════════════════════════════════
  // 5. HC-SR04 — patrullaje autónomo + detención con confirmación MQTT
  //
  // REDISEÑADO — [fecha de esta sesión] — Reto 4:
  //   Versión anterior: el robot ESQUIVABA (retroceder+girar) al
  //   detectar un obstáculo, y la evidencia era visual (fotografiar
  //   el momento exacto del esquive, o que siguiera avanzando 30s
  //   sin chocar bajo flood sostenido). Eso exigía que la conexión
  //   TLS de ICSCERT aguantara ininterrumpida por muchos segundos —
  //   y encontramos que WiFiClientSecure puede estancarse bajo
  //   flood sostenido por razones ajenas al firmware (ver drenarMQTT
  //   y el watchdog más abajo). Esto hacía la demo poco confiable.
  //
  //   Versión nueva: el robot avanza en línea recta y, al llegar al
  //   obstáculo, simplemente SE DETIENE y PUBLICA la distancia
  //   medida en TOPIC_SENSOR. La evidencia ya no es un gesto físico
  //   fugaz, sino un mensaje persistente visible en MQTT Explorer:
  //
  //   Red Team — STUXNET bajo flood (flood_reto4.py):
  //     loop() saturado por el callback → nunca llega a este bloque
  //     con la frecuencia necesaria → el robot NUNCA llega a
  //     publicar la distancia, aunque choque físicamente contra el
  //     obstáculo. El SILENCIO en TOPIC_SENSOR es la evidencia del
  //     DoS — los estudiantes ven en MQTT Explorer que no llega nada.
  //
  //   Blue Team — ICSCERT bajo flood (flood_reto4_icscert.py):
  //     RATE_LIMIT=20 libera loop() → el robot detecta el obstáculo,
  //     se detiene y SÍ publica la distancia en TOPIC_SENSOR, aunque
  //     el flood siga activo. Solo se necesita que la conexión
  //     aguante los pocos segundos entre que arranca el patrullaje
  //     y llega al obstáculo — no los 30 segundos completos del
  //     flood — así que no depende del estancamiento TLS visto bajo
  //     flood sostenido.
  //
  //   Para repetir la prueba: reenviar {"accion":"explorar"} — esto
  //   reinicia _detenido a false y el robot vuelve a patrullar.
  // ═══════════════════════════════════════════════════════════════
  #if CAMARA_ACTIVA == 0 && SENSOR_ACTIVO == 1
  if (!_detenido && (millis() - _ultimoSensorMs > INTERVALO_SENSOR_MS)) {
    _ultimoSensorMs = millis();
    float distancia = medirDistanciaCm();

    if (distancia < DISTANCIA_DETENCION_CM && distancia > 2.0f) {
      detener();
      _detenido    = true;
      _patrullando = false;

      Serial.print("[SENSOR] Detenido a ");
      Serial.print(distancia, 1);
      Serial.println("cm — publicando confirmacion.");

      // Publicar la distancia de detencion en TOPIC_SENSOR.
      // Esta es la evidencia del Reto 4: si este mensaje llega o
      // no a MQTT Explorer durante el flood.
      char payload[64];
      snprintf(payload, sizeof(payload),
               "{\"evento\":\"detenido\",\"distancia_cm\":%.1f}", distancia);
      mqttClient.publish(TOPIC_SENSOR, payload);

      ultimoCmdMs = millis();  // evitar Fail-Safe durante la maniobra
      Serial.println("[SENSOR] Confirmacion publicada.");
    }

    // Patrullaje: si el camino está libre y el robot no está ya en
    // marcha ni detenido por un obstáculo, retoma o inicia el avance.
    if (!_patrullando && !_detenido) {
      avanzar(VELOCIDAD_PATRULLA);
      _patrullando = true;
      ultimoCmdMs  = millis();
      Serial.println("[SENSOR] Recorrido iniciado/retomado.");
    }
  }
  #endif

  // ═══════════════════════════════════════════════════════════════
  // Watchdog de estancamiento TLS/TCP.
  //
  // Bajo flood sostenido se observó que el socket puede dejar de
  // recibir datos por completo (mensajesRecibidos=0) durante varios
  // segundos seguidos, mientras mqttClient sigue creyendo que está
  // "conectado" (mqtt_state=0) y sin fuga de memoria de por medio
  // (heap estable) — es decir, ni PubSubClient ni el heap reportan
  // ningún error consultable desde la aplicación. drenarMQTT()
  // tampoco lo resuelve de forma permanente. La única salida
  // práctica encontrada es forzar una reconexión completa del
  // socket cuando el estancamiento dura varios segundos seguidos.
  //
  // CORREGIDO — [fecha de esta sesión]: la reconexión forzada SOLO
  // se aplica en MODO_ICSCERT. En MODO_ICSCERT queremos que la
  // conexión se recupere — esa recuperación ES la demostración de
  // que el rate limiting protege el servicio. En MODO_STUXNET
  // queremos exactamente lo contrario: que el robot falle por
  // completo y SIN recuperarse, porque el silencio total en
  // TOPIC_SENSOR es la evidencia del DoS para el Red Team. Se
  // observó que, sin esta restricción, la reconexión forzada le
  // daba al robot "ventanas de respiro" incluso bajo STUXNET,
  // permitiendo que ocasionalmente publicara una confirmación —
  // contradiciendo la evidencia que el Paso 2-3 del Red Team espera.
  // ═══════════════════════════════════════════════════════════════
  if (millis() - _ultimoDiagMs >= 1000) {
    _ultimoDiagMs = millis();

    if (mensajesRecibidos == 0) {
      _segundosSinMensajes++;
    } else {
      _segundosSinMensajes = 0;
    }
    #ifdef MODO_ICSCERT
    if (_segundosSinMensajes >= 3 && mqttClient.connected()) {
      Serial.println("[WATCHDOG] 3s sin mensajes con conexion 'activa' — forzando reconexion TCP/TLS.");
      mqttClient.disconnect();
      _segundosSinMensajes = 0;
    }
    #endif

    mensajesRecibidos = 0;   // reinicia para medir el siguiente segundo
  }

  // 6. Actualizar OLED cada 2000ms
  if (millis() - ultimoOledMs > INTERVALO_OLED_MS) {
    ultimoOledMs = millis();
    String ip     = WiFi.localIP().toString();
    String estado = conectado ? "OK" : "RECON";
    actualizarOLED(ip, estado, mensajesRechazados);
  }
}


// ════════════════════════════════════════════════════════════════════
// procesarComandoJSON(String json)
// Llamada desde _callbackMensaje() en security.cpp via extern.
//
// Formato JSON — "duracion" es OPCIONAL:
//   {"accion":"avanzar","velocidad":200}
//   {"accion":"avanzar","velocidad":200,"duracion":3000}
//   {"accion":"retroceder","velocidad":150,"duracion":500}
//   {"accion":"detener"}
//   {"accion":"explorar"}   ← NUEVO, solo Reto 4 (SENSOR_ACTIVO=1)
//
// "retroceder" ahora funciona realmente gracias a AIN2/BIN2.
// "duracion" usa millis() — no bloquea el Fail-Safe ni el MQTT.
//
// NOTA — [fecha de esta sesión]: control remoto del patrullaje.
//   "detener"  → para los motores YA. _patrullando NO se toca, así
//                que el patrullaje autónomo NO retoma la marcha por
//                su cuenta — el robot se queda detenido hasta que
//                llegue otro comando o el sensor detecte un obstáculo.
//                Esto es lo que permite que el Paso BT-3 funcione:
//                el robot debe quedarse detenido tras el "detener".
//   "explorar" → (re)inicia el patrullaje autónomo de inmediato:
//                arranca avanzar(VELOCIDAD_PATRULLA) y marca
//                _patrullando=true, exactamente lo mismo que hace el
//                bloque 5 de loop() al arrancar. Útil para el Paso 1
//                del Red Team: en vez de solo enviar un "avanzar" de
//                prueba, puede enviar "explorar" para que el robot
//                arranque su recorrido autónomo bajo control MQTT y
//                así demostrar, con el mismo comando, cómo se inicia
//                y cómo se detiene desde MQTT Explorer.
//   Ambas acciones solo existen cuando CAMARA_ACTIVA=0 y
//   SENSOR_ACTIVO=1 (Reto 4) — en los demás retos, "explorar" no
//   está definida y el robot ignora ese payload (comportamiento
//   igual al de cualquier "accion" desconocida: return sin hacer
//   nada).
// ════════════════════════════════════════════════════════════════════
void procesarComandoJSON(String json) {

  StaticJsonDocument<128> doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return;

  String accion   = doc["accion"]    | "";
  int    vel      = doc["velocidad"] | VELOCIDAD_DEFECTO;
  int    duracion = doc["duracion"]  | 0;

  vel      = constrain(vel, 0, 255);
  duracion = constrain(duracion, 0, 10000);

  if      (accion == "avanzar")    avanzar(vel);
  else if (accion == "retroceder") retroceder(vel);
  else if (accion == "izquierda")  girarIzquierda(vel);
  else if (accion == "derecha")    girarDerecha(vel);
  else if (accion == "detener")    detener();
  #if CAMARA_ACTIVA == 0 && SENSOR_ACTIVO == 1
  else if (accion == "explorar") {
    avanzar(VELOCIDAD_PATRULLA);
    _patrullando = true;
    _detenido    = false;   // RENOMBRADO de _esquivando — permite repetir la prueba
    Serial.println("[CMD] explorar — patrullaje autonomo iniciado por MQTT.");
  }
  #endif
  else return;

  ultimoCmdMs = millis();
  setLEDs(COLOR_NORMAL);

  Serial.print("[CMD] "); Serial.print(accion);
  Serial.print(" vel="); Serial.print(vel);

  if (duracion > 0 && accion != "detener") {
    Serial.print(" dur="); Serial.print(duracion); Serial.println("ms");

    unsigned long inicio = millis();
    while (millis() - inicio < (unsigned long)duracion) {
      if (mqttClient.connected()) mqttClient.loop();
      ultimoCmdMs = millis();
      delay(10);
    }
    detener();
    setLEDs(COLOR_NORMAL);
    Serial.println("[CMD] Duracion completada.");
  } else {
    Serial.println();
  }
}
