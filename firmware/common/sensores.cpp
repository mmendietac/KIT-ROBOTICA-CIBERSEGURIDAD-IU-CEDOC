// ════════════════════════════════════════════════════════════════════
// sensores.cpp — Módulo de sensores
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// CORRECCIONES — 08/07/2026:
//   [NUEVO] TRIG=GPIO3  (GPIO23 no expuesto en FNK0084)
//   [NUEVO] ECHO=GPIO14 (GPIO24 no expuesto en FNK0084)
//   [NUEVO] Sensor desactivado cuando CAMARA_ACTIVA=1 (Reto 5)
//           GPIO23/24 son libres en ambos modos, pero pedagógicamente
//           el esquive no aplica al Reto 5 (robot auditado, no móvil)
//
// ADVERTENCIA DE HARDWARE (sin cambio):
//   El HC-SR04 emite 5V en ECHO.
//   El ESP32-S3 tolera MÁXIMO 3.6V en sus entradas GPIO.
//   Divisor resistivo OBLIGATORIO: R1=10kΩ entre ECHO y GPIO24,
//   R2=20kΩ entre GPIO24 y GND. Resultado: 3.33V en GPIO24.
//   SIN el divisor el GPIO24 se destruye en segundos.
//
// FUNCIÓN PEDAGÓGICA EN EL RETO 4:
//   La lógica de esquive está en Kit_Robotica_Segura_V2.ino (loop()).
//   Este módulo solo provee medirDistanciaCm().
//   Bajo flood STUXNET: loop() saturado → medirDistanciaCm() no se
//   llama con la frecuencia necesaria → robot no esquiva → CHOCA.
//   Bajo flood ICSCERT: rate limiting → loop() libre →
//   medirDistanciaCm() se llama cada 150ms → robot esquiva.
// ════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "sensores.h"
#include "config.h"

// ════════════════════════════════════════════════════════════════════
// iniciarSensores()
// ════════════════════════════════════════════════════════════════════
void iniciarSensores() {

#if CAMARA_ACTIVA == 0
  // TRIG=GPIO23 y ECHO=GPIO24 — libres de la cámara OV2640
  pinMode(PIN_HC_TRIG, OUTPUT);
  digitalWrite(PIN_HC_TRIG, LOW);   // LOW inicial — evitar disparo espurio
  pinMode(PIN_HC_ECHO, INPUT);

  Serial.println("[SENSORES] HC-SR04 activo. TRIG=GPIO3 ECHO=GPIO14.");
  Serial.println("[SENSORES] ECHO via divisor 10k/20k (5V->3.33V) obligatorio.");
  Serial.println("[SENSORES] TCRT5000 declarado — sin modulo fisico en v1.0.");

#else
  // CAMARA_ACTIVA=1 (Reto 5) — sensor desactivado
  // GPIO23/24 son libres (no usados por la cámara) pero el
  // HC-SR04 no tiene función pedagógica en el Reto 5.
  Serial.println("[SENSORES] HC-SR04 desactivado (CAMARA_ACTIVA=1).");
  Serial.println("[SENSORES] Reto 5: robot auditado, no movil.");
#endif
}

// ════════════════════════════════════════════════════════════════════
// medirDistanciaCm()
//
// Protocolo HC-SR04:
//   1. LOW 2µs  en TRIG → limpiar señal residual
//   2. HIGH 10µs en TRIG → pulso ultrasónico
//   3. pulseIn() mide µs en que ECHO permanece HIGH
//   4. Conversión: cm = (duracion × 0.0343) / 2
//
// Retorna 400.0 si no hay respuesta (sin obstáculo o fuera de rango).
// Tiempo máximo de ejecución: 30ms (timeout de pulseIn).
//
// VERIFICACIÓN PTI-05:
//   Medir a 10, 30, 50 y 80 cm. Error máximo aceptable: ±3 cm.
// ════════════════════════════════════════════════════════════════════
float medirDistanciaCm() {

#if CAMARA_ACTIVA == 0 && SENSOR_ACTIVO == 1
  digitalWrite(PIN_HC_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_HC_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_HC_TRIG, LOW);

  long duracion = pulseIn(PIN_HC_ECHO, HIGH, 30000);

  if (duracion == 0) return 400.0f;

  float distancia = (duracion * 0.0343f) / 2.0f;

  if (distancia < 2.0f)   distancia = 2.0f;
  if (distancia > 400.0f) distancia = 400.0f;

  return distancia;

#else
  return 400.0f;   // sensor desactivado (SENSOR_ACTIVO=0 o CAMARA_ACTIVA=1)
#endif
}
