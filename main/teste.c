#include "teste.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "teste";

static SemaphoreHandle_t pronto;       // loop -> servidor: "respondi"
static volatile int  cmd = TESTE_NADA; // servidor -> loop: o que fazer
static volatile bool ativo = false;
static int64_t  ultimo_cmd;            // p/ sair sozinho se o PC sumir
static uint8_t *jpg;
static size_t   jpg_tam;
static uint32_t t_ms;
static char     resposta[512];

// Passa o comando p/ o loop e espera a resposta (ate 10 s)
static esp_err_t executa(httpd_req_t *req, int c)
{
    xSemaphoreTake(pronto, 0);         // descarta aviso velho
    ultimo_cmd = esp_timer_get_time();
    if (c == TESTE_INICIO) ativo = true;
    cmd = c;
    if (xSemaphoreTake(pronto, pdMS_TO_TICKS(10000)) != pdTRUE) {
        cmd = TESTE_NADA;
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "placa nao respondeu");
        return ESP_FAIL;
    }
    if (c == TESTE_SAIR) ativo = false;
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, resposta, HTTPD_RESP_USE_STRLEN);
}

// le ?t=1234 da URL
static uint32_t le_t(httpd_req_t *req)
{
    char q[48], v[16];
    if (httpd_req_get_url_query_str(req, q, sizeof q) == ESP_OK &&
        httpd_query_key_value(q, "t", v, sizeof v) == ESP_OK)
        return (uint32_t)strtoul(v, NULL, 10);
    return 0;
}

static esp_err_t h_inicio(httpd_req_t *req) { return executa(req, TESTE_INICIO); }
static esp_err_t h_sair(httpd_req_t *req)   { return executa(req, TESTE_SAIR); }
static esp_err_t h_fim(httpd_req_t *req)
{
    t_ms = le_t(req);
    return executa(req, TESTE_FIM);
}

static esp_err_t h_quadro(httpd_req_t *req)
{
    size_t tam = req->content_len;
    if (tam == 0 || tam > 200000) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "JPEG invalido");
        return ESP_FAIL;
    }
    uint8_t *buf = heap_caps_malloc(tam, MALLOC_CAP_SPIRAM);
    if (!buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "sem memoria");
        return ESP_FAIL;
    }
    size_t lido = 0;                   // o corpo chega em pedacos
    while (lido < tam) {
        int r = httpd_req_recv(req, (char *)buf + lido, tam - lido);
        if (r <= 0) {
            heap_caps_free(buf);
            return ESP_FAIL;
        }
        lido += r;
    }
    jpg = buf; jpg_tam = tam; t_ms = le_t(req);
    esp_err_t e = executa(req, TESTE_QUADRO);
    jpg = NULL;
    heap_caps_free(buf);
    return e;
}

void teste_registra(httpd_handle_t srv)
{
    pronto = xSemaphoreCreateBinary();
    httpd_uri_t u[] = {
        { .uri = "/teste/inicio", .method = HTTP_GET,  .handler = h_inicio },
        { .uri = "/teste/quadro", .method = HTTP_POST, .handler = h_quadro },
        { .uri = "/teste/fim",    .method = HTTP_GET,  .handler = h_fim },
        { .uri = "/teste/sair",   .method = HTTP_GET,  .handler = h_sair },
    };
    for (int i = 0; i < 4; i++) httpd_register_uri_handler(srv, &u[i]);
}

bool teste_ativo(void)
{
    // PC parou de mandar ha 30 s: volta p/ a camera sozinho
    if (ativo && esp_timer_get_time() - ultimo_cmd > 30000000) {
        ESP_LOGW(TAG, "PC sumiu: saindo do modo teste");
        ativo = false;
    }
    return ativo;
}

int teste_pedido(const uint8_t **j, size_t *tam, uint32_t *t)
{
    int c = cmd;
    if (c == TESTE_NADA) return TESTE_NADA;
    *j = jpg; *tam = jpg_tam; *t = t_ms;
    return c;
}

void teste_responde(const char *texto)
{
    snprintf(resposta, sizeof resposta, "%s", texto);
    cmd = TESTE_NADA;
    xSemaphoreGive(pronto);
}
