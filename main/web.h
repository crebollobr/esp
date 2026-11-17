#ifndef WEB_H
#define WEB_H
#include <stdint.h>
#include <stdbool.h>

// Pagina p/ ver a camera no navegador: http://<IP do ESP32>/
// O servidor nao mexe na camera: ele PEDE uma foto e o loop principal
// entrega o proximo frame (assim os dois nao disputam a camera).
bool web_inicia(void);                    // sobe o servidor (porta 80)
bool web_quer_foto(void);                 // o navegador esta esperando foto?
void web_entrega_foto(const uint8_t *rgb565, int largura, int altura);
#endif
