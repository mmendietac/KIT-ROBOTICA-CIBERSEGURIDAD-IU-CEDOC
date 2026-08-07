// sensores.h — Declaraciones del módulo de sensores
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
//
// HC-SR04:
//   TRIG → GPIO23 (antes GPIO12=CAM_Y6 — conflicto cámara)
//   ECHO → GPIO24 via divisor 10kΩ/20kΩ (antes GPIO13=CAM_PCLK)
//
// FUNCIÓN PEDAGÓGICA RETO 4:
//   STUXNET bajo flood: loop() saturado → robot no esquiva → CHOCA
//   ICSCERT bajo flood: rate limiting → loop() libre → robot esquiva
//   El contraste físico hace visible el ataque de Disponibilidad.

#pragma once
#include <Arduino.h>

void  iniciarSensores();
float medirDistanciaCm();
