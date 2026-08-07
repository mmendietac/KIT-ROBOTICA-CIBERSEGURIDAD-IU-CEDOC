// ════════════════════════════════════════════════════════════════════
// camara.h — Declaraciones del módulo de cámara
// Kit de Robótica Educativa en Ciberseguridad v2.0
// Especialización en Ciberseguridad — ESCOM / CEDOC — 2025
// ════════════════════════════════════════════════════════════════════
//
// RESPONSABILIDAD DE ESTE ARCHIVO:
//   Declarar las funciones que camara.cpp implementa.
//   Este módulo gestiona el servidor HTTP MJPEG que transmite
//   el video de la cámara OV2640 en tiempo real.
//
// QUIÉN INCLUYE ESTE ARCHIVO:
//   - Kit_Robotica_Segura.ino → iniciarCamara() en setup()
//   - Nadie más lo necesita
//
// HARDWARE QUE GESTIONA:
//   Módulo OV2640 integrado en la placa FNK0084 (Freenove con cámara).
//   Resolución mínima QVGA (320×240) a ~15 fps con WiFi activo.
//
//   IMPORTANTE — COMPATIBILIDAD DE HARDWARE:
//   El FNK0091 (Freenove breakout board SIN cámara) NO tiene OV2640.
//   Si el kit usa FNK0091, iniciarCamara() retorna inmediatamente
//   sin intentar inicializar el sensor — evita el crash
//   ESP_ERR_NOT_SUPPORTED (0x106) que se produce al llamar
//   esp_camera_init() sin módulo físico conectado.
//
// COMPORTAMIENTO SEGÚN EL MODO COMPILADO:
//
//   MODO_STUXNET:
//     La cámara se inicializa Y se levanta un servidor HTTP en
//     el puerto 80 con el endpoint /stream accesible sin
//     ningún tipo de autenticación.
//     Esto activa la VULNERABILIDAD INTENCIONAL [VUL-OI02]:
//     cualquier dispositivo en la red puede ver el video en vivo
//     navegando a http://[IP_ROBOT]/stream sin usuario ni contraseña.
//     Viola el control OWASP OI-02 (interfaces sin acceso innecesario).
//     Los estudiantes lo verifican en el Reto 5 (Auditoría OWASP).
//
//   MODO_ICSCERT:
//     La cámara se inicializa pero el servidor HTTP NO se levanta.
//     El endpoint /stream no existe en este modo.
//     Un navegador que intente acceder a http://[IP_ROBOT]/stream
//     no recibirá respuesta — conexión rechazada.
//     Cumple el control OWASP OI-02.
//
// FUNCIÓN PEDAGÓGICA:
//   El stream sin autenticación en MODO_STUXNET hace tangible
//   el concepto de "interfaz expuesta innecesariamente".
//   El estudiante puede ver en su navegador el video del robot
//   sin ninguna credencial — exactamente el tipo de vulnerabilidad
//   que afectó a cámaras IP industriales en incidentes reales
//   como el ataque a la red eléctrica de Ucrania en 2015.
//
// PRECONDICIÓN PARA iniciarCamara():
//   Llamar DESPUÉS de iniciarWiFi() en setup().
//   El servidor HTTP de la cámara requiere el stack de red activo.
//   Si se llama antes de iniciarWiFi() puede producir el crash
//   Guru Meditation: LoadProhibited que se observó en los logs.
// ════════════════════════════════════════════════════════════════════

#pragma once

#include <Arduino.h>   // Serial, String, etc.

// ════════════════════════════════════════════════════════════════════
// FUNCIONES PÚBLICAS DEL MÓDULO
// ════════════════════════════════════════════════════════════════════

// ── iniciarCamara() ─────────────────────────────────────────────────
// Intenta inicializar el módulo OV2640 y, según el modo compilado,
// levanta o no el servidor HTTP MJPEG.
//
// Comportamiento si NO hay módulo físico (FNK0091 sin cámara):
//   Retorna inmediatamente con un mensaje en Serial.
//   NO llama esp_camera_init() — evita el crash 0x106.
//   El sistema continúa normalmente sin funcionalidad de cámara.
//   Esta es la situación actual del kit en desarrollo.
//
// Comportamiento en MODO_STUXNET con módulo físico:
//   1. Llama esp_camera_init() con la configuración de pines OV2640
//   2. Levanta WebServer en puerto 80
//   3. Registra el handler /stream SIN autenticación — VUL-OI02
//   4. Llama _servidor.begin()
//   5. Imprime en Serial la URL del stream y activa VUL-OI02
//
// Comportamiento en MODO_ICSCERT con módulo físico:
//   1. Llama esp_camera_init() (inicializa el hardware)
//   2. NO levanta el WebServer
//   3. NO registra ningún endpoint HTTP
//   4. Imprime en Serial que el servidor está desactivado
//
// LLAMAR EN: setup(), como quinto paso, después de iniciarWiFi()
//            y antes de iniciarMQTT().
//
// VERIFICACIÓN PTI-10 (solo con módulo físico en MODO_STUXNET):
//   Abrir navegador → http://[IP_ROBOT]/stream
//   PASS: video MJPEG carga sin pedir usuario ni contraseña.
//   FAIL: pide autenticación o no carga → servidor HTTP fuera
//         del bloque #ifdef MODO_STUXNET en camara.cpp.
void iniciarCamara();

// ── detenerCamara() ─────────────────────────────────────────────────
// Detiene el servidor HTTP si está activo.
// Libera el puerto 80 para que pueda ser usado por otro servicio.
//
// En la versión actual no se llama desde ningún lugar del proyecto
// porque el servidor corre durante toda la sesión del robot.
// Se declara aquí para facilitar futuras extensiones donde
// se necesite apagar la cámara en tiempo de ejecución
// (por ejemplo, al cambiar de MODO_STUXNET a MODO_ICSCERT
// sin reiniciar el robot — característica planeada para v2.0).
//
// LLAMAR: solo si se necesita liberar el puerto 80 explícitamente.
// En el uso normal del kit NO es necesario llamar esta función.
void detenerCamara();
bool camaraActiva();
