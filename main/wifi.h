#ifndef WIFI_H
#define WIFI_H
#include <stdbool.h>

// Conecta o ESP32 na rede Wi-Fi (modo cliente) e espera receber um IP.
// Devolve true se conectou dentro de timeout_ms. Se a rede cair depois,
// reconecta sozinho.
bool wifi_conecta(const char *rede, const char *senha, int timeout_ms);
// Forca do sinal do roteador em dBm (-50 otimo, -67 bom, -80 ruim).
// Devolve 0 se nao estiver conectado.
int wifi_rssi(void);
#endif
