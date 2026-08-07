// ════════════════════════════════════════════════════════════════════
// display.h — Declaraciones del módulo de display y LEDs
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// RESPONSABILIDAD DE ESTE ARCHIVO:
//   Declarar las funciones que display.cpp implementa.
//   Este módulo gestiona DOS periféricos de salida visual:
//     1. OLED SSD1306  — pantalla de 128×64 px por I2C
//     2. WS2812B       — 3 LEDs NeoPixel por protocolo de 1 hilo
//
// QUIÉN INCLUYE ESTE ARCHIVO:
//   - Kit_Robotica_Segura.ino → iniciarDisplay(), actualizarOLED()
//   - security.cpp            → setLEDs() (señal visual de red)
//   - motores.cpp             → NO lo necesita
//   - sensores.cpp            → NO lo necesita
//   - camara.cpp              → NO lo necesita
//
// HARDWARE QUE GESTIONA:
//
//   OLED SSD1306 — 128×64 px, I2C, 3.3V
//     SDA → GPIO8  (pull-up 4.7kΩ a 3.3V OBLIGATORIO en hardware)
//     SCL → GPIO9  (pull-up 4.7kΩ a 3.3V OBLIGATORIO en hardware)
//     VCC → 3.3V
//     GND → GND
//     Dirección I2C: 0x3C (definida como OLED_ADDR en config.h)
//     Si no detecta en 0x3C probar 0x3D — depende del módulo físico.
//     Sin pull-ups: bus I2C flotante → OLED no responde jamás.
//
//   WS2812B — 3 LEDs NeoPixel
//     DIN → GPIO48 con resistencia 330Ω en el cable de datos
//     VCC → 3.3V  (brillo limitado a 31% para no exceder corriente)
//     GND → GND
//     Brillo configurado en 80/255 ≈ 31% → ~60mA total (3 LEDs)
//     A brillo 100% consumiría ~180mA — riesgo para el regulador 3.3V
//
// LIBRERÍAS REQUERIDAS:
//   Instalar desde Herramientas → Gestor de librerías en Arduino IDE:
//   - "Adafruit SSD1306"     (autor: Adafruit)
//   - "Adafruit GFX Library" (autor: Adafruit) — dependencia del SSD1306
//   - "Adafruit NeoPixel"    (autor: Adafruit)
//
// LOS 5 ESTADOS VISUALES DEL SISTEMA (enum EstadoLed, definido en config.h):
//
//   Estado                    Color         Cuándo ocurre
//   ───────────────────────── ───────────── ──────────────────────────
//   LED_VERDE_FIJO             Verde fijo    ICSCERT conectado y seguro
//   LED_AZUL_PARPADEANTE       Azul lento    Conectando al WiFi
//   LED_ROJO_PARPADEANTE       Rojo lento    STUXNET activo/vulnerable
//   LED_AMARILLO_PARPADEANTE   Amarillo rápido  Flood detectado (Reto 4)
//   LED_ROJO_RAPIDO            Rojo intenso  Fail-Safe sin MQTT
//   LED_OFF                    Apagado       Estado desconocido / por defecto
//
// NOTA SOBRE setLEDs() Y EL PARPADEO:
//   setLEDs() usa millis() internamente — NO usa delay().
//   Es seguro llamarla en cada iteración del loop() sin bloquear
//   el procesamiento de mensajes MQTT.
//   El parpadeo ocurre naturalmente porque loop() la llama
//   repetidamente y la función alterna el estado del LED
//   cada cierto intervalo de tiempo usando millis().
// ════════════════════════════════════════════════════════════════════

#pragma once

#include <Arduino.h>   // String, Serial, millis(), etc.
#include "config.h"    // enum EstadoLed — usado en setLEDs()

// ════════════════════════════════════════════════════════════════════
// FUNCIONES PÚBLICAS DEL MÓDULO
// ════════════════════════════════════════════════════════════════════

// ── iniciarDisplay() ────────────────────────────────────────────────
// Inicializa el bus I2C, el OLED SSD1306 y los LEDs WS2812B.
//
// Secuencia interna:
//   1. Wire.begin(I2C_SDA, I2C_SCL) — inicia I2C en GPIO8 y GPIO9
//   2. Wire.setClock(400000)         — Fast Mode 400kHz
//   3. oled.begin()                  — detecta el SSD1306 en OLED_ADDR
//   4. Muestra pantalla de inicio con OLED_MODO ("STUXNET"/"ICSCERT")
//   5. leds.begin() + setBrightness(80) — inicia NeoPixel al 31%
//   6. Parpadeo azul 3 veces como confirmación visual de arranque
//
// Si el OLED no responde (oled.begin() devuelve false):
//   Reporta el error por Serial y continúa sin OLED.
//   El sistema sigue funcionando — el OLED no es crítico para los retos.
//   Causa más frecuente: pull-ups 4.7kΩ ausentes en GPIO8/GPIO9.
//
// LLAMAR EN: setup(), como PRIMER paso antes de todos los demás.
// Razón: si otro módulo falla durante setup(), el OLED puede
// mostrar el error. Si display no está listo, esa información se pierde.
void iniciarDisplay();

