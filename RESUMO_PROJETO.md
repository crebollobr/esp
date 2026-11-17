# Detector de quedas com ESP32-S3 — resumo do projeto

Placa: ESP32-S3 Freenove, câmera OV3660, ESP-IDF v6.0.2.
Projeto no Windows: `C:\Users\crebo\teste10` (nome `meu_projeto`).

---

## 1) O que usamos no projeto (bibliotecas)

### Na placa (ESP-IDF)

| Biblioteca | Para que serve |
|---|---|
| `espressif/esp32-camera` (^2.1.0) | Driver da câmera. Foto em RGB565, 320x240 (QVGA). |
| `espressif/pedestrian_detect` (^0.3.2) | Modelo pronto que acha pessoas na imagem. Já traz o ESP-DL. |
| ESP-DL (vem junto) | Roda a rede neural; também decodifica JPEG (`dl_image_jpeg.hpp`). |
| `spiffs` | Sistema de arquivos na flash (guarda `imagens/teste.jpg`). |
| `esp_wifi`, `esp_netif`, `esp_event`, `nvs_flash` | Conectar no Wi-Fi. |
| `esp_http_server` | Servidor web: página com a foto e o modo teste. |
| `esp_timer` | Hora em milissegundos de cada quadro. |
| FreeRTOS | Tarefas e esperas (`vTaskDelay`). |

Configurações importantes (`sdkconfig.defaults`):
- CPU a 240 MHz, otimização de desempenho.
- Flash 8 MB com tabela própria (`partitions.csv`):
  app 4 MB e SPIFFS 2 MB.
- PSRAM octal a 80 MHz (o modelo precisa de memória).
- Cache recomendado pelo exemplo do `pedestrian_detect`.

As dependências ficam em `main/idf_component.yml`.
O IDF baixa sozinho na primeira compilação.

### Arquivos do nosso código (`main/`)

| Arquivo | O que faz |
|---|---|
| `main.cpp` | Laço principal: pega imagem, roda o modelo, chama `quedas.c`, desenha, loga. |
| `camera_ov2640.c` | Liga a câmera (RGB565, QVGA). |
| `image_source.cpp/.hpp` | Pega imagem da câmera, de arquivo ou de um JPEG na memória. |
| `quedas.c/.h` | **A lógica: quem é quem, postura, queda, levantou.** |
| `wifi.c/.h` | Conecta no Wi-Fi. |
| `web.c/.h` | Página `/` e `/foto.jpg`. |
| `teste.c/.h` | Modo teste: o PC manda quadros de vídeo pela rede. |

### No PC (só para teste e calibração)

| Ferramenta | Para que serve |
|---|---|
| Python + OpenCV (`cv2`) | Abrir os vídeos e salvar quadros em JPG. |
| Python + `ultralytics` (YOLO11n) | Detector "de referência" no PC, para comparar com a placa. |
| `gcc` | Compilar o `quedas.c` no PC e simular a placa. |
| `teste_videos.py` | Manda os quadros p/ a placa. Só usa a biblioteca padrão do Python. |

---

## 2) Como o vídeo virou imagem e como preparamos os testes

### Os vídeos

Base pública **URFD** (University of Rzeszow Fall Detection):
- 30 vídeos de queda: `fall-01` a `fall-30`
- 40 vídeos de atividades normais: `adl-01` a `adl-40`
  (andar, sentar, agachar, deitar na cama, pegar coisa no chão)
- Endereço: `https://fenix.ur.edu.pl/~mkepski/ds/data/fall-XX-cam0.mp4`
  (e `adl-XX-cam0.mp4`)

Cada vídeo tem 30 quadros por segundo. A imagem é dupla:
metade esquerda = câmera de profundidade, **metade direita = câmera
normal (RGB) de 320x240** — o mesmo tamanho da nossa câmera.

### Vídeo -> imagens

Script em Python com OpenCV:
1. Abre cada `.mp4` com `cv2.VideoCapture`.
2. Lê quadro por quadro.
3. Fica só com **1 a cada 10 quadros** (3 por segundo — perto da
   velocidade da placa, que faz ~1 a 3 quadros por segundo).
4. Corta a **metade direita** (`fr[:, 320:]`) = imagem RGB 320x240.
5. Salva em JPG (qualidade 90):
   `teste_videos/quadros/<video>/<numero do quadro>.jpg`

