#include "quedas.h"
#include <math.h>
#include <string.h>

// ---- Parametros (calibrar com videos reais!) ----
#define RAZAO_EM_PE      1.5f    // h/w acima disso = em pe
#define RAZAO_CAIDO      0.7f    // h/w abaixo disso = no chao (0.7: calibrado
                                 // c/ as caixas da placa; 0.8 pegava sentar)
#define VEL_QUEDA_MIN    1.0f    // razao h/w perdida por segundo: acima = tombo
                                 // (1.0 calibrado c/ 70 videos URFD)
#define T_CONFIRMA_MS    5000    // tombo rapido + 5 s no chao  => alerta
#define T_CHAO_MAX_MS    60000   // 60 s no chao (mesmo devagar) => alerta
#define T_ESQUECE_MS     3000    // sumiu 3 s => esquece a pessoa
#define T_ESQUECE_CAIDO_MS 30000 // caida o detector pisca: espera mais
#define T_ESQUECE_ALERTOU_MS 300000 // alertou: lembra ate 5 min sumida
#define DIST_MAX         0.6f    // casa box se o centro andou < 0.6 x tamanho
                                 // (1.0 deixava a cadeira "roubar" a pessoa)
#define N_CONFIRMA       3       // fantasma: so vira pessoa apos 3 frames seguidos
#define SCORE_EM_PE      0.7f    // caixa em pe so vale com score alto: gente
                                 // em pe pontua ~0.9; ventilador (alto e fino)
                                 // pontua menos. Sentada/deitada: 0.3 basta
#define ALTURA_IMG       240     // QVGA (320x240)
#define VEL_SUMIU        0.6f    // sumiu com a cabeca descendo mais rapido que
                                 // 0.6 alturas/s => caiu e sumiu (ex.: atras
                                 // de um movel). Quem sai andando: ~0
#define VEL_DESCE        0.7f    // cabeca descendo > 0.7 alturas/s ...
#define DESCE_MIN        0.4f    // ... e ja desceu 40% da altura em pe
                                 // => "desceu p/ o chao" (mesmo se a caixa
                                 // nao deita: queda na direcao da camera)

static pessoa_t pessoas[MAX_PESSOAS];
static int prox_id = 1;

void quedas_init(void)
{
    memset(pessoas, 0, sizeof pessoas);
    prox_id = 1;
}

const pessoa_t *quedas_pessoas(void) { return pessoas; }

// "Quem e' quem": liga cada caixa nova a uma pessoa conhecida.
// Escolhe sempre o par (caixa, pessoa) MAIS PERTO de todos, depois o
// proximo par mais perto, e assim por diante. Decidir caixa por caixa
// pode trocar duas pessoas lado a lado; assim nao troca.
// Resultado: dono[k] = vaga da pessoa da caixa k, ou -1 (pessoa nova).
static void casa(const caixa_t *caixas, int n, int *dono)
{
    bool caixa_usada[MAX_PESSOAS]  = { false };
    bool pessoa_usada[MAX_PESSOAS] = { false };
    for (int k = 0; k < n; k++) dono[k] = -1;

    while (true) {
        int   melhor_k = -1, melhor_i = -1;
        float dmin = 1e9f;
        for (int k = 0; k < n; k++) {
            if (caixa_usada[k]) continue;
            const caixa_t *b = &caixas[k];
            float w = b->x2 - b->x1, h = b->y2 - b->y1;
            if (w <= 0 || h <= 0) continue;   // caixa invalida
            float cx = (b->x1 + b->x2) / 2, cy = (b->y1 + b->y2) / 2;
            for (int i = 0; i < MAX_PESSOAS; i++) {
                const pessoa_t *p = &pessoas[i];
                if (!p->ativo || pessoa_usada[i]) continue;
                // ninguem sai do chao direto p/ em pe em um frame (precisa
                // passar por sentado): deitada no frame anterior nao pode
                // virar um retangulo em pe agora
                if (p->h / p->w < RAZAO_CAIDO && h / w > RAZAO_EM_PE) continue;
                float d   = hypotf(cx - p->cx, cy - p->cy);  // distancia na imagem
                float lim = DIST_MAX * fmaxf(fmaxf(w, h), fmaxf(p->w, p->h));
                if (d < lim && d < dmin) { dmin = d; melhor_k = k; melhor_i = i; }
            }
        }
        if (melhor_k < 0) break;              // nao sobrou par possivel
        dono[melhor_k] = melhor_i;
        caixa_usada[melhor_k]  = true;
        pessoa_usada[melhor_i] = true;
    }
}

