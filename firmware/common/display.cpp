// ════════════════════════════════════════════════════════════════════
// display.cpp — OLED SSD1306 + LEDs NeoPixel WS2812B
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// PINES GPIO (definidos en config.h — NO hardcodeados aquí):
//   I2C_SDA = GPIO38  (ACTUALIZADO — antes GPIO8 = CAM_Y4 cámara)
//   I2C_SCL = GPIO25  (ACTUALIZADO — antes GPIO9 = CAM_Y3 cámara)
//   OLED_ADDR = 0x3C
//   PIN_NEOPIXEL = GPIO48 (sin cambio)
//
// CORRECCIÓN — 08/07/2026:
//   GPIO8 y GPIO9 son CAM_Y4 y CAM_Y3 de la cámara OV2640 FNK0084.
//   Moverlos a GPIO38 (SDA) y GPIO25 (SCL) elimina el conflicto.
//   reiniciarI2C() se mantiene como salvaguarda pero ya no es
//   necesario porque GPIO38/25 no los usa la cámara.
//
// API (compatibilidad con Kit_Robotica_Segura_V2.ino):
//   iniciarDisplay()                         → setup()
//   reiniciarI2C()                            → setup() tras iniciarCamara()
//   actualizarOLED(ip, mqttEstado, rechazados)→ loop() cada 2000ms
//   setLEDs(EstadoLed estado)                 → security.cpp + loop()
// ════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "display.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>

// ── Objetos estáticos ────────────────────────────────────────────────
static Adafruit_SSD1306  oled(128, 64, &Wire, -1);
static Adafruit_NeoPixel leds(NUM_LEDS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

// ── Variable interna para el parpadeo de LEDs ────────────────────────
static unsigned long _ultimoParpadeoMs = 0;
static bool          _estadoParpadeo   = false;

// ════════════════════════════════════════════════════════════════════
// iniciarDisplay()
// Inicializa el bus I2C, el OLED y los LEDs NeoPixel.
// Llamar en setup() como PRIMERA función.
// ════════════════════════════════════════════════════════════════════
void iniciarDisplay() {

  // Iniciar bus I2C
  Wire.begin(I2C_SDA, I2C_SCL);  // SDA=GPIO38 SCL=GPIO1
  Wire.setClock(400000);  // Fast Mode: 400 kHz

  // Inicializar OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[DISPLAY] ERROR: OLED no detectado.");
    Serial.println("[DISPLAY] Verificar pull-ups 4.7k en SDA(GPIO38) y SCL(GPIO1).");
    Serial.print  ("[DISPLAY] Direccion buscada: 0x");
    Serial.println(OLED_ADDR, HEX);
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);

    // Pantalla de inicio durante el arranque
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("Kit Robotica v2.0");
    oled.setCursor(0, 12); oled.print("ESCOM / CEDOC 2025");
    oled.setCursor(0, 28); oled.print("Modo:");
    oled.setCursor(0, 40);
    oled.setTextSize(2);
    oled.print(OLED_MODO);
    oled.display();

    Serial.print("[DISPLAY] OLED iniciado. Modo: ");
    Serial.println(OLED_MODO);
  }

  // Iniciar LEDs NeoPixel
  leds.begin();
  leds.setBrightness(80);
  leds.clear();
  leds.show();

  // Parpadeo azul de confirmación al arrancar
  for (int i = 0; i < 3; i++) {
    leds.fill(leds.Color(0, 0, 60));
    leds.show();
    delay(200);
    leds.clear();
    leds.show();
    delay(200);
  }

  Serial.println("[DISPLAY] LEDs NeoPixel iniciados.");
}

// ════════════════════════════════════════════════════════════════════
// reiniciarI2C()
// NUEVA FUNCIÓN — llamar desde setup() después de iniciarCamara().
//
// La librería esp32-camera reconfigura GPIO8 (I2C_SDA) durante
// esp_camera_init() porque ese pin también es CAM_Y4 en el FNK0084.
// Esto rompe el bus Wire silenciosamente: oled.display() ejecuta
// sin error pero los datos nunca llegan al SSD1306.
//
// Esta función reinicia completamente el bus y reconfigura el OLED,
// restaurando la comunicación I2C después de la inicialización
// de la cámara.
// ════════════════════════════════════════════════════════════════════
void reiniciarI2C() {
  Serial.println("[I2C] Reiniciando bus Wire tras inicializar camara...");

  // Detener el bus Wire completamente
  Wire.end();
  delay(50);

  // Reiniciar el bus con los pines correctos
  Wire.begin(I2C_SDA, I2C_SCL);  // SDA=GPIO38 SCL=GPIO1
  Wire.setClock(400000);
  delay(50);

  // Reinicializar el OLED desde cero
  // Es necesario porque Wire.end() libera los recursos del SSD1306
  if (!oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("[I2C] ERROR: OLED no responde tras reinicio I2C.");
    Serial.println("[I2C] Verificar que GPIO38 no quedó en modo CAM.");
  } else {
    oled.clearDisplay();
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);
    Serial.println("[I2C] Bus Wire y OLED reiniciados correctamente.");
  }
}

