#include "image_source.hpp"
#include "pedestrian_detect.hpp"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include "camera_source.h"
#include "quedas.h"
#include "wifi.h"
#include "web.h"
#include "teste.h"
#include "sirene.h"
}

static const char *TAG = "main";

// Rede Wi-Fi (so 2.4 GHz: o ESP32 nao enxerga redes 5 GHz)
#define WIFI_REDE  "NOME_DA_REDE"
#define WIFI_SENHA "SENHA_DA_REDE"

// Monta a particao "storage" em /spiffs (os .jpg da pasta imagens/)
static void monta_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {};
    conf.base_path       = "/spiffs";
    conf.partition_label = "storage";
    conf.max_files       = 4;
    ESP_ERROR_CHECK(esp_vfs_spiffs_register(&conf));
}

// Alguem ainda esta com alarme de queda (caido e nao levantou)?
static bool alguem_caido(void)
{
    const pessoa_t *p = quedas_pessoas();
    for (int i = 0; i < MAX_PESSOAS; i++)
        if (p[i].ativo && p[i].alertou) return true;
    return false;
}

// Chamado pelo modulo de quedas. Futuro: MQTT/Telegram aqui.
// Queda => toca a sirene (ate apertarem o botao). Levantou => para,
// se ninguem mais estiver caido.
static void alerta(const pessoa_t *p, const char *motivo)
{
    ESP_LOGW(TAG, ">>> pessoa %d: %s (vel=%.2f)", p->id, motivo, p->vel_queda);
    if (strncmp(motivo, "QUEDA", 5) == 0)
        sirene_liga();
    else if (!alguem_caido())
        sirene_desliga();
}

static uint32_t t_frame;   // hora do ultimo frame da camera (p/ o desenho)

// Roda o modelo: devolve quantas pessoas viu e as caixas delas
static int roda_modelo(PedestrianDetect *modelo, dl::image::img_t &img,
                       caixa_t *caixas)
{
    int n = 0;
    auto &res = modelo->run(img);
    for (const auto &r : res) {            // loop nas pessoas do frame
        if (n == MAX_PESSOAS) break;
        caixas[n].x1 = r.box[0];
        caixas[n].y1 = r.box[1];
        caixas[n].x2 = r.box[2];
        caixas[n].y2 = r.box[3];
        caixas[n].score = r.score;
        n++;
    }
    return n;
}

// Roda o modelo e passa as pessoas p/ o modulo de quedas.
// Devolve em caixas[]/n tudo o que o detector viu (ate as fracas).
// NAO libera a imagem: quem chamou libera.
static void detecta(PedestrianDetect *modelo, dl::image::img_t &img,
                    const char *origem, caixa_t *caixas, int &n)
{
    n = 0;
    if (!img.data) return;
    n = roda_modelo(modelo, img, caixas);

    uint32_t agora = (uint32_t)(esp_timer_get_time() / 1000);
    t_frame = agora;
    quedas_update(caixas, n, agora, alerta);

    // o que o detector VIU neste frame
    ESP_LOGI(TAG, "%s: detector viu %d", origem, n);
    for (int k = 0; k < n; k++)
        ESP_LOGI(TAG, "   caixa score=%.2f (%d,%d)-(%d,%d)", caixas[k].score,
                 (int)caixas[k].x1, (int)caixas[k].y1,
                 (int)caixas[k].x2, (int)caixas[k].y2);

    // quem o rastreio ainda LEMBRA (pode nao estar na imagem agora)
    const pessoa_t *p = quedas_pessoas();
    for (int i = 0; i < MAX_PESSOAS; i++) {
        if (!p[i].ativo || !p[i].confirmada) continue;  // pula fantasma
        uint32_t sumiu = agora - p[i].t_visto;
        if (sumiu == 0)
            ESP_LOGI(TAG, "   pessoa %d h/w=%.2f %s", p[i].id,
                     p[i].h / p[i].w, p[i].t_caido ? "NO CHAO" : "");
        else
            ESP_LOGI(TAG, "   pessoa %d (memoria: sumiu ha %.1f s) %s",
                     p[i].id, sumiu / 1000.0f,
                     p[i].t_caido ? "NO CHAO" : "");
    }
}

// Desenha em vermelho (2 px) so quem o programa considera PESSOA:
// confirmada (3 frames) e vista neste frame. Caixa fraca do detector
// (score < 0.7 em pe, ex.: ventilador) nao vira pessoa e nao e' desenhada.
// Frame RGB565 big-endian: vermelho = bytes F8 00
static void desenha_pessoas(dl::image::img_t &img)
{
    uint8_t *px = (uint8_t *)img.data;
    int W = img.width, H = img.height;
    auto ponto = [&](int x, int y) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        uint8_t *q = px + (y * W + x) * 2;
        q[0] = 0xF8; q[1] = 0x00;
    };
    const pessoa_t *p = quedas_pessoas();
    for (int i = 0; i < MAX_PESSOAS; i++) {
        if (!p[i].ativo || !p[i].confirmada || p[i].t_visto != t_frame)
            continue;
        int x1 = p[i].cx - p[i].w / 2, x2 = p[i].cx + p[i].w / 2;
        int y1 = p[i].cy - p[i].h / 2, y2 = p[i].cy + p[i].h / 2;
        for (int e = 0; e < 2; e++) {          // espessura 2
            for (int x = x1; x <= x2; x++) { ponto(x, y1 + e); ponto(x, y2 - e); }
            for (int y = y1; y <= y2; y++) { ponto(x1 + e, y); ponto(x2 - e, y); }
        }
    }
}

