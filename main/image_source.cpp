// Fontes de imagem p/ o ESP-DL. A camera vem pela interface camera_source.h;
// o arquivo vem por fopen, entao o SPIFFS precisa estar montado antes.
#include "image_source.hpp"
#include "dl_image_jpeg.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <cstdio>
#include <cstring>

extern "C" {
#include "camera_source.h"
}

static const char *TAG = "image";

dl::image::img_t get_image_cam(void)
{
    dl::image::img_t img = {};

    frame_t *f = camera_get_frame();
    if (!f) {
        ESP_LOGE(TAG, "camera nao entregou frame");
        return img;
    }

    size_t bytes = (size_t)f->largura * f->altura * 2;
    if (f->tam < bytes) {
        ESP_LOGE(TAG, "frame nao e' RGB565 %dx%d (%u bytes)",
                 f->largura, f->altura, (unsigned)f->tam);
        camera_release_frame(f);
        return img;
    }

    void *copia = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
    if (!copia) {
        ESP_LOGE(TAG, "sem memoria p/ copiar o frame");
        camera_release_frame(f);
        return img;
    }
    memcpy(copia, f->dados, bytes);
    camera_release_frame(f);   // ja temos a copia: devolve o buffer JA

    img.data     = copia;
    img.width    = (uint16_t)f->largura;
    img.height   = (uint16_t)f->altura;
    // o esp32-camera entrega RGB565 big-endian; o ESP-DL aceita direto
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565BE;
    return img;
}

dl::image::img_t get_image_file(const char *path)
{
    dl::image::img_t img = {};

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG, "nao abriu %s", path);
        return img;
    }
    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0) {
        ESP_LOGE(TAG, "%s vazio", path);
        fclose(fp);
        return img;
    }

    uint8_t *buf = (uint8_t *)heap_caps_malloc(len, MALLOC_CAP_SPIRAM);
    if (!buf) {
        ESP_LOGE(TAG, "sem memoria p/ %ld bytes", len);
        fclose(fp);
        return img;
    }
    size_t lidos = fread(buf, 1, len, fp);
    fclose(fp);

    if (lidos == (size_t)len) {
        img = get_image_jpeg(buf, (size_t)len);
        if (!img.data)
            ESP_LOGE(TAG, "%s nao e' um JPEG valido", path);
    } else {
        ESP_LOGE(TAG, "leitura incompleta de %s", path);
    }
    heap_caps_free(buf);
    return img;
}

dl::image::img_t get_image_jpeg(const uint8_t *dados, size_t tam)
{
    dl::image::jpeg_img_t jpeg = {};
    jpeg.data     = (void *)dados;
    jpeg.data_len = tam;
    return dl::image::sw_decode_jpeg(jpeg, dl::image::DL_IMAGE_PIX_TYPE_RGB888);
}

void free_image(dl::image::img_t *img)
{
    if (img && img->data) {
        heap_caps_free(img->data);
        img->data = nullptr;
    }
}
