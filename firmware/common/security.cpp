/* Kit Robotica Ciberseguridad ESCOM/CEDOC
* Autores: Milton Alvaro Mendieta Cifuentes, Juan Pablo Pedroche Quevedo 
* Director: Dr. Arles Prieto * Licencia MIT (c) 2026 
* Modulo: Control de motores 
*/
// OPTIMIZACIONES DE LATENCIA — 01/07/2026:
//   - keepAlive reducido de 15s a 3s: el broker detecta más rápido
//     las desconexiones y el cliente mantiene el TCP más activo.
//   - socketTimeout reducido de 5s a 1s: si el broker no responde
//     en 1s, no espera 5s bloqueando el loop.
//   - Intervalo de reconexión reducido de 5000ms a 1000ms: el robot
//     vuelve a estar disponible en 1s en vez de 5s.
//   - mqttClient.loop() se llama siempre, incluso durante intentos
//     de reconexión fallidos, para no perder mensajes en tránsito.
//   - ultimoCmdMs se resetea al reconectar exitosamente.
//
// NOTA — [fecha de esta sesión]:
//   iniciarMQTT() todavía tenía setKeepAlive(30) y setSocketTimeout(2),
//   que contradecían el comentario de arriba (3s / 1s). Este es un
//   problema de INTEGRIDAD, no de compactación, y no se tocó en esta
//   pasada — queda pendiente decidir el valor real deseado antes de
//   volver a compilar. Ver aviso en la respuesta del chat.
//
// COMPACTACIÓN — [fecha de esta sesión]:
//   _conectarMQTT() repetía setServer()/setCallback()/subscribe()/
//   ultimoCmdMs=millis() en los dos bloques #ifdef (ICSCERT/STUXNET).
//   Se dejó fuera del #ifdef lo común; dentro de cada rama solo queda
//   lo que realmente cambia: la llamada a connect() (con o sin
//   credenciales) y sus mensajes de log específicos. El color de LED
//   de éxito ahora usa COLOR_NORMAL (config.h) en vez de repetir
//   "VERDE_FIJO"/"ROJO_PARPADEANTE" — ya eran el mismo valor.
//   setLEDs() ahora recibe valores del enum EstadoLed en vez de
//   literales String ("AMARILLO_PARPADEANTE" → LED_AMARILLO_PARPADEANTE,
//   etc.) — ver config.h y display.h/cpp.
// ════════════════════════════════════════════════════════════════════

#include "security.h"
#include "config.h"
#include "display.h"
#include <WiFi.h>
#include <PubSubClient.h>

// ── Cliente MQTT según el modo ────────────────────────────────────
#ifdef MODO_ICSCERT
  #include <WiFiClientSecure.h>
  #include "certs.h"
  static WiFiClientSecure _clienteSeguro;
  PubSubClient mqttClient(_clienteSeguro);
#endif

#ifdef MODO_STUXNET
  static WiFiClient _clientePlano;
  PubSubClient mqttClient(_clientePlano);
#endif

// ── Variables internas ────────────────────────────────────────────
int mensajesRechazados = 0;
int mensajesRecibidos = 0;    // NUEVO — diagnóstico temporal Reto 4

static unsigned long _ultimoSegundo      = 0;
static unsigned long _ultimaConexionMQTT = 0;
static int           _msgEsteSegundo     = 0;

// ── Extern — timer del Fail-Safe definido en el .ino ─────────────
extern unsigned long ultimoCmdMs;

// ════════════════════════════════════════════════════════════════════
// _callbackMensaje()
// Llamada por mqttClient.loop() cuando llega un mensaje MQTT.
// Se ejecuta de forma síncrona dentro de mqttClient.loop(),
// así que cualquier delay() aquí bloquearía el loop principal.
// ════════════════════════════════════════════════════════════════════
static void _callbackMensaje(char* topic, byte* payload, unsigned int len) {
  mensajesRecibidos++;   // NUEVO — cuenta TODO lo que realmente llega, se acepte o no
  unsigned long ahora = millis();

  // Rate limiting — activo en ambos modos
  if (ahora - _ultimoSegundo >= 1000UL) {
    _ultimoSegundo  = ahora;
    _msgEsteSegundo = 0;
  }

  if (++_msgEsteSegundo > RATE_LIMIT) {
    mensajesRechazados++;
    setLEDs(LED_AMARILLO_PARPADEANTE);
    return;
  }

  // Construir el payload como String
  String msg = "";
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

  extern void procesarComandoJSON(String json);
  procesarComandoJSON(msg);
}

