/**
 * @file main.c
 * @brief Comunicación Bluetooth con HC-06
 * @details Este programa mide el nivel de ruido utilizando un micrófono analógico conectado a un Raspberry Pi Pico, guarda los datos en la memoria flash y permite la comunicación UART para la recuperación de datos.
 * @authors 
 *  - Juan Guillermo Quevedo
 *  - Luis Fernando Torres
 *  - Juan Manuel Correa
 */

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/rtc.h"
#include "hardware/adc.h"
#include "hc06.h"
#include <stdio.h>
#include <string.h>

#define ECG_PIN 26 // ADC0 pin (GPIO26)
#define LO_PLUS_PIN 27 // LO+ pin (GPIO27)
#define LO_MINUS_PIN 28 // LO- pin (GPIO28)

char datetime_buf[256];
char *datetime_str = &datetime_buf[0];

char message[24];

volatile bool ban_ecg = false;
volatile bool ban_clean = false;

/**
 * @brief Estructura de tiempo para actualizar el RTC
 */
datetime_t real_t = {
    .year = 2024,
    .month = 6,
    .day = 13,
    .dotw = 4, // 0 is Sunday, so 4 is Thursday
    .hour = 14,
    .min = 45,
    .sec = 0
};

/**
 * @brief Callback del temporizador repetitivo
 * @param t Puntero a la estructura del temporizador
 * @return true para mantener el temporizador en ejecución
 */
bool repeating_timer_callback(struct repeating_timer *t) {
    if (get_flash_st()==2){
        ban_clean = true;
        multicore_reset_core1();
    }
    return true; // Mantener el temporizador en ejecución
}

/**
 * @brief Callback del temporizador repetitivo para medir el ECG
 * @param t Puntero a la estructura del temporizador
 * @return true para mantener el temporizador en ejecución
 */
bool repeating_ecg_mesure(struct repeating_timer *t) {
    ban_ecg = true;
    return true; // Mantener el temporizador en ejecución
}

/**
 * @brief Inicializa el ADC
 */
void init_adc() {
    adc_init();
    adc_gpio_init(ECG_PIN);
    adc_select_input(0); // ADC input 0 corresponds to GPIO26
}

/**
 * @brief Inicializa los GPIOs
 */
void init_gpio() {
    gpio_init(LO_PLUS_PIN);
    gpio_set_dir(LO_PLUS_PIN, GPIO_IN);
    gpio_pull_up(LO_PLUS_PIN);
    
    gpio_init(LO_MINUS_PIN);
    gpio_set_dir(LO_MINUS_PIN, GPIO_IN);
    gpio_pull_up(LO_MINUS_PIN);
}

/**
 * @brief Realiza la medición del ECG
 */
void ecg_sense(){
    char buffer[26];
    bool lo_plus = !gpio_get(LO_PLUS_PIN); // Read LO+ pin status
    bool lo_minus = !gpio_get(LO_MINUS_PIN); // Read LO- pin status

    if (lo_plus || lo_minus) {
        uint16_t raw_value = adc_read(); // Read raw ADC value
        float voltage = raw_value * 3.3 / (1 << 12); // Convert to voltage (assuming 3.3V reference and 12-bit ADC)
        snprintf(buffer, sizeof(buffer), "\r$ec,%.3f,%d,%d,%d,%d\n", voltage, real_t.dotw, real_t.hour, real_t.min, real_t.sec);
        printf(buffer);
        write_to_flash(buffer,strlen(buffer));
    }
}

/**
 * @brief Función principal
 * @return 0 en caso de éxito
 */
int main() {
    stdio_init_all();
    sleep_ms(500);

    setup_hc06();

    // Inicialización del RTC con el tiempo actual
    rtc_init();
    rtc_set_datetime(&real_t);
    sleep_us(64);

    init_adc();       // Initialize ADC
    init_gpio();      // Initialize GPIO for lead off detection

    clean_data_flash();

    // Configurar la interrupción
    irq_set_exclusive_handler(UART1_IRQ, on_uart_rx);
    irq_set_enabled(UART1_IRQ, true);

    // Inicializar el temporizador repetitivo 
    struct repeating_timer timer;
    add_repeating_timer_ms(1000, repeating_timer_callback, NULL, &timer);

    // Inicializar el temporizador repetitivo para medir el ECG
    struct repeating_timer timer_1;
    add_repeating_timer_ms(200, repeating_ecg_mesure, NULL, &timer_1);

    while (1) {
        if (ban_clean){
            clean_data_flash();
            ban_clean = false;
        }
        if(ban_ecg){
            ecg_sense();
            ban_ecg = false;
        }
    }
    return 0;
}
