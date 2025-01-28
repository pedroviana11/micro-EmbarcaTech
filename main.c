// Inclusão das bibliotecas necessárias para o funcionamento do código
#include <stdio.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/dma.h"
#include "neopixel.c"
#include "ssd1306.h"

// Definição dos pinos para comunicação I2C do display OLED
#define I2C_SDA 14
#define I2C_SCL 15

// Definição do canal e pino do microfone no ADC
#define MIC_CHANNEL 2
#define MIC_PIN (26 + MIC_CHANNEL)

// Configuração do ADC
#define ADC_CLOCK_DIV 96.f           // Fator de divisão do clock do ADC
#define SAMPLES 200                  // Número de amostras a serem capturadas
#define ADC_ADJUST(x) (x * 3.3f / (1 << 12u) - 1.65f)  // Ajuste do ADC para tensão
#define ADC_MAX 3.3f                  // Tensão máxima do ADC
#define ADC_STEP (3.3f / 5.f)         // Intervalos para classificação do nível de ruído

// Definição da matriz de LEDs
#define LED_PIN 7
#define LED_COUNT 25

// Função para calcular o valor absoluto
#define abs(x) ((x < 0) ? (-x) : (x))

// Variáveis globais para DMA, intensidade máxima e tempo de exposição
uint dma_channel;
dma_channel_config dma_cfg;
float max_db = 0;       // Valor máximo registrado de dB
uint exposure_time = 0; // Tempo total exposto acima de 80 dB
uint16_t adc_buffer[SAMPLES]; // Buffer para armazenar as amostras do ADC

// Protótipos das funções
void sample_mic();
float mic_power();
uint8_t get_intensity(float v);
float calculate_db(float avg_voltage);

int main()
{
    // Inicializa a comunicação serial e o barramento I2C para o display OLED
    stdio_init_all();
    i2c_init(i2c1, 400000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    // Configuração do display OLED
    ssd1306_t disp;
    disp.external_vcc = false;
    ssd1306_init(&disp, 128, 64, 0x3C, i2c1);
    ssd1306_clear(&disp);

    // Delay para garantir inicialização correta do sistema
    sleep_ms(5000);

    // Configuração da matriz de LEDs
    npInit(LED_PIN, LED_COUNT);

    // Configuração do ADC para capturar dados do microfone
    adc_gpio_init(MIC_PIN);
    adc_init();
    adc_select_input(MIC_CHANNEL);
    adc_fifo_setup(true, true, 1, false, false);
    adc_set_clkdiv(ADC_CLOCK_DIV);

    // Configuração do DMA para transferência eficiente dos dados do ADC
    dma_channel = dma_claim_unused_channel(true);
    dma_cfg = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&dma_cfg, DMA_SIZE_16);
    channel_config_set_read_increment(&dma_cfg, false);
    channel_config_set_write_increment(&dma_cfg, true);
    channel_config_set_dreq(&dma_cfg, DREQ_ADC);

    // Loop principal para monitoramento contínuo do ruído
    while (true)
    {
        // Captura de amostras do microfone
        sample_mic();

        // Cálculo do nível de dB com base nas amostras coletadas
        float avg_voltage = mic_power();
        avg_voltage = 2.f * abs(ADC_ADJUST(avg_voltage));
        float db = calculate_db(avg_voltage);

        // Atualiza o nível máximo de dB registrado
        if (db > max_db)
            max_db = db;

        // Se o nível ultrapassar 80 dB, incrementa o tempo de exposição
        if (db > 80)
            exposure_time += 200;

        // Atualização da matriz de LEDs conforme a intensidade do ruído
        npClear();
        npSetLED(11, 0, 50, 0);
        npSetLED(12, 0, 50, 0);
        npSetLED(13, 0, 50, 0);
        npSetLED(7, 0, 50, 0);
        npSetLED(17, 0, 50, 0);

        if (db >= 65)
        {
            // Acende LEDs amarelos entre 65 e 75 dB
            npSetLED(2, 50, 50, 0);
            npSetLED(6, 50, 50, 0);
            npSetLED(8, 50, 50, 0);
            npSetLED(10, 50, 50, 0);
            npSetLED(14, 50, 50, 0);
            npSetLED(16, 50, 50, 0);
            npSetLED(18, 50, 50, 0);
            npSetLED(22, 50, 50, 0);
        }

        if (db > 75)
        {
            // Acende LEDs vermelhos entre 75 e 85 dB
            npSetLED(0, 50, 0, 0);
            npSetLED(1, 50, 0, 0);
            npSetLED(3, 50, 0, 0);
            npSetLED(4, 50, 0, 0);
            npSetLED(5, 50, 0, 0);
            npSetLED(9, 50, 0, 0);
            npSetLED(15, 50, 0, 0);
            npSetLED(19, 50, 0, 0);
            npSetLED(20, 50, 0, 0);
            npSetLED(21, 50, 0, 0);
            npSetLED(23, 50, 0, 0);
            npSetLED(24, 50, 0, 0);
        }

        if (db > 85)
        {
            // Acende todos os LEDs vermelhos acima de 85 dB
            npSetAll(50, 0, 0);
        }

        npWrite();

        // Atualiza o display OLED com os dados coletados
        char display_text[50];
        ssd1306_clear(&disp);
        sprintf(display_text, "Volume: %.2f dB", db);
        ssd1306_draw_string(&disp, 5, 0, 1, display_text);
        sprintf(display_text, "Max: %.2f dB", max_db);
        ssd1306_draw_string(&disp, 5, 16, 1, display_text);
        sprintf(display_text, ">80dB: %ds", exposure_time / 1000);
        ssd1306_draw_string(&disp, 5, 32, 1, display_text);
        ssd1306_show(&disp);

        // Exibe os dados no terminal
        printf("dB: %.2f, Max: %.2f, Exp: %ds\r\n", db, max_db, exposure_time / 1000);

        // Aguarda um pequeno intervalo antes da próxima leitura
        sleep_ms(200);
    }
}

// Função para capturar amostras do ADC usando DMA
void sample_mic()
{
    adc_fifo_drain();
    adc_run(false);
    dma_channel_configure(dma_channel, &dma_cfg, adc_buffer, &(adc_hw->fifo), SAMPLES, true);
    adc_run(true);
    dma_channel_wait_for_finish_blocking(dma_channel);
    adc_run(false);
}

// Função para calcular a potência média do sinal do microfone
float mic_power()
{
    float avg = 0.f;
    for (uint i = 0; i < SAMPLES; ++i)
        avg += adc_buffer[i] * adc_buffer[i];
    avg /= SAMPLES;
    return sqrt(avg);
}

// Função para calcular o nível de dB com base na tensão média
float calculate_db(float avg_voltage)
{
    return 20 * log10(avg_voltage / 0.0001);
}