// ════════════════════════════════════════════════════════════════════
// actualizarOLED(ip, mqttEstado, rechazados)
// Refresca las 4 líneas del OLED con el estado actual del sistema.
// Se llama desde loop() cada INTERVALO_OLED_MS (2000ms).
//
// AMPLIADO — [fecha de esta sesión]: las líneas 3 y 4 ahora también
// muestran el puerto MQTT (MQTT_PUERTO) y el rate limit (RATE_LIMIT),
// ambos constantes de config.h, sin agregar una línea nueva a la
// pantalla (no hay espacio vertical libre en los 64px del SSD1306):
//   Línea 3: "MQTT:OK P:1883"     (antes solo "MQTT:OK")
//   Línea 4: "Rech:0 RL:OFF"      (antes solo "Rech: 0")
// RL muestra "OFF" cuando RATE_LIMIT=9999 (MODO_STUXNET, sin límite
// real) o el número exacto cuando hay un límite activo (MODO_ICSCERT,
// ej. "RL:20") — así el contraste STUXNET/ICSCERT del Reto 4 es
// visible directamente en la pantalla del robot.
// ════════════════════════════════════════════════════════════════════
void actualizarOLED(String ip, String mqttEstado, int rechazados) {

  oled.clearDisplay();

  // ── Línea 1 — Modo compilado (texto 2x) ─────────────────────────
  oled.setTextSize(2);
  oled.setCursor(0, 0);
  oled.print(OLED_MODO);   // "STUXNET" o "ICSCERT"

  // ── Línea 2 — IP del robot ───────────────────────────────────────
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.print("IP: ");
  oled.print(ip);

  // ── Línea 3 — Estado MQTT + Puerto ──────────────────────────────
  oled.setCursor(0, 36);
  oled.print("MQTT:");
  oled.print(mqttEstado);
  oled.print("   P:");
  oled.print(MQTT_PUERTO);

  // ── Línea 4 — Mensajes rechazados + Rate Limit ──────────────────
  oled.setCursor(0, 50);
  oled.print("Rech:");
  oled.print(rechazados);
  oled.print("  RL:");
#if RATE_LIMIT >= 9999
  oled.print("OFF");   // Sin límite real — MODO_STUXNET (RATE_LIMIT=9999)
#else
  oled.print(RATE_LIMIT);   // Límite activo — MODO_ICSCERT (ej. 20)
#endif

  // Enviar buffer al OLED por I2C
  oled.display();
}

// ════════════════════════════════════════════════════════════════════
// setLEDs(EstadoLed estado)
// Cambia el color y patrón de los LEDs WS2812B.
// Llamada desde security.cpp y desde el .ino principal.
//
// CORREGIDO — [fecha de esta sesión]: firma migrada de
// setLEDs(String estado) a setLEDs(EstadoLed estado) para coincidir
// con la declaración de security.h. La comparación de String contra
// literales de texto se reemplazó por un switch sobre el enum
// (definido en config.h) — más rápido y sin uso de heap, ya que se
// llama en cada iteración de loop().
// ════════════════════════════════════════════════════════════════════
void setLEDs(EstadoLed estado) {
  unsigned long ahora = millis();

  switch (estado) {

    case LED_VERDE_FIJO:
      leds.fill(leds.Color(0, 60, 0));
      leds.show();
      break;

    case LED_AZUL_PARPADEANTE:
      if (ahora - _ultimoParpadeoMs >= 500) {
        _ultimoParpadeoMs = ahora;
        _estadoParpadeo   = !_estadoParpadeo;
        if (_estadoParpadeo) leds.fill(leds.Color(0, 0, 60));
        else                 leds.clear();
        leds.show();
      }
      break;

    case LED_ROJO_PARPADEANTE:
      if (ahora - _ultimoParpadeoMs >= 800) {
        _ultimoParpadeoMs = ahora;
        _estadoParpadeo   = !_estadoParpadeo;
        if (_estadoParpadeo) leds.fill(leds.Color(60, 0, 0));
        else                 leds.clear();
        leds.show();
      }
      break;

    case LED_AMARILLO_PARPADEANTE:
      if (ahora - _ultimoParpadeoMs >= 150) {
        _ultimoParpadeoMs = ahora;
        _estadoParpadeo   = !_estadoParpadeo;
        if (_estadoParpadeo) leds.fill(leds.Color(60, 60, 0));
        else                 leds.clear();
        leds.show();
      }
      break;

    case LED_ROJO_RAPIDO:
      if (ahora - _ultimoParpadeoMs >= 100) {
        _ultimoParpadeoMs = ahora;
        _estadoParpadeo   = !_estadoParpadeo;
        if (_estadoParpadeo) leds.fill(leds.Color(80, 0, 0));
        else                 leds.clear();
        leds.show();
      }
      break;

    case LED_OFF:
    default:
      leds.clear();
      leds.show();
      break;
  }
}