// Guarda a posicao no historico (so as 3 ultimas)
static void guarda_hist(pessoa_t *p, uint32_t t, float topo, float h, float cx)
{
    if (p->nhist == 3) {               // cheio: descarta a mais velha
        for (int j = 0; j < 2; j++) {
            p->hist_t[j]    = p->hist_t[j + 1];
            p->hist_topo[j] = p->hist_topo[j + 1];
            p->hist_h[j]    = p->hist_h[j + 1];
            p->hist_cx[j]   = p->hist_cx[j + 1];
        }
        p->nhist = 2;
    }
    p->hist_t[p->nhist]    = t;
    p->hist_topo[p->nhist] = topo;
    p->hist_h[p->nhist]    = h;
    p->hist_cx[p->nhist]   = cx;
    p->nhist++;
}

// Velocidade do PASSADO: compara a posicao mais velha do historico com
// a mais nova. Em "alturas da pessoa por segundo" (nao depende da
// distancia ate a camera). vy > 0: cabeca descendo. vx > 0: p/ direita.
// Sem historico (menos de 2 posicoes): 0.
static void velocidade(const pessoa_t *p, float *vx, float *vy)
{
    *vx = *vy = 0;
    if (p->nhist < 2) return;
    int u = p->nhist - 1;
    float dt = (p->hist_t[u] - p->hist_t[0]) / 1000.0f;
    if (dt <= 0) return;
    *vx = (p->hist_cx[u]   - p->hist_cx[0])   / p->hist_h[0] / dt;
    *vy = (p->hist_topo[u] - p->hist_topo[0]) / p->hist_h[0] / dt;
}

static int slot_livre(void)
{
    for (int i = 0; i < MAX_PESSOAS; i++)
        if (!pessoas[i].ativo) return i;
    return -1;
}

