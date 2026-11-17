#include "web.h"
#include "teste.h"
#include <stdlib.h>
#include "esp_http_server.h"
#include "img_converters.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "web";

static SemaphoreHandle_t foto_pronta;   // loop -> servidor: "a foto chegou"
static SemaphoreHandle_t trava;         // protege jpg/jpg_tam
static volatile bool pedido = false;    // servidor -> loop: "quero foto"
static uint8_t *jpg = NULL;
static size_t   jpg_tam = 0;

// Pagina: mostra /foto.jpg e pede a proxima assim que a anterior chega
static const char PAGINA[] =
    "<!doctype html><meta name=viewport content='width=device-width'>"
    "<body style='margin:0;background:#000'>"
    "<img id=f style='width:100%' src='/foto.jpg'>"
    "<script>var f=document.getElementById('f');"
    "f.onload=f.onerror=function(){setTimeout(function(){"
    "f.src='/foto.jpg?'+Date.now()},100)}</script>";

static esp_err_t pagina(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, PAGINA, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t foto(httpd_req_t *req)
{
    xSemaphoreTake(foto_pronta, 0);     // descarta aviso velho
    pedido = true;
    if (xSemaphoreTake(foto_pronta, pdMS_TO_TICKS(3000)) != pdTRUE) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sem foto");
        return ESP_FAIL;
    }
    xSemaphoreTake(trava, portMAX_DELAY);
    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t r = httpd_resp_send(req, (const char *)jpg, jpg_tam);
    xSemaphoreGive(trava);
    return r;
}

bool web_inicia(void)
{
    foto_pronta = xSemaphoreCreateBinary();
    trava       = xSemaphoreCreateMutex();

    httpd_handle_t srv = NULL;
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&srv, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "servidor nao subiu");
        return false;
    }
    httpd_uri_t u_pagina = { .uri = "/",         .method = HTTP_GET, .handler = pagina };
    httpd_uri_t u_foto   = { .uri = "/foto.jpg", .method = HTTP_GET, .handler = foto };
    httpd_register_uri_handler(srv, &u_pagina);
    httpd_register_uri_handler(srv, &u_foto);
    teste_registra(srv);               // /teste/... (videos vindos do PC)
    ESP_LOGI(TAG, "pagina no ar: abra http://<IP do ESP32>/ no navegador");
    return true;
}

bool web_quer_foto(void) { return pedido; }

void web_entrega_foto(const uint8_t *rgb565, int largura, int altura)
{
    if (!pedido) return;
    uint8_t *novo = NULL;
    size_t   tam  = 0;
    // RGB565 da camera -> JPEG (qualidade 80)
    if (!fmt2jpg((uint8_t *)rgb565, (size_t)largura * altura * 2, largura, altura,
                 PIXFORMAT_RGB565, 80, &novo, &tam)) {
        ESP_LOGE(TAG, "falha ao gerar JPEG");
        return;
    }
    xSemaphoreTake(trava, portMAX_DELAY);
    free(jpg);
    jpg = novo;
    jpg_tam = tam;
    xSemaphoreGive(trava);
    pedido = false;
    xSemaphoreGive(foto_pronta);
}
