// ════════════════════════════════════════════════════════════════════
// motores.cpp — Control de 2 motores DC via TB6612FNG
// ESP32-S3 — Arduino Core 3.x — API ledcAttach por pin
// ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// CORRECCIONES — 08/07/2026:
//   [NUEVO] AIN2=GPIO47 y BIN2=GPIO2 habilitados como GPIO de salida
//           Esto habilita el retroceso REAL del robot.
//           Antes: AIN2/BIN2 en GND fijo → AIN1=LOW producía coast
//           (rueda libre), no retroceso. Los motores no retrocedían.
//           Ahora: AIN1=L AIN2=H → motor izq atrás (real)
//                  BIN1=L BIN2=H → motor der atrás (real)
//   [NUEVO] PIN_STBY=GPIO21 (antes GPIO16=CAM_Y9 — conflicto cámara)
//   Conservado: solo PWMA (GPIO39) y PWMB (GPIO41) usan ledcAttach()
//   Conservado: AIN1/BIN1 usan digitalWrite() — corrección 30/06/2026
//
// COMPACTACIÓN — [fecha de esta sesión]:
//   avanzar/retroceder/girarIzquierda/girarDerecha repetían el mismo
//   par "digitalWrite dirección + ledcWrite velocidad" para cada
//   motor (~8 líneas cada una). Se extrajo _setMotor(), que aplica
//   la lógica TB6612FNG de un solo motor según el sentido pedido.
//   Comportamiento observable idéntico — mismos pines, misma lógica.
//
// LÓGICA TB6612FNG con 4 pines de dirección:
//   AIN1=H AIN2=L PWMA=vel → motor izq ADELANTE
//   AIN1=L AIN2=H PWMA=vel → motor izq ATRÁS
//   AIN1=L AIN2=L PWMA=x  → motor izq FRENO
//   BIN1=H BIN2=L PWMB=vel → motor der ADELANTE
//   BIN1=L BIN2=H PWMB=vel → motor der ATRÁS
//   BIN1=L BIN2=L PWMB=x  → motor der FRENO
// ════════════════════════════════════════════════════════════════════

#include <Arduino.h>
#include "motores.h"
#include "config.h"

void iniciarMotores() {

  // ── Pines de DIRECCIÓN — digitalWrite(), NO ledcAttach() ─────────
  // AIN1/AIN2 controlan dirección motor izquierdo
  // BIN1/BIN2 controlan dirección motor derecho
  pinMode(PIN_MOTOR_IZQ_BCK,  OUTPUT);  // AIN1
  pinMode(PIN_MOTOR_IZQ_BCK2, OUTPUT);  // AIN2 — retroceso
  pinMode(PIN_MOTOR_DER_BCK,  OUTPUT);  // BIN1
  pinMode(PIN_MOTOR_DER_BCK2, OUTPUT);  // BIN2 — retroceso

  digitalWrite(PIN_MOTOR_IZQ_BCK,  LOW);
  digitalWrite(PIN_MOTOR_IZQ_BCK2, LOW);
  digitalWrite(PIN_MOTOR_DER_BCK,  LOW);
  digitalWrite(PIN_MOTOR_DER_BCK2, LOW);

  // ── Pines PWM — SOLO PWMA y PWMB ────────────────────────────────
  if (!ledcAttach(PIN_MOTOR_IZQ_FWD, 1000, 8)) {
    Serial.println("[MOTORES] ERROR: LEDC no asignado a PWMA GPIO39");
  }
  if (!ledcAttach(PIN_MOTOR_DER_FWD, 1000, 8)) {
    Serial.println("[MOTORES] ERROR: LEDC no asignado a PWMB GPIO41");
  }

  // ── STBY GPIO21 — habilitar driver ───────────────────────────────
  pinMode(PIN_STBY, OUTPUT);
  digitalWrite(PIN_STBY, HIGH);

  detener();

  Serial.println("[MOTORES] TB6612FNG inicializado.");
  Serial.println("[MOTORES] PWMA=GPIO39 PWMB=GPIO41 STBY=GPIO21");
  Serial.println("[MOTORES] AIN1=GPIO40 AIN2=GPIO47 BIN1=GPIO42 BIN2=GPIO2");
  Serial.println("[MOTORES] Retroceso real habilitado via AIN2/BIN2.");
}

