#include "sirene.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sirene";

// Pinos livres na Freenove ESP32-S3 WROOM CAM (a camera usa 4-13, 15-18;
// o cartao SD usa 38-40). Mudou de placa? Confira o esquematico.
#define PINO_SIRENE  GPIO_NUM_14  // saida: 1 = MOSFET conduz = sirene toca
#define PINO_BOTAO   GPIO_NUM_21  // entrada: botao entre o pino e o GND
#define BIP_MS       500          // toque intermitente: 0,5 s liga, 0,5 s desliga
#define PASSO_MS     10           // a tarefa acorda a cada 10 ms
#define N_DEBOUNCE   5            // botao apertado 5 leituras seguidas (50 ms)

static volatile bool tocando = false;

static void tarefa(void *arg)
{
    int apertado = 0;             // leituras seguidas com o botao em 0
    uint32_t t = 0;               // ms desde que comecou a tocar
    while (true) {
        // botao: so conta depois de N_DEBOUNCE leituras iguais (o contato
        // mecanico "pula" por alguns ms ao apertar)
        if (gpio_get_level(PINO_BOTAO) == 0) {
            if (++apertado == N_DEBOUNCE && tocando) {
                ESP_LOGW(TAG, "botao: sirene desligada");
                tocando = false;
            }
        } else {
            apertado = 0;
        }

        if (tocando) {
            gpio_set_level(PINO_SIRENE, (t / BIP_MS) % 2 == 0);
            t += PASSO_MS;
        } else {
            gpio_set_level(PINO_SIRENE, 0);
            t = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(PASSO_MS));
    }
}

void sirene_init(void)
{
    gpio_config_t saida = {
        .pin_bit_mask = 1ULL << PINO_SIRENE,
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&saida);
    gpio_set_level(PINO_SIRENE, 0);

    gpio_config_t entrada = {
        .pin_bit_mask = 1ULL << PINO_BOTAO,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,   // solto = 1, apertado = 0
    };
    gpio_config(&entrada);

    xTaskCreate(tarefa, "sirene", 2048, NULL, 5, NULL);
}

void sirene_liga(void)
{
    if (!tocando) ESP_LOGW(TAG, "sirene LIGADA");
    tocando = true;
}

void sirene_desliga(void)
{
    if (tocando) ESP_LOGI(TAG, "sirene desligada");
    tocando = false;
}

bool sirene_tocando(void) { return tocando; }