Resultado: 70 pastas, **1210 imagens**, ~18 MB
(`teste_videos.zip`).

### Modo teste na placa (`teste.c`)

A placa ganhou 4 endereços web:

| Endereço | O que faz |
|---|---|
| `GET /teste/inicio` | Começa um vídeo: zera as pessoas, para a câmera. |
| `POST /teste/quadro?t=MS` | Manda 1 JPEG. A placa roda o modelo + `quedas.c` e devolve as caixas. |
| `GET /teste/fim?t=MS` | Fim do vídeo. Responde `alarme;vmax`. |
| `GET /teste/sair` | Volta para a câmera. |

- O `t=MS` é a hora "falsa" do quadro (número do quadro / 30 fps),
  para a placa medir velocidades como se fosse ao vivo.
- No fim do vídeo a placa **repete o último quadro por 70 s**
  (para as regras que esperam 5 s ou 60 s no chão).
- Se o PC sumir por 30 s, a placa sai do modo teste sozinha.

### `teste_videos.py` (roda no Windows)

```
python teste_videos.py 192.168.0.225
```

Para cada vídeo: `inicio` -> todos os quadros -> `fim`.
Salva as caixas que a placa achou em `caixas_placa.csv`
e mostra a estatística no final.

---

## 3) Onde o código decide cada coisa

Tudo em `main/quedas.c`, função `quedas_update` (linha 122),
chamada uma vez por quadro. Os números são as linhas atuais.

### Antes: o modelo (main.cpp)

- `set_score_thr(0.3f)` (main.cpp:187): o modelo devolve caixas
  com certeza (score) a partir de 0.3.

### a) Quem é pessoa

| Linha | Regra |
|---|---|
| 130 | Caixa **em pé** (h/w > 1.5) só vale com score >= **0.7**. Gente em pé pontua ~0.9; ventilador, porta etc. pontuam menos. Deitada/sentada: 0.3 basta (o score cai quando a pessoa deita). |
| 133 | Caixa que encosta em **cima e embaixo** da imagem é ignorada (corpo cortado, porta, objeto na lente). |
| 152 | **Pessoa nova só nasce em pé.** Algo deitado que aparece do nada é suspeito (sofá, cadeira). |
| 165 | Só vira pessoa **confirmada** depois de **3 quadros seguidos**. |
| 209 | Não confirmada que falha 1 quadro é apagada (fantasma). |
| main.cpp:110 | A página só desenha pessoas confirmadas e vistas neste quadro. |

### b) Quem é quem (`casa`, linha 47)

Liga cada caixa nova a uma pessoa que já estava sendo seguida:
- Pega sempre o par (caixa, pessoa) **mais perto de todos**,
  depois o próximo, etc. (assim duas pessoas lado a lado não trocam).
- Só liga se o centro andou menos de **0.6 x o tamanho** da caixa
  (`DIST_MAX`). Com 1.0 a cadeira "roubava" a pessoa.
- Linha 68: quem estava **deitado** não pode virar uma caixa
  **em pé** no quadro seguinte (tem que passar por sentado).

### c) Está de pé? Está no chão? (forma da caixa, linhas 167-190)

A postura vem da **forma da caixa no quadro atual**: `r = h / w`.

| Linha | Forma | Postura |
|---|---|---|
| 178 | `r > 1.5` (alta e fina) | **em pé**: guarda a hora, a forma, onde estava a cabeça e a altura |
| 190 | `r < 0.7` (larga e baixa) | **no chão**: guarda a hora em que chegou ao chão |
| — | entre 0.7 e 1.5 | sentada / agachada (nada é marcado) |

### d) O histórico (passado)

| Linha | O que faz |
|---|---|
| 164 `guarda_hist` | Guarda as **3 últimas** posições: hora, topo (cabeça), altura, centro x. |
| 104 `velocidade` | Compara a mais velha com a mais nova: **velocidade vertical (vy) e horizontal (vx)** em "alturas da pessoa por segundo" (não depende da distância até a câmera). vy > 0 = descendo. |

### e) Caiu (4 regras)

| Regra | Onde | Usa histórico? | Como |
|---|---|---|---|
| **Tombo rápido** | 196 (mede), 226 (alarma) | não | Forma mudou de em pé p/ deitada rápido (> 1.0 por segundo) e ficou **5 s** no chão. |
| **Muito tempo no chão** | 227 | não | **60 s** no chão (mesmo caindo devagar). Só para quem já foi visto em pé. |
| **Desceu rápido e ficou embaixo** | 176 (marca), 218 (alarma) | **sim** | Cabeça descendo > **0.7 alturas/s** e já **40%** abaixo de onde estava em pé; continua vista embaixo por **5 s**. Pega a queda **na direção da câmera** (caixa fica quadrada e nunca parece deitada). |
| **Caiu e sumiu** | 245 | **sim** | Sumiu da imagem com a cabeça descendo > **0.6 alturas/s** e não voltou em **5 s** (caiu atrás de um móvel). Quem sai andando: ~0, é esquecido em 3 s (linha 250). |

### f) Levantou (linha 178-188)

Quando a caixa volta a ser **em pé** (`r > 1.5`):
- zera a queda (hora no chão, velocidade, "desceu");
- se já tinha dado alarme, avisa **"levantou"** (linha 188).

Também vale para quem "caiu e sumiu" e reaparece em pé.

---

## 4) Como testamos e os resultados

### Três jeitos de testar

1. **Testes sintéticos no PC** (`gcc`): caixas inventadas para cada
   situação (cair, sentar devagar, ventilador, porta, sair andando,
   cair e sumir, cair na direção da câmera). Toda mudança roda
   todos eles de novo.
2. **Simulação no PC com caixas reais**: o `quedas.c` igual ao da
   placa lê um arquivo de caixas (`caixas_placa.csv` ou as do YOLO)
   e diz, vídeo por vídeo, se deu alarme. Varremos os parâmetros
   (RAZAO_CAIDO, velocidades, distância) para escolher os melhores.
3. **Na placa**, com `teste_videos.py` (modelo de verdade).

A simulação com `caixas_placa.csv` dá **exatamente** o mesmo
resultado da placa. Então dá para testar mudanças no PC antes de gravar.

### Resultados

"Quedas" = de 30 vídeos de queda. "Falsos" = de 40 vídeos normais.

| Versão | Quedas | Falsos |
|---|---|---|
| PC, YOLO, regras originais (limiar 0.6, vel 1.5) | 12/30 | 2/40 |
| PC, YOLO, limiar 0.3 + score por postura | 19/30 | — |
| PC, YOLO, velocidade 1.0 | 20/30 | 4/40 |
| **Placa**, RAZAO_CAIDO 0.8 | 26/30 | 13/40 |
| **Placa**, RAZAO_CAIDO 0.7 (rodado de verdade) | 26/30 | 5/40 |
| Simulação placa, 0.7 + "desceu rápido" + DIST 0.6 | **30/30** | 5/40 |

Os números acima repetem o último quadro por **70 s**, como o script.

**Atenção aos falsos:** adl-31, 32, 34, 38 terminam com a pessoa
**agachada**; repetir esse quadro 70 s é como ela ficar 70 s no chão.
Na vida real ela levanta. Repetindo só **6 s**:

| Versão | Quedas | Falsos |
|---|---|---|
| RAZAO_CAIDO 0.8 | 25/30 | 6/40 |
| RAZAO_CAIDO 0.7 | 23/30 | 1/40 |
| RAZAO_CAIDO 0.65 | 21/30 | 0/40 |
| **Atual** (0.7 + "desceu rápido" + DIST 0.6) | **27/30** | **1/40** |

- O único falso de verdade: **adl-37** (pessoa se joga na cama).
- Quedas ainda perdidas: fall-08, fall-10, fall-16.
- Velocidade na placa: **0,64 s por quadro**.

### Ideias testadas e descartadas

| Ideia | Por que saiu |
|---|---|
| Altura < metade da altura em pé | Mais falsos (sentar no sofá). |
| "Objeto que nunca anda" não é pessoa | Caiu de 20 para 7 quedas. |
| Prever a posição futura na `casa` | O ganho era sorte (pessoa trocada pela cadeira). |
| Desconfiar de deitada de score baixo na `casa` se não vinha descendo | Só perdeu quedas (23 -> 20). |
| VEL_SUMIU 0.35 | Nenhuma queda a mais e 3 falsos a mais. Ficou 0.6. |

### O que falta

1. Testar **na sua sala**, com a câmera a 1,5-2 m, inclinada para
   baixo, mostrando o chão e sem a planta na frente da lente.
2. Cama e sofá (talvez marcar uma área para ignorar).
3. A regra dos 60 s não dá para avaliar com vídeos curtos.
