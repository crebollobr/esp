// Driver real da camera (componente espressif/esp32-camera).
// ATENCAO: pinos do modulo "Freenove ESP32-S3 WROOM CAM".
// Cada placa tem pinagem diferente — confira o esquematico da SUA placa.
#include "camera_source.h"
#include "esp_camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

#define CAM_PIN_PWDN  -1
#define CAM_PIN_RESET -1
#define CAM_PIN_XCLK  15
#define CAM_PIN_SIOD  4
#define CAM_PIN_SIOC  5
#define CAM_PIN_D7    16
#define CAM_PIN_D6    17
#define CAM_PIN_D5    18
#define CAM_PIN_D4    12
#define CAM_PIN_D3    10
#define CAM_PIN_D2    8
#define CAM_PIN_D1    9
#define CAM_PIN_D0    11
#define CAM_PIN_VSYNC 6
#define CAM_PIN_HREF  7
#define CAM_PIN_PCLK  13

static frame_t frame_atual;

int camera_init(void)
{
    camera_config_t cfg = {
        .pin_pwdn = CAM_PIN_PWDN,   .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD, .pin_sccb_scl = CAM_PIN_SIOC,
        .pin_d7 = CAM_PIN_D7, .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5, .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3, .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1, .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC, .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_RGB565,    // ESP-DL come RGB, nao JPEG
        .frame_size   = FRAMESIZE_QVGA,      // 320x240
        .fb_count     = 2,                   // 2 buffers na PSRAM
        .fb_location  = CAMERA_FB_IN_PSRAM,
        .grab_mode    = CAMERA_GRAB_LATEST,  // sempre o frame mais novo
    };
    esp_err_t err = esp_camera_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "falha init camera: 0x%x", err);
        return -1;
    }
    return 0;
}

frame_t *camera_get_frame(void)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return NULL;
    frame_atual.dados   = fb->buf;
    frame_atual.tam     = fb->len;
    frame_atual.largura = fb->width;
    frame_atual.altura  = fb->height;
    frame_atual.priv    = fb;   // guarda p/ devolver (1 frame em voo por vez)
    return &frame_atual;
}

void camera_release_frame(frame_t *f)
{
    if (f && f->priv) esp_camera_fb_return((camera_fb_t *)f->priv);
}