// ════════════════════════════════════════════════════════════════════
// _conectarMQTT()
// Intenta UNA conexión al broker.
// No bloquea más de socketTimeout.
//
// Lo común a ambos modos (setServer/setCallback, y en éxito:
// subscribe + LED de color normal + reset del timer Fail-Safe)
// vive fuera del #ifdef. Dentro de cada rama solo queda lo que
// realmente difiere: la llamada connect() y sus logs específicos.
// ════════════════════════════════════════════════════════════════════
static bool _conectarMQTT() {
  if (WiFi.status() != WL_CONNECTED) return false;

  mqttClient.setServer(BROKER_IP, MQTT_PUERTO);
  mqttClient.setCallback(_callbackMensaje);

  bool ok = false;

  #ifdef MODO_ICSCERT
    _clienteSeguro.setCACert(CA_CERT);
    _clienteSeguro.setInsecure();

    ok = mqttClient.connect(
      ROBOT_ID,
      MQTT_USUARIO,
      MQTT_CONTRASENA,
      TOPIC_ESTADO, 1, true,
      "{\"estado\":\"offline\"}"
    );

    if (ok) {
      Serial.println("[MQTT] Conectado con TLS + autenticacion. (ICSCERT)");
    }
  #endif

  #ifdef MODO_STUXNET
    if (strlen(MQTT_USUARIO) > 0) {
      // Reto 2 Blue Team: broker ya exige autenticación, aunque siga sin TLS
      ok = mqttClient.connect(ROBOT_ID, MQTT_USUARIO, MQTT_CONTRASENA);
      if (ok) {
        Serial.println("[MQTT] Conectado SIN TLS, CON autenticacion. (STUXNET + credenciales)");
      }
    } else {
      ok = mqttClient.connect(ROBOT_ID);  // Sin credenciales — VUL-2
      if (ok) {
        mqttClient.publish(TOPIC_SECRETO, "BANDERA_RETO1_ENCONTRADA", false);
        Serial.println("[MQTT] Conectado SIN TLS ni autenticacion. (STUXNET)");
        Serial.println("[MQTT] VUL-1 y VUL-2 activas.");
      }
    }
  #endif

  if (ok) {
    mqttClient.subscribe(TOPIC_CMD);
    setLEDs(COLOR_NORMAL);   // LED_VERDE_FIJO en ICSCERT, LED_ROJO_PARPADEANTE en STUXNET
    ultimoCmdMs = millis();  // evitar Fail-Safe al reconectar
  } else {
    Serial.print("[MQTT] FALLO. Codigo de error: ");
    Serial.println(mqttClient.state());
    #ifdef MODO_STUXNET
      Serial.println("[MQTT] Verificar que el broker Mosquitto este activo.");
      Serial.println("[MQTT] Ejecutar activar_stuxnet.bat como administrador.");
    #endif
  }

  return ok;
}

// ════════════════════════════════════════════════════════════════════
// drenarMQTT()
// NUEVO — [fecha de esta sesión] — diagnóstico de flood sostenido.
//
// Hallazgo: bajo flood, mensajesRecibidos/seg cae a 0 después de
// ~10 mensajes y se queda ahí indefinidamente, SIN IMPORTAR la
// velocidad del flood (pasa igual a 500 msg/seg que a 100 msg/seg).
// El log de Mosquitto confirma que el broker sigue enviando sin
// ningún error durante ese tiempo — el robot simplemente deja de
// leer el socket. Firma de una ventana TCP de recepción cerrada:
// el buffer se llena porque WiFiClientSecure + PubSubClient dejan
// datos ya descifrados sin drenar si solo se llama loop() una vez
// por iteración de loop() Arduino.
//
// Este drenado fuerza múltiples llamadas a mqttClient.loop() en la
// misma iteración mientras el socket siga reportando datos
// disponibles, en vez de una sola llamada que puede dejar el
// backlog sin consumir.
// ════════════════════════════════════════════════════════════════════
void drenarMQTT() {
  if (!mqttClient.connected()) return;

  int intentos = 0;

  #ifdef MODO_ICSCERT
    while (_clienteSeguro.available() > 0 && intentos < 100) {
      mqttClient.loop();
      intentos++;
    }
  #endif

  #ifdef MODO_STUXNET
    while (_clientePlano.available() > 0 && intentos < 100) {
      mqttClient.loop();
      intentos++;
    }
  #endif

  mqttClient.loop();  // llamada final, igual que antes
}

