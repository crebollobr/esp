#ifndef QUEDAS_H
#define QUEDAS_H
#include <stdint.h>
#include <stdbool.h>

// Deteccao de queda por pessoa, sem dependencia de hardware (roda no PC).
// Cada frame: passe os boxes do detector -> o modulo descobre "quem e' quem"
// (pelo box mais proximo do frame anterior) e mede a queda de cada pessoa.

#define MAX_PESSOAS 5

typedef struct {
    float x1, y1, x2, y2;   // box do detector (canto sup. esq. / inf. dir.)
    float score;
} caixa_t;

typedef struct {
    bool     ativo;
    int      id;              // numero fixo enquanto a pessoa e' seguida
    float    cx, cy, w, h;    // ultima posicao
    uint32_t t_visto;         // ultimo frame em que apareceu
    uint32_t t_em_pe;         // ultimo frame em pe (0 = nunca vista em pe)
    float    razao_em_pe;     // h/w nesse momento
    uint32_t t_caido;         // desde quando esta no chao (0 = nao esta)
    float    vel_queda;       // velocidade do tombo (razao h/w por segundo)
    bool     alertou;
    int      vistos;          // frames seguidos em que apareceu
    bool     confirmada;      // vista N_CONFIRMA vezes seguidas: e' gente
    float    score;           // confianca do detector no ultimo frame
    // historico das 3 ultimas vezes que foi vista: da a velocidade
    // vertical e horizontal do PASSADO (p/ "caiu e sumiu" e "desceu")
    uint32_t hist_t[3];
    float    hist_topo[3];    // y1 da caixa: onde esta a cabeca
    float    hist_h[3];       // altura da caixa
    float    hist_cx[3];      // centro x da caixa
    int      nhist;
    float    topo_em_pe;      // cabeca na ultima vez que estava em pe
    float    h_em_pe;         // altura na ultima vez que estava em pe
    uint32_t t_desceu;        // quando desceu rapido p/ baixo (0 = nao)
} pessoa_t;

typedef void (*alerta_cb)(const pessoa_t *p, const char *motivo);

void            quedas_init(void);
void            quedas_update(const caixa_t *caixas, int n, uint32_t t_ms,
                              alerta_cb cb);
const pessoa_t *quedas_pessoas(void);   // vetor de MAX_PESSOAS posicoes
#endif
