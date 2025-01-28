#ifndef __NEOPIXEL_INC
#define __NEOPIXEL_INC

#include <stdlib.h>
#include "ws2818b.pio.h"

// Estrutura que define um pixel no formato GRB.
// A ordem GRB é utilizada porque alguns LEDs endereçáveis exigem essa ordem para exibição correta das cores.
struct pixel_t {
  uint8_t G, R, B;
};
typedef struct pixel_t pixel_t;
typedef pixel_t npLED_t; // Renomeação para maior clareza ao trabalhar com LEDs.

// Buffer que armazena o estado dos LEDs na matriz.
static npLED_t *leds;
static uint led_count;

// Variáveis para configurar e utilizar a máquina de estado programável (PIO).
static PIO np_pio;
static uint np_sm;

// Inicializa a máquina PIO e aloca memória para o buffer de LEDs.
void npInit(uint pin, uint amount) {
  led_count = amount;
  leds = (npLED_t *)calloc(led_count, sizeof(npLED_t)); // Aloca memória para armazenar o estado de cada LED.

  // Adiciona o programa PIO à primeira máquina disponível.
  uint offset = pio_add_program(pio0, &ws2818b_program);
  np_pio = pio0;

  // Obtém uma máquina de estado PIO disponível.
  np_sm = pio_claim_unused_sm(np_pio, false);
  if (np_sm < 0) { // Se nenhuma máquina estiver livre, tenta a segunda PIO.
    np_pio = pio1;
    np_sm = pio_claim_unused_sm(np_pio, true);
  }

  // Inicializa o programa PIO com o pino e frequência corretos.
  ws2818b_program_init(np_pio, np_sm, offset, pin, 800000.f);

  // Garante que todos os LEDs comecem apagados.
  for (uint i = 0; i < led_count; ++i) {
    leds[i].R = 0;
    leds[i].G = 0;
    leds[i].B = 0;
  }
}

// Define a cor de um LED específico no buffer.
void npSetLED(const uint index, const uint8_t r, const uint8_t g, const uint8_t b) {
  leds[index].R = r;
  leds[index].G = g;
  leds[index].B = b;
}

// Apaga todos os LEDs da matriz, alterando o buffer de estado.
void npClear() {
  for (uint i = 0; i < led_count; ++i)
    npSetLED(i, 0, 0, 0);
}

// Envia os dados armazenados no buffer para os LEDs físicos.
void npWrite() {
  for (uint i = 0; i < led_count; ++i) {
    pio_sm_put_blocking(np_pio, np_sm, leds[i].G); // Primeiro envia o componente verde.
    pio_sm_put_blocking(np_pio, np_sm, leds[i].R); // Depois o vermelho.
    pio_sm_put_blocking(np_pio, np_sm, leds[i].B); // Por último, o azul.
  }
  sleep_us(100); // Tempo de reset necessário para garantir que os LEDs processem os novos valores.
}

// Define a mesma cor para todos os LEDs da matriz.
void npSetAll(const uint8_t r, const uint8_t g, const uint8_t b) {
    for (uint i = 0; i < led_count; ++i) {
        npSetLED(i, r, g, b);
    }
}

#endif
