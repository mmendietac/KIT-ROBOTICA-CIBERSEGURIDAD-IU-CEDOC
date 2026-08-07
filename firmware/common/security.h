// ════════════════════════════════════════════════════════════════════
// security.h — Declaraciones del módulo de seguridad y conectividad
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// RESPONSABILIDAD DE ESTE ARCHIVO:
//   Declarar (NO implementar) todo lo que security.cpp exporta
//   hacia el resto del proyecto.
//
//   Regla de los archivos .h:
//   - Declaraciones  → van aquí   (le dicen al compilador QUÉ existe)
//   - Implementación → va en .cpp (le dice al compilador CÓMO funciona)
//
// QUIÉN INCLUYE ESTE ARCHIVO:
//   - Kit_Robotica_Segura.ino  → para llamar iniciarWiFi(), iniciarMQTT()
//                                 mantenerConexion() y setLEDs()
//   - display.cpp              → para leer mensajesRechazados (via extern)
//   - camara.cpp               → no lo necesita directamente
//
// DEPENDENCIAS DE ESTE MÓDULO (librerías a instalar):
//   - PubSubClient  (Nick O'Leary) — cliente MQTT
//   - WiFi          (incluida en el paquete ESP32 — no instalar aparte)
// ════════════════════════════════════════════════════════════════════

#pragma once
// #pragma once reemplaza al guardián tradicional #ifndef / #define / #endif
// Garantiza que este archivo se incluya una sola vez por unidad de compilación
// aunque varios .cpp lo incluyan. Soportado por todos los compiladores modernos.

#include <Arduino.h>      // String, Serial, millis(), etc.
#include <WiFi.h>         // WiFiClient, WiFiClientSecure, WiFi.begin()
#include <PubSubClient.h> // PubSubClient — cliente MQTT
#include "config.h"       // enum EstadoLed — usado en setLEDs()

// ════════════════════════════════════════════════════════════════════
// VARIABLE EXPORTADA — definida en security.cpp, visible en otros módulos
// ════════════════════════════════════════════════════════════════════

// Contador de mensajes MQTT descartados por rate limiting.
// Sube durante el flood del Reto 4 cuando MODO_ICSCERT está activo
// (RATE_LIMIT = 20). En MODO_STUXNET siempre permanece en 0
// porque RATE_LIMIT = 9999 nunca se supera en la práctica.
//
// 'extern' significa: "esta variable EXISTE en security.cpp,
// búscala ahí al enlazar". Sin 'extern' cada archivo que incluya
// este .h crearía su propia copia desconectada de la variable.
extern int mensajesRechazados;

// ════════════════════════════════════════════════════════════════════
// CLIENTE MQTT EXPORTADO — instancia definida en security.cpp
// ════════════════════════════════════════════════════════════════════

// El objeto mqttClient es instanciado en security.cpp con el tipo
// correcto según el modo compilado:
//   MODO_ICSCERT → PubSubClient sobre WiFiClientSecure (con TLS)
//   MODO_STUXNET → PubSubClient sobre WiFiClient       (sin TLS)
//
// Se exporta como extern para que el .ino pueda verificar
// mqttClient.connected() si fuera necesario en el futuro.
// En el diseño actual mantenerConexion() encapsula esa lógica
// y el .ino no necesita acceder al cliente directamente.
extern PubSubClient mqttClient;

// Variable de rate limiting — accesible desde el .ino via extern
extern int mensajesRechazados;

// ════════════════════════════════════════════════════════════════════
// FUNCIONES PÚBLICAS DEL MÓDULO
// Implementadas en security.cpp — declaradas aquí para que
// el compilador las conozca antes de ver su implementación.
// ════════════════════════════════════════════════════════════════════

