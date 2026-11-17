#ifndef IMAGE_SOURCE_HPP
#define IMAGE_SOURCE_HPP
#include "dl_image.hpp"

// Fontes de imagem no formato do ESP-DL (dl::image::img_t):
//  - get_image_cam : um frame da camera, em RGB565 big-endian
//  - get_image_file: um JPEG do SPIFFS (/spiffs/...), em RGB888
//  - get_image_jpeg: um JPEG que ja esta na memoria, em RGB888
// Em erro, img.data == nullptr. Toda imagem valida e' uma COPIA:
// libere com free_image() quando terminar.
dl::image::img_t get_image_cam(void);
dl::image::img_t get_image_file(const char *path);
dl::image::img_t get_image_jpeg(const uint8_t *dados, size_t tam);
void             free_image(dl::image::img_t *img);
#endif