// ── actualizarOLED(ip, mqttEstado, rechazados) ──────────────────────
// Refresca las 4 líneas del OLED con el estado actual del sistema.
// Llamar desde loop() cada INTERVALO_OLED_MS (2000ms) usando millis().
// NO llamar en cada iteración — cada llamada envía ~1KB por I2C.
//
// Contenido de las 4 líneas:
//
//   Línea 1 (texto 2x, grande):
//     "STUXNET" o "ICSCERT" — identifica el modo compilado.
//     Fija en tiempo de compilación — no cambia en ejecución.
//     Es la información más crítica: permite saber de un vistazo
//     qué binario está corriendo sin abrir el Monitor Serial.
//
//   Línea 2 (texto 1x):
//     "IP: 192.168.10.42" — dirección asignada por el router.
//     Si muestra "IP: 0.0.0.0" el WiFi no está conectado todavía.
//     Los estudiantes la usan para configurar MQTT Explorer.
//
//   Línea 3 (texto 1x):
//     "MQTT:OK P:1883"    — conectado al broker, listo para comandos,
//                            más el puerto MQTT en uso (MQTT_PUERTO).
//     "MQTT:RECON P:1883" — intentando reconectar, Fail-Safe activo.
//     El puerto es una constante de config.h (1883 en MODO_STUXNET,
//     8883 en MODO_ICSCERT) — confirma de un vistazo si TLS está en
//     juego, sin tener que abrir el Monitor Serial.
//
//   Línea 4 (texto 1x):
//     "Rech:N RL:OFF" o "Rech:N RL:20" — contador de mensajes
//     rechazados por rate limiting, más el límite configurado
//     (RATE_LIMIT de config.h).
//     RL:OFF cuando RATE_LIMIT=9999 (MODO_STUXNET — sin límite real).
//     RL:<número> cuando hay un límite activo (MODO_ICSCERT — ej. 20).
//     En MODO_ICSCERT el contador N sube durante el flood del Reto 4.
//     En MODO_STUXNET el contador N siempre permanece en 0.
//     Cuando N sube es evidencia visual de que el anti-DoS funciona.
//
// Parámetros:
//   ip         — String con la IP actual (ej: "192.168.10.42")
//   mqttEstado — String "OK" o "RECON"
//   rechazados — int con el valor actual de mensajesRechazados
//
// VERIFICACIÓN PTI-03:
//   Contar 5 ciclos de actualización con cronómetro.
//   Cada ciclo debe durar 2.0 ± 0.2 segundos.

void reiniciarI2C();


void actualizarOLED(String ip, String mqttEstado, int rechazados);

// ── setLEDs(EstadoLed estado) ───────────────────────────────────────
// Cambia el color y patrón de parpadeo de los 3 LEDs WS2812B.
//
// CORREGIDO — [fecha de esta sesión]: la firma pasó de
// setLEDs(String estado) a setLEDs(EstadoLed estado) para coincidir
// con la declaración de security.h y las llamadas ya migradas en
// security.cpp y en el .ino (setLEDs(LED_AMARILLO_PARPADEANTE),
// setLEDs(COLOR_NORMAL), etc.). El enum EstadoLed se define en
// config.h — ver también el comentario de compactación ahí.
//
// Implementa los 5 estados visuales del sistema usando millis()
// para el parpadeo — nunca delay(). Es seguro llamarla en cada
// iteración del loop() sin afectar el procesamiento MQTT.
//
// Intervalos de parpadeo por estado:
//   LED_VERDE_FIJO            → sin parpadeo   (estable = todo OK)
//   LED_AZUL_PARPADEANTE      → cada 500ms     (conectando — lento)
//   LED_ROJO_PARPADEANTE      → cada 800ms     (STUXNET — muy lento)
//   LED_AMARILLO_PARPADEANTE  → cada 150ms     (flood — rápido)
//   LED_ROJO_RAPIDO           → cada 100ms     (Fail-Safe — urgente)
//
// LED_OFF o cualquier valor no manejado → LEDs apagados (comportamiento
// seguro por defecto).
//
// LLAMADA DESDE:
//   - security.cpp  → al cambiar estado de WiFi/MQTT/flood
//   - motores.cpp   → NO (los motores no cambian el estado visual)
//   - .ino loop()   → al activar el Fail-Safe (LED_ROJO_RAPIDO)
//   - .ino procesarComandoJSON() → COLOR_NORMAL al procesar comando
//
// VERIFICACIÓN PTI-04:
//   Verificar los 5 estados visualmente a 3 metros de distancia.
//   Cada estado debe ser distinguible del anterior sin ambigüedad.
//   El cambio entre estados debe ocurrir en menos de 2 segundos.
void setLEDs(EstadoLed estado);