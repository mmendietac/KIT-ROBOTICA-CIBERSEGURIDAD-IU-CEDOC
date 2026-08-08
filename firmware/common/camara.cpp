/* Kit Robotica Ciberseguridad ESCOM/CEDOC
* Autores: Milton Alvaro Mendieta Cifuentes, Juan Pablo Pedroche Quevedo 
* Director: Dr. Arles Prieto * Licencia MIT (c) 2026 
* Modulo: Control de motores 
*/
// COMPORTAMIENTO SEGÚN MODO Y CAMARA_ACTIVA:
//
//   MODO_STUXNET + CAMARA_ACTIVA=0 (Retos 1-4):
//     → Cámara NO inicializada — sin servidor HTTP
//     → Conexión MQTT completamente estable sin desconexiones
//
//   MODO_STUXNET + CAMARA_ACTIVA=1 (Reto 5):
//     → Stream MJPEG activo en /stream SIN autenticación (VUL-OI02)
//     → Foto estática en /foto SIN autenticación (VUL-OI02)
//     → Desconexiones MQTT intermitentes aceptables en este reto
//       porque el robot no necesita moverse durante la auditoría
//
//   MODO_ICSCERT (cualquier valor de CAMARA_ACTIVA):
//     → Cámara SIEMPRE desactivada
//     → Control OI-02 OWASP IoT implementado: sin interfaces expuestas
//
// CORRECCIONES APLICADAS — 01/07/2026:
//   - CAMARA_ACTIVA=0 desactiva la cámara en Retos 1-4
//   - Pines D0-D7 corregidos desde camera_pins.h oficial ESP32S3_EYE
//   - fb_count=1 + CAMERA_FB_IN_DRAM para evitar panic por PSRAM
//   - LEDC_CHANNEL_4 / LEDC_TIMER_1 para no colisionar con motores
// ════════════════════════════════════════════════════════════════════

#include "camara.h"
#include "config.h"

// ════════════════════════════════════════════════════════════════════
// MODO_STUXNET con CAMARA_ACTIVA=1 — única combinación que activa
// el servidor HTTP de la cámara
// ════════════════════════════════════════════════════════════════════
#if defined(MODO_STUXNET) && (CAMARA_ACTIVA == 1)

#include "esp_camera.h"
#include "esp_http_server.h"
#include <Arduino.h>

// ── Variables internas ────────────────────────────────────────────
static httpd_handle_t _servidor  = NULL;
static bool           _camaraOK  = false;
static size_t         _framesEnv = 0;

// ════════════════════════════════════════════════════════════════════
// Handler: stream MJPEG sin autenticación (VUL-OI02 activa)
// ════════════════════════════════════════════════════════════════════
static esp_err_t _handlerStream(httpd_req_t* req) {
  camera_fb_t* fb  = NULL;
  esp_err_t    res = ESP_OK;

  if (!_camaraOK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Camara no disponible");
    return ESP_FAIL;
  }

  // Cabecera MJPEG — SIN autenticación (VULNERABILIDAD VUL-OI02)
  res = httpd_resp_set_type(req,
        "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "15");

  Serial.println("[CAM] Cliente conectado al stream (VUL-OI02 activa).");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      delay(100);
      continue;
    }

    char header[80];
    size_t hlen = snprintf(header, sizeof(header),
      "--frame\r\n"
      "Content-Type: image/jpeg\r\n"
      "Content-Length: %u\r\n\r\n",
      (unsigned)fb->len);

    res = httpd_resp_send_chunk(req, header, hlen);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    if (res == ESP_OK)
      res = httpd_resp_send_chunk(req, "\r\n", 2);

    esp_camera_fb_return(fb);
    _framesEnv++;

    if (_framesEnv % 150 == 0) {
      Serial.printf("[CAM] Stream activo — %u frames enviados.\n",
                    (unsigned)_framesEnv);
    }

    if (res != ESP_OK) {
      Serial.println("[CAM] Cliente del stream desconectado.");
      break;
    }
  }
  return res;
}

// ════════════════════════════════════════════════════════════════════
// Handler: foto estática sin autenticación (VUL-OI02 activa)
// ════════════════════════════════════════════════════════════════════
static esp_err_t _handlerFoto(httpd_req_t* req) {
  if (!_camaraOK) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Camara no disponible");
    return ESP_FAIL;
  }

  camera_fb_t* fb = NULL;
  for (int i = 0; i < 3; i++) {
    fb = esp_camera_fb_get();
    if (fb) break;
    delay(200);
  }

  if (!fb) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Error capturando imagen");
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition",
                     "inline; filename=foto.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  esp_err_t res = httpd_resp_send(req,
                  (const char*)fb->buf, fb->len);

  Serial.printf("[CAM] Foto servida — %u bytes.\n",
                (unsigned)fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ════════════════════════════════════════════════════════════════════
// Inicializar sensor OV2640
// ════════════════════════════════════════════════════════════════════
static bool _iniciarSensor() {
  camera_config_t config;

  // Canal LEDC 4 y Timer 1 — NO colisionan con motores
  // (motores usan canales 0 y 2 internamente via ledcAttach)
  config.ledc_channel  = LEDC_CHANNEL_4;
  config.ledc_timer    = LEDC_TIMER_1;

  // Pines verificados — CAMERA_MODEL_ESP32S3_EYE — 30/06/2026
  config.pin_d0        = CAM_PIN_D0;    // GPIO11
  config.pin_d1        = CAM_PIN_D1;    // GPIO9
  config.pin_d2        = CAM_PIN_D2;    // GPIO8
  config.pin_d3        = CAM_PIN_D3;    // GPIO10
  config.pin_d4        = CAM_PIN_D4;    // GPIO12
  config.pin_d5        = CAM_PIN_D5;    // GPIO18
  config.pin_d6        = CAM_PIN_D6;    // GPIO17
  config.pin_d7        = CAM_PIN_D7;    // GPIO16
  config.pin_xclk      = CAM_PIN_XCLK; // GPIO15
  config.pin_pclk      = CAM_PIN_PCLK; // GPIO13
  config.pin_vsync     = CAM_PIN_VSYNC; // GPIO6
  config.pin_href      = CAM_PIN_HREF;  // GPIO7
  config.pin_sccb_sda  = CAM_PIN_SIOD;  // GPIO4
  config.pin_sccb_scl  = CAM_PIN_SIOC;  // GPIO5
  config.pin_pwdn      = CAM_PIN_PWDN;  // -1
  config.pin_reset     = CAM_PIN_RESET; // -1

  config.xclk_freq_hz  = 20000000;          // 20 MHz
  config.pixel_format  = PIXFORMAT_JPEG;
  config.frame_size    = FRAMESIZE_QVGA;    // 320×240 — estable con WiFi
  config.jpeg_quality  = 12;               // 0-63, menor = mejor calidad
  config.fb_count      = 1;               // 1 buffer — evita panic por PSRAM
  config.fb_location   = CAMERA_FB_IN_DRAM; // RAM interna — no depende de PSRAM
  config.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] ERROR init sensor: 0x%x\n", err);
    Serial.println("[CAM] 0x105 = pines incorrectos");
    Serial.println("[CAM] 0x101 = PSRAM no disponible");
    return false;
  }

  // Ajustes del sensor OV2640
  sensor_t* s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_hmirror(s, 0);  // cambiar a 1 si imagen en espejo
    s->set_vflip(s, 0);    // cambiar a 1 si imagen de cabeza
  }

  Serial.println("[CAM] OV2640 inicializada — MODO STUXNET.");
  Serial.println("[CAM] VUL-OI02 activa: stream sin autenticacion.");
  return true;
}

// ════════════════════════════════════════════════════════════════════
// Iniciar servidor HTTP
// ════════════════════════════════════════════════════════════════════
static bool _iniciarServidor() {
  httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
  config.server_port      = 80;
  config.max_uri_handlers = 4;
  config.stack_size       = 8192;

  if (httpd_start(&_servidor, &config) != ESP_OK) {
    Serial.println("[CAM] Error al iniciar servidor HTTP.");
    return false;
  }

  // /stream — sin autenticación (VUL-OI02)
  httpd_uri_t uriStream = {
    .uri      = "/stream",
    .method   = HTTP_GET,
    .handler  = _handlerStream,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(_servidor, &uriStream);

  // /foto — sin autenticación (VUL-OI02)
  httpd_uri_t uriFoto = {
    .uri      = "/foto",
    .method   = HTTP_GET,
    .handler  = _handlerFoto,
    .user_ctx = NULL
  };
  httpd_register_uri_handler(_servidor, &uriFoto);

  Serial.println("[CAM] Servidor HTTP activo en puerto 80.");
  return true;
}

// ════════════════════════════════════════════════════════════════════
// API pública — MODO_STUXNET + CAMARA_ACTIVA=1
// ════════════════════════════════════════════════════════════════════
void iniciarCamara() {
  Serial.println("[CAM] Iniciando en MODO_STUXNET (CAMARA_ACTIVA=1)...");

  _camaraOK = _iniciarSensor();
  if (!_camaraOK) {
    Serial.println("[CAM] Sensor no disponible — stream inactivo.");
    return;
  }

  if (!_iniciarServidor()) {
    Serial.println("[CAM] Servidor no iniciado.");
    _camaraOK = false;
    return;
  }

  Serial.println("[CAM] Lista. URLs disponibles:");
  Serial.println("[CAM]   /stream → stream MJPEG (VUL-OI02)");
  Serial.println("[CAM]   /foto   → captura estatica (VUL-OI02)");
}

void detenerCamara() {
  if (_servidor != NULL) {
    httpd_stop(_servidor);
    _servidor = NULL;
  }
  esp_camera_deinit();
  _camaraOK  = false;
  _framesEnv = 0;
  Serial.println("[CAM] Camara y servidor detenidos.");
}

bool camaraActiva() {
  return _camaraOK;
}

// ════════════════════════════════════════════════════════════════════
// MODO_STUXNET + CAMARA_ACTIVA=0 — Retos 1 a 4
// Cámara desactivada para mantener MQTT estable
// ════════════════════════════════════════════════════════════════════
#elif defined(MODO_STUXNET) && (CAMARA_ACTIVA == 0)

void iniciarCamara() {
  Serial.println("[CAM] Desactivada para Retos 1-4 (CAMARA_ACTIVA=0).");
  Serial.println("[CAM] Cambiar CAMARA_ACTIVA=1 en config.h para el Reto 5.");
  Serial.println("[CAM] MQTT sera completamente estable sin camara activa.");
}

void detenerCamara() {}

bool camaraActiva() {
  return false;
}

// ════════════════════════════════════════════════════════════════════
// MODO_ICSCERT — cámara SIEMPRE desactivada
// Control OI-02 OWASP IoT: sin interfaces de red innecesarias
// ════════════════════════════════════════════════════════════════════
#else

void iniciarCamara() {
  Serial.println("[CAM] MODO_ICSCERT — camara deshabilitada.");
  Serial.println("[CAM] Control OI-02 activo: sin stream expuesto.");
}

void detenerCamara() {}

bool camaraActiva() {
  return false;
}

#endif
