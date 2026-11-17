#ifndef TESTE_H
#define TESTE_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_http_server.h"

// Modo teste: o PC manda quadros de video (JPEG) pela rede e a placa
// roda o modelo + quedas.c neles, como se viessem da camera.
// O servidor so recebe e repassa; quem roda e' o loop principal (dono
// do modelo e do quedas.c), igual ao esquema da foto em web.c.
//   GET  /teste/inicio        zera o quedas.c (video novo)
//   POST /teste/quadro?t=MS   corpo = JPEG; responde as caixas vistas
//   GET  /teste/fim?t=MS      fim do video; responde "alarme;vmax"
//   GET  /teste/sair          volta p/ a camera
enum { TESTE_NADA, TESTE_INICIO, TESTE_QUADRO, TESTE_FIM, TESTE_SAIR };

void teste_registra(httpd_handle_t srv);      // chamado por web_inicia

// p/ o loop principal
bool teste_ativo(void);                        // camera fica parada
int  teste_pedido(const uint8_t **jpg, size_t *tam, uint32_t *t_ms);
void teste_responde(const char *texto);        // libera o navegador/PC
#endif