// ── iniciarWiFi() ───────────────────────────────────────────────────
// Configura el modo WiFi como estación (WIFI_STA) y se conecta
// a la red definida en config.h (WIFI_SSID / WIFI_PASS).
// Espera hasta 10 segundos. Si no conecta, continúa de todas formas
// — mantenerConexion() reintentará en cada iteración del loop().
// Llamar en setup() ANTES de iniciarCamara() e iniciarMQTT().
void iniciarWiFi();
bool iniciarMQTT();
bool mantenerConexion();


// ── iniciarMQTT() ───────────────────────────────────────────────────
// Conecta al broker Mosquitto usando los parámetros de config.h.
//
// En MODO_ICSCERT:
//   - Usa WiFiClientSecure con setCACert(CA_CERT) de certs.h
//   - Puerto 8883, usuario robot01, contraseña T0k3n_Segur0!
//   - Registra un Last Will Testament (LWT) en TOPIC_ESTADO
//
// En MODO_STUXNET:
//   - Usa WiFiClient plano (sin TLS) — VUL-1 activa
//   - Puerto 1883, sin usuario ni contraseña — VUL-2 activa
//
// Retorna true si la conexión fue exitosa, false si falló.
// Si falla, mantenerConexion() reintentará automáticamente.


// ── mantenerConexion() ──────────────────────────────────────────────
// Función de mantenimiento — debe llamarse en CADA iteración de loop().
// Hace tres cosas en orden:
//   1. Verifica que el WiFi sigue conectado. Si no, llama WiFi.reconnect()
//   2. Verifica que el cliente MQTT sigue conectado. Si no, llama iniciarMQTT()
//   3. Llama mqttClient.loop() para procesar mensajes entrantes
//      CRÍTICO: sin esta llamada el robot no recibe ningún comando MQTT.
//
// Retorna:
//   true  → WiFi y MQTT conectados, mensajes siendo procesados
//   false → alguna conexión perdida, intentando reconectar
//
// El valor de retorno lo usa loop() para mostrar "OK" o "RECON"
// en el OLED y para saber si activar el Fail-Safe.
bool mantenerConexion();

// ── setLEDs() ───────────────────────────────────────────────────────
// Controla el color y patrón de parpadeo de los 3 LEDs WS2812B.
// Aunque la implementación física está en display.cpp, se declara
// aquí porque security.cpp la llama frecuentemente (desde el callback
// MQTT y desde iniciarMQTT()) y necesita conocer su firma.
//
// Estados válidos (enum EstadoLed, definido en config.h) y su
// significado:
//   LED_VERDE_FIJO           → ICSCERT conectado y operativo
//   LED_AZUL_PARPADEANTE     → conectando al WiFi
//   LED_ROJO_PARPADEANTE     → STUXNET activo — robot vulnerable
//   LED_AMARILLO_PARPADEANTE → rate limiting activo — flood detectado
//   LED_ROJO_RAPIDO          → Fail-Safe — sin conexión MQTT
//
// Nota: setLEDs() usa millis() internamente para el parpadeo.
// No bloquea el loop(). Llamarla frecuentemente es seguro.
void setLEDs(EstadoLed estado);

// ── drenarMQTT() ──────────────────────────────────────────────────
// NUEVO — [fecha de esta sesión] — diagnóstico de flood sostenido.
// Bajo flood, mensajesRecibidos/seg cae a 0 después de ~10 mensajes
// y se queda ahí indefinidamente, sin importar la velocidad del
// flood. El log de Mosquitto confirma que el broker sigue enviando
// sin ningún error durante ese tiempo — el robot deja de leer el
// socket. Firma de una ventana TCP de recepción cerrada: el buffer
// se llena porque WiFiClientSecure + PubSubClient dejan datos ya
// descifrados sin drenar si solo se llama loop() una vez por
// iteración de loop() Arduino.
//
// Llamar en cada iteración de loop() EN VEZ DE la llamada suelta a
// mqttClient.loop() — ver Kit_Robotica_Segura_V2.ino, paso 2.
void drenarMQTT();
