# Detector de quedas com ESP32-S3

Câmera + ESP32-S3 que detecta quedas sozinha (rede neural na própria placa)
e toca uma sirene, desligada por um botão.

- **Relatório completo:** [RELATORIO.md](RELATORIO.md)
- **Circuito da sirene e do botão:** [docs/circuito_sirene.svg](docs/circuito_sirene.svg)
- **Resumo técnico (parâmetros e linhas do código):** [RESUMO_PROJETO.md](RESUMO_PROJETO.md)

Resultado nos vídeos da base URFD: 27 de 30 quedas detectadas, 1 alarme falso em 40 vídeos normais.

## Placa

ESP32-S3 Freenove WROOM CAM (flash 8 MB, PSRAM 8 MB), câmera OV3660, ESP-IDF 5.3 ou mais novo.

| Pino | Uso |
| --- | --- |
| GPIO 14 | Sirene (via MOSFET) |
| GPIO 21 | Botão para desligar a sirene (até o GND) |

## Compilar e gravar

1. Coloque o nome e a senha do Wi-Fi em `main/main.cpp` (`WIFI_REDE`, `WIFI_SENHA`).
2. Rode:

```
idf.py set-target esp32s3
idf.py build flash monitor
```

As bibliotecas (`esp32-camera`, `pedestrian_detect`) são baixadas sozinhas na primeira compilação.

## Testar com vídeos

Com a placa ligada, `python teste_videos/teste_videos.py <IP da placa>` manda os quadros dos
vídeos para a placa e mostra a estatística. Os quadros (`teste_videos/quadros/`) não estão no
repositório: são tirados dos vídeos da [base URFD](http://fenix.ur.edu.pl/~mkepski/ds/uf.html)
(1 quadro a cada 10, metade direita da imagem, 320x240).