// ════════════════════════════════════════════════════════════════════
// iniciarWiFi()
// ════════════════════════════════════════════════════════════════════
void iniciarWiFi() {
  Serial.print("[WiFi] Conectando a: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.println("[WiFi] Conectado. IP: " + WiFi.localIP().toString());
    #ifdef MODO_STUXNET
      Serial.println("[WiFi] MODO_STUXNET — red Robot-Lab-01 (VUL-3: pwd debil).");
    #endif
    #ifdef MODO_ICSCERT
      Serial.println("[WiFi] MODO_ICSCERT — red RedLab_Segura (SSID oculto).");
    #endif
  } else {
    Serial.println();
    Serial.println("[WiFi] FALLO — no se pudo conectar.");
  }
}

// ════════════════════════════════════════════════════════════════════
// iniciarMQTT()
// Configurar parámetros de latencia ANTES de la primera conexión.
// ════════════════════════════════════════════════════════════════════
bool iniciarMQTT() {
  // ── Parámetros de latencia ──────────────────────────────────────
  // NOTA: estos valores (30 / 2) no coinciden con el comentario de
  // "OPTIMIZACIONES DE LATENCIA" al inicio del archivo (3 / 1).
  // Se dejaron sin tocar en esta pasada de compactación — es un
  // ajuste de comportamiento, no de estructura del código.
  mqttClient.setKeepAlive(30);
  mqttClient.setSocketTimeout(2);

  // Buffer de 512 bytes — suficiente para el JSON más largo del kit
  // {"accion":"retroceder","velocidad":255} = ~40 bytes
  mqttClient.setBufferSize(512);

  return _conectarMQTT();
}

// ════════════════════════════════════════════════════════════════════
// mantenerConexion()
// Llamada en CADA iteración del loop() — debe ser rápida.
//
// CAMBIO CLAVE de latencia:
//   mqttClient.loop() se llama SIEMPRE, incluso si el cliente
//   no está conectado en este momento. Esto procesa cualquier
//   mensaje que haya llegado en el buffer TCP antes de que se
//   detectara la desconexión, evitando perder comandos.
// ════════════════════════════════════════════════════════════════════
bool mantenerConexion() {

  // ── 1. Verificar WiFi ─────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Conexion perdida — reconectando...");
    setLEDs(LED_AZUL_PARPADEANTE);
    WiFi.disconnect();
    delay(100);
    WiFi.begin(WIFI_SSID, WIFI_PASS);  // ← usar begin en vez de reconnect



    int intentos = 0;
    while (WiFi.status() != WL_CONNECTED && intentos < 6) {
      delay(500);
      intentos++;
    }

    if (WiFi.status() != WL_CONNECTED) {
      return false;
    }
    Serial.println("[WiFi] Reconectado. IP: " +
                   WiFi.localIP().toString());
  }

  // ── 2. Verificar MQTT y reconectar si es necesario ───────────
  if (!mqttClient.connected()) {
    unsigned long ahora = millis();

    // Intervalo reducido a 1000ms (antes 5000ms)
    // El robot vuelve a estar disponible en 1s en vez de 5s
    if (ahora - _ultimaConexionMQTT >= 1000UL) {
      _ultimaConexionMQTT = ahora;
      Serial.println("[MQTT] Conexion perdida — reconectando...");
      setLEDs(LED_AZUL_PARPADEANTE);

      if (_conectarMQTT()) {
        Serial.println("[MQTT] Reconexion exitosa.");
      } else {
        Serial.println("[MQTT] Reconexion fallida — reintentando en 1s.");
        // Procesar el loop aunque no esté conectado
        // para no perder mensajes en el buffer TCP
        mqttClient.loop();
        return false;
      }
    } else {
      // Dentro del intervalo de espera — procesar buffer igualmente
      mqttClient.loop();
      return false;
    }
  }

  // ── 3. Procesar mensajes MQTT entrantes ──────────────────────
  // CRÍTICO: llamar en cada iteración del loop().
  // Sin esta llamada el robot no recibe ningún comando.
  mqttClient.loop();

  return true;
}
