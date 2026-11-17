#ifndef CAMERA_SOURCE_H
#define CAMERA_SOURCE_H
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *dados;   // pixels RGB565 big-endian (como o esp32-camera entrega)
    size_t   tam;
    int      largura, altura;
    void    *priv;    // fb original do driver (p/ devolver)
} frame_t;

int      camera_init(void);
frame_t *camera_get_frame(void);           // NULL se falhar
void     camera_release_frame(frame_t *f); // devolver O QUANTO ANTES
#endif