void quedas_update(const caixa_t *todas, int n_todas, uint32_t t, alerta_cb cb)
{
    // 0) score conforme a forma: em pe exige SCORE_EM_PE, o resto passa
    caixa_t caixas[MAX_PESSOAS];
    int n = 0;
    for (int k = 0; k < n_todas && n < MAX_PESSOAS; k++) {
        const caixa_t *b = &todas[k];
        float w = b->x2 - b->x1, h = b->y2 - b->y1;
        if (w > 0 && h / w > RAZAO_EM_PE && b->score < SCORE_EM_PE) continue;
        // encosta em cima E embaixo: o corpo esta cortado (ou e' parede,
        // porta, objeto colado na lente). Nao da p/ saber a postura: ignora
        if (b->y1 <= 2 && b->y2 >= ALTURA_IMG - 3) continue;
        caixas[n++] = *b;
    }

    int dono[MAX_PESSOAS];
    casa(caixas, n, dono);

    // 1) loop nas pessoas detectadas neste frame
    for (int k = 0; k < n; k++) {
        const caixa_t *b = &caixas[k];
        float w = b->x2 - b->x1, h = b->y2 - b->y1;
        if (w <= 0 || h <= 0) continue;
        float cx = (b->x1 + b->x2) / 2, cy = (b->y1 + b->y2) / 2;

        int i = dono[k];
        if (i < 0) {                       // pessoa nova
            // pessoa so NASCE em pe (e em pe o score ja e' >= SCORE_EM_PE).
            // Deitada com score baixo so vale se veio de alguem em pe que
            // caiu; deitada que surge do nada e' suspeita: ignora
            if (h / w <= RAZAO_EM_PE) continue;
            i = slot_livre();
            if (i < 0) continue;           // lotado: ignora
            memset(&pessoas[i], 0, sizeof pessoas[i]);
            pessoas[i].ativo = true;
            pessoas[i].id    = prox_id++;
        }

        pessoa_t *p = &pessoas[i];
        p->cx = cx; p->cy = cy; p->w = w; p->h = h;
        p->t_visto = t;
        p->score   = b->score;
        guarda_hist(p, t, b->y1, h, cx);
        if (++p->vistos >= N_CONFIRMA) p->confirmada = true;

        float r = h / w;

        // desceu p/ o chao? Cabeca descendo rapido (historico) e ja bem
        // abaixo de onde estava em pe. Pega a queda na direcao da camera,
        // em que a caixa fica quadrada (h/w ~1) e nunca parece deitada
        if (p->t_em_pe && !p->t_desceu && r <= RAZAO_EM_PE) {
            float vx, vy;
            velocidade(p, &vx, &vy);
            float desceu = (b->y1 - p->topo_em_pe) / p->h_em_pe;
            if (vy > VEL_DESCE && desceu > DESCE_MIN) p->t_desceu = t;
        }
        if (r > RAZAO_EM_PE) {             // em pe: zera a queda
            p->t_em_pe     = t;
            p->razao_em_pe = r;
            p->topo_em_pe  = b->y1;
            p->h_em_pe     = h;
            p->t_desceu    = 0;
            p->t_caido     = 0;
            p->vel_queda   = 0;
            if (p->alertou) {
                p->alertou = false;
                cb(p, "levantou");
            }
        } else if (r < RAZAO_CAIDO && p->t_caido == 0) {
            // acabou de chegar ao chao: mede a velocidade do tombo
            // (quanto de "verticalidade" perdeu por segundo)
            p->t_caido = t;
            if (p->t_em_pe) {
                float dt = (t - p->t_em_pe) / 1000.0f;
                p->vel_queda = (p->razao_em_pe - r) / fmaxf(dt, 0.1f);
            }
        }
    }

    // 2) loop em todas as pessoas seguidas: alerta e limpeza
    for (int i = 0; i < MAX_PESSOAS; i++) {
        pessoa_t *p = &pessoas[i];
        if (!p->ativo) continue;
        uint32_t sumido = t - p->t_visto;

        // fantasma: o detector pisca em sombra/objeto por 1 ou 2 frames.
        // Ainda nao confirmada e falhou 1 frame => apaga sem alertar.
        if (!p->confirmada) {
            if (sumido > 0) p->ativo = false;
            continue;
        }

        // desceu rapido e ficou embaixo 5 s (vista agora, sem levantar)
        if (p->t_desceu && !p->alertou && !p->t_caido && sumido == 0
            && t - p->t_desceu > T_CONFIRMA_MS) {
            p->alertou = true;
            cb(p, "QUEDA (desceu rapido e ficou embaixo)");
        }

        if (p->t_caido) {
            uint32_t no_chao = t - p->t_caido;
            if (!p->alertou) {
                if (p->vel_queda > VEL_QUEDA_MIN && no_chao > T_CONFIRMA_MS) {
                    p->alertou = true;
                    cb(p, "QUEDA (tombo rapido e nao levantou)");
                } else if (no_chao > T_CHAO_MAX_MS && p->t_em_pe) {
                    // so quem ja foi vista EM PE: "pessoa" que nasceu
                    // deitada e nunca levantou e' objeto parado (fantasma)
                    p->alertou = true;
                    cb(p, "QUEDA (muito tempo no chao)");
                }
            }
            // caida que ja alertou: lembra mais (alguem pode estar na
            // frente dela), mas nao p/ sempre, senao fantasma vira eterno
            uint32_t lim = p->alertou ? T_ESQUECE_ALERTOU_MS
                                      : T_ESQUECE_CAIDO_MS;
            if (sumido > lim)
                p->ativo = false;
        } else {
            // sumiu sem ter sido vista no chao: saiu andando ou caiu fora
            // da vista? A velocidade da cabeca antes de sumir decide
            float vx, vy;
            velocidade(p, &vx, &vy);
            bool caiu = !p->alertou && vy > VEL_SUMIU;
            if (caiu && sumido > T_CONFIRMA_MS) {
                p->alertou = true;
                p->t_caido = p->t_visto;   // daqui p/ frente: como caida
                cb(p, "QUEDA (caiu e sumiu)");
            } else if (!caiu && sumido > T_ESQUECE_MS) {
                p->ativo = false;
            }
        }
    }
}
