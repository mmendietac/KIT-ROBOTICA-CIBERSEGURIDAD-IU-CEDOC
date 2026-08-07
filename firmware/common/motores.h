// motores.h — Control de motores TB6612FNG
// Ver motores.cpp para documentación completa.
#pragma once
#include <Arduino.h>
void iniciarMotores();
void avanzar(int vel);
void retroceder(int vel);
void girarIzquierda(int vel);
void girarDerecha(int vel);
void detener();
void modoEmergencia();
