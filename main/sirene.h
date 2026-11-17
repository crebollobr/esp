#ifndef SIRENE_H
#define SIRENE_H
#include <stdbool.h>

// Sirene + botao para desligar.
//  - sirene: GPIO 14 liga um MOSFET que alimenta a sirene (12 V)
//  - botao : GPIO 21 para o GND (pull-up interno; apertado = 0)
// Uma tarefa propria cuida do toque intermitente e le o botao, entao
// o botao responde na hora mesmo com o loop principal ocupado no modelo.
void sirene_init(void);       // chame primeiro no app_main: sirene desligada
void sirene_liga(void);       // queda: comeca a tocar
void sirene_desliga(void);    // para de tocar
bool sirene_tocando(void);
#endif
