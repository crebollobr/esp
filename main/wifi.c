#include "wifi.h"
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi";
static EventGroupHandle_t eventos;
#define BIT_CONECTOU BIT0

// Chamada pelo sistema a cada novidade do Wi-Fi
static void ao_evento(void *arg, esp_event_base_t base, int32_t id, void *dados)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();                  // radio ligou: tenta conectar
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "nao conectou / caiu — tentando de novo");
        xEventGroupClearBits(eventos, BIT_CONECTOU);
        esp_wifi_connect();                  // tenta para sempre
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)dados;
        ESP_LOGI(TAG, "conectado! IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(eventos, BIT_CONECTOU);
    }
}

// copia texto p/ campo de tamanho fixo sem estourar
static void copia(uint8_t *dst, size_t tam, const char *src)
{
    size_t n = strlen(src);
    if (n > tam) n = tam;
    memcpy(dst, src, n);
}

bool wifi_conecta(const char *rede, const char *senha, int timeout_ms)
{
    // 1) NVS: memoria onde o Wi-Fi guarda calibracao do radio
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // 2) pilha de rede (TCP/IP) e sistema de eventos
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    eventos = xEventGroupCreate();

    // 3) liga o driver Wi-Fi e avisa quem trata os eventos
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               ao_evento, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               ao_evento, NULL));

    // 4) nome e senha da rede, modo cliente (STA)
    wifi_config_t wc = { 0 };
    copia(wc.sta.ssid, sizeof wc.sta.ssid, rede);
    copia(wc.sta.password, sizeof wc.sta.password, senha);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());   // dispara WIFI_EVENT_STA_START
    // sem economia de energia: o radio fica sempre ouvindo. Por padrao ele
    // "cochila" entre beacons do roteador -> ping lento e pacotes perdidos
    esp_wifi_set_ps(WIFI_PS_NONE);

    // 5) espera o IP chegar (ou desiste depois de timeout_ms)
    EventBits_t bits = xEventGroupWaitBits(eventos, BIT_CONECTOU, pdFALSE,
                                           pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (bits & BIT_CONECTOU) != 0;
}

int wifi_rssi(void)
{
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) != ESP_OK) return 0;
    return ap.rssi;
}