// ════════════════════════════════════════════════════════════════════
// _setMotor(pinFwd, pinBck1, pinBck2, vel, adelante)
// Aplica la lógica TB6612FNG a UN motor:
//   adelante=true  → BCK1=H BCK2=L PWM=vel  (ADELANTE)
//   adelante=false → BCK1=L BCK2=H PWM=vel  (ATRÁS)
// Usado por avanzar/retroceder/girarIzquierda/girarDerecha para
// evitar repetir el mismo par digitalWrite+ledcWrite por motor.
// No se usa en detener() ni modoEmergencia(): ahí ambos BCK van a
// LOW (freno), un caso que no encaja en "adelante/atrás".
// ════════════════════════════════════════════════════════════════════
static inline void _setMotor(int pinFwd, int pinBck1, int pinBck2,
                              int vel, bool adelante) {
  vel = constrain(vel, 0, 255);
  digitalWrite(pinBck1, adelante ? HIGH : LOW);
  digitalWrite(pinBck2, adelante ? LOW  : HIGH);
  ledcWrite(pinFwd, vel);
}

void avanzar(int vel) {
  _setMotor(PIN_MOTOR_IZQ_FWD, PIN_MOTOR_IZQ_BCK, PIN_MOTOR_IZQ_BCK2, vel, true);
  _setMotor(PIN_MOTOR_DER_FWD, PIN_MOTOR_DER_BCK, PIN_MOTOR_DER_BCK2, vel, true);
}

void retroceder(int vel) {
  _setMotor(PIN_MOTOR_IZQ_FWD, PIN_MOTOR_IZQ_BCK, PIN_MOTOR_IZQ_BCK2, vel, false);
  _setMotor(PIN_MOTOR_DER_FWD, PIN_MOTOR_DER_BCK, PIN_MOTOR_DER_BCK2, vel, false);
}

void girarIzquierda(int vel) {
  // Motor izquierdo atrás, motor derecho adelante
  _setMotor(PIN_MOTOR_IZQ_FWD, PIN_MOTOR_IZQ_BCK, PIN_MOTOR_IZQ_BCK2, vel, false);
  _setMotor(PIN_MOTOR_DER_FWD, PIN_MOTOR_DER_BCK, PIN_MOTOR_DER_BCK2, vel, true);
}

void girarDerecha(int vel) {
  // Motor izquierdo adelante, motor derecho atrás
  _setMotor(PIN_MOTOR_IZQ_FWD, PIN_MOTOR_IZQ_BCK, PIN_MOTOR_IZQ_BCK2, vel, true);
  _setMotor(PIN_MOTOR_DER_FWD, PIN_MOTOR_DER_BCK, PIN_MOTOR_DER_BCK2, vel, false);
}

void detener() {
  // AIN1=L AIN2=L → freno motor izquierdo
  digitalWrite(PIN_MOTOR_IZQ_BCK,  LOW);
  digitalWrite(PIN_MOTOR_IZQ_BCK2, LOW);
  ledcWrite(PIN_MOTOR_IZQ_FWD, 0);

  // BIN1=L BIN2=L → freno motor derecho
  digitalWrite(PIN_MOTOR_DER_BCK,  LOW);
  digitalWrite(PIN_MOTOR_DER_BCK2, LOW);
  ledcWrite(PIN_MOTOR_DER_FWD, 0);
}

void modoEmergencia() {
  detener();
  digitalWrite(PIN_STBY, LOW);   // corte físico de corriente al driver
  delay(100);
  digitalWrite(PIN_STBY, HIGH);  // re-habilitar para el próximo comando
}