// ---- Modo teste: quadros de video vindos do PC (teste.c) ----
static char     t_alarme[64];          // 1o alarme do video ("" = nenhum)
static float    t_vmax;                // maior velocidade de tombo vista
static caixa_t  t_ult[MAX_PESSOAS];    // caixas do ultimo quadro
static int      t_nult;

static void alerta_teste(const pessoa_t *p, const char *motivo)
{
    if (!t_alarme[0]) snprintf(t_alarme, sizeof t_alarme, "%s", motivo);
}

static void anota_vmax(void)
{
    const pessoa_t *p = quedas_pessoas();
    for (int i = 0; i < MAX_PESSOAS; i++)
        if (p[i].ativo && p[i].vel_queda > t_vmax) t_vmax = p[i].vel_queda;
}

static void trata_teste(PedestrianDetect *modelo, int cmd,
                        const uint8_t *jpg, size_t tam, uint32_t t)
{
    char r[512];
    if (cmd == TESTE_INICIO || cmd == TESTE_SAIR) {   // video novo / acabou
        quedas_init();
        t_alarme[0] = 0; t_vmax = 0; t_nult = 0;
        teste_responde("ok");
    } else if (cmd == TESTE_QUADRO) {
        dl::image::img_t img = get_image_jpeg(jpg, tam);
        int n = 0;
        if (img.data) n = roda_modelo(modelo, img, t_ult);
        free_image(&img);
        t_nult = n;
        quedas_update(t_ult, n, t, alerta_teste);
        anota_vmax();
        int k = 0;                     // resposta: uma caixa por linha
        r[0] = 0;
        for (int i = 0; i < n; i++)
            k += snprintf(r + k, sizeof r - k, "%d,%d,%d,%d,%.3f\n",
                          (int)t_ult[i].x1, (int)t_ult[i].y1,
                          (int)t_ult[i].x2, (int)t_ult[i].y2, t_ult[i].score);
        teste_responde(r);
    } else if (cmd == TESTE_FIM) {
        // o video acabou: a pessoa continua como no ultimo quadro por 70 s
        for (int i = 0; i < 210 && !t_alarme[0]; i++) {
            t += 333;
            quedas_update(t_ult, t_nult, t, alerta_teste);
            anota_vmax();
        }
        snprintf(r, sizeof r, "%s;%.2f", t_alarme[0] ? t_alarme : "-", t_vmax);
        teste_responde(r);
    }
}

extern "C" void app_main(void)
{
    sirene_init();   // primeiro: garante a sirene desligada desde o boot
    if (!wifi_conecta(WIFI_REDE, WIFI_SENHA, 15000))
        ESP_LOGW(TAG, "Wi-Fi ainda nao conectou — segue tentando sozinho");
    // liga a pagina SEMPRE: se o Wi-Fi conectar depois, ela ja esta pronta
    web_inicia();

    monta_spiffs();
    ESP_ERROR_CHECK(camera_init() == 0 ? ESP_OK : ESP_FAIL);
    PedestrianDetect *modelo = new PedestrianDetect();
    // padrao do modelo e' 0.7. Deitada a pessoa pontua 0.3-0.6, entao o
    // detector entrega tudo acima de 0.3; o quedas.c exige 0.7 de quem
    // esta em pe e so cria pessoa nova em pe, o que segura os fantasmas.
    modelo->set_score_thr(0.3f);
    quedas_init();

    // 1x: testa o modelo com uma foto conhecida
    caixa_t caixas[MAX_PESSOAS];
    int n;
    dl::image::img_t img = get_image_file("/spiffs/teste.jpg");
    detecta(modelo, img, "arquivo", caixas, n);
    free_image(&img);
    quedas_init();   // a foto de teste nao conta como gente na sala

    // camera ao vivo
    int64_t t_sinal = 0;
    while (true) {
        // PC mandou algo do modo teste? atende primeiro
        const uint8_t *jpg; size_t tam; uint32_t t;
        int cmd = teste_pedido(&jpg, &tam, &t);
        if (cmd != TESTE_NADA) {
            trata_teste(modelo, cmd, jpg, tam, t);
            continue;
        }
        if (teste_ativo()) {           // em teste a camera fica parada
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        img = get_image_cam();
        detecta(modelo, img, "camera", caixas, n);
        if (img.data && web_quer_foto()) {  // navegador pediu: manda este
            desenha_pessoas(img);           // frame, com as pessoas
            web_entrega_foto((const uint8_t *)img.data, img.width, img.height);
        }
        free_image(&img);

        // a cada 10 s mostra a forca do sinal Wi-Fi
        if (esp_timer_get_time() - t_sinal > 10000000) {
            t_sinal = esp_timer_get_time();
            ESP_LOGI(TAG, "sinal Wi-Fi: %d dBm", wifi_rssi());
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
