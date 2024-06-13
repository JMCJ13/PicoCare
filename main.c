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
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/rtc.h"
#include "hardware/adc.h"
#include "hc06.h"
#include <stdio.h>
#include <string.h>

//pines ECG
#define ECG_PIN 26 // ADC0 pin (GPIO26)
#define LO_PLUS_PIN 27 // LO+ pin (GPIO27)
#define LO_MINUS_PIN 28 // LO- pin (GPIO28)

//pines del I2C para el sensor de oxigeno
#define I2C_PORT i2c1   
#define MAX30102_ADDR 0x57
#define I2C_SDA 2
#define I2C_SCL 3

//buffers de tiempo
char datetime_buf[256];
char *datetime_str = &datetime_buf[0];

char message[24];

//banderas de interrupciones
volatile bool ban_ecg = false;
volatile bool ban_spo = false;
volatile bool ban_clean = false;

/**
 * @brief Estructura de tiempo para actualizar el RTC
 */
datetime_t real_t = {
    .year = 2024,
    .month = 6,
    .day = 13,
    .dotw = 4, // 0 is Sunday, so 4 is Thursday
    .hour = 15,
    .min = 53,
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
    ban_ecg = true;
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
 * @brief Inicializa el i2c para el medidor de oxigeno
 */
void my_i2c_init() {
    i2c_init(I2C_PORT, 100 * 1000);  // 100 kHz
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

/**
 * @brief Inicializa el i2c para el medidor de oxigeno
 */
void max30102_init() {
    uint8_t buffer[2];

    // Reset the sensor
    buffer[0] = 0x09;  // MODE_CONFIG register
    buffer[1] = 0x40;  // Reset command
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);

    sleep_ms(1000);  // Wait for reset to complete

    // Set LED pulse amplitude
    buffer[0] = 0x0C;  // LED1_PA register
    buffer[1] = 0x24;  // LED1 pulse amplitude
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);

    buffer[0] = 0x0D;  // LED2_PA register
    buffer[1] = 0x24;  // LED2 pulse amplitude
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);

    // Set mode configuration to SpO2 mode
    buffer[0] = 0x09;  // MODE_CONFIG register
    buffer[1] = 0x03;  // SpO2 mode
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);

    // Set SpO2 configuration
    buffer[0] = 0x0A;  // SPO2_CONFIG register
    buffer[1] = 0x27;  // SPO2_ADC range, sample rate, pulse width
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);

    // Set FIFO configuration
    buffer[0] = 0x08;  // FIFO_CONFIG register
    buffer[1] = 0x4F;  // Sample average = 4, FIFO rollover enabled, FIFO almost full = 17
    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, buffer, 2, false);
}

/**
 * @brief Lectura de los registros del modulo para obtener los datos de la oxigenación en sangre
 * @param red_led Puntero a la variable de la intensidad medida del receptor rojo.
 * @param ir_led Puntero a la variable de la intensidad medida del receptor infrarojo.
 */
void max30102_read_fifo(uint32_t *red_led, uint32_t *ir_led) {
    uint8_t buffer[6];
    uint8_t reg = 0x07;  // FIFO_DATA register

    i2c_write_blocking(I2C_PORT, MAX30102_ADDR, &reg, 1, true);  // Repeated start
    i2c_read_blocking(I2C_PORT, MAX30102_ADDR, buffer, 6, false);

    *red_led = ((buffer[0] << 16) | (buffer[1] << 8) | buffer[2]) & 0x03FFFF;
    *ir_led = ((buffer[3] << 16) | (buffer[4] << 8) | buffer[5]) & 0x03FFFF;
}
/**
 * @brief Inicializa el i2c para el medidor de oxigeno
 * @param red_led Puntero a la variable de la intensidad medida del receptor rojo.
 * @param ir_led Puntero a la variable de la intensidad medida del receptor infrarojo.
 * @return variable flotante con el porcentaje de oxigenación en sangre
 */
float calculate_spo2(uint32_t red_led, uint32_t ir_led) {
    // Constantes empíricas ajustadas según datos de calibración
    const float A = 110.0;
    const float B = 25.0;

    // Suponiendo que red_led e ir_led contienen señales ya filtradas para obtener componentes DC y AC
    // Extracción simplificada de componentes AC y DC
    float dc_red = (float)(red_led >> 10);  // Component DC (parte alta de la señal)
    float ac_red = (float)(red_led & 0x3FF); // Component AC (parte baja de la señal)
    float dc_ir = (float)(ir_led >> 10);  // Component DC (parte alta de la señal)
    float ac_ir = (float)(ir_led & 0x3FF); // Component AC (parte baja de la señal)

    // Asegurarse de no dividir por cero
    if (dc_red == 0 || dc_ir == 0) {
        return 0.0; // O manejar el error según sea necesario
    }

    // Calcular la relación de ratios
    float ratio = (ac_red / dc_red) / (ac_ir / dc_ir);

    // Calcular SpO2 usando la fórmula empírica
    float spo2 = A - (B * ratio);

    // Limitar el rango de SpO2
    if (spo2 > 100.0) spo2 = 100.0;
    if (spo2 < 0.0) spo2 = 0.0;

    return spo2;
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
 * @brief Realiza la medición de la oxigenación en sangre
 */
void ecg_spo(){
    char buffer[26];
    uint32_t red_led, ir_led;
    max30102_read_fifo(&red_led, &ir_led);

    float spo2 = calculate_spo2(red_led, ir_led);
    if (spo2 > 100 || spo2 < 0){
        printf("Erorr en la medida de saturacion de oxigeno\n");
    }
    else {
        snprintf(buffer, sizeof(buffer), "\r$ox,%.2f,%d,%d,%d,%d\n", spo2, real_t.dotw, real_t.hour, real_t.min, real_t.sec);
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

    // inicialización del sensor de oxigeno
    my_i2c_init();
    max30102_init();

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
        if(ban_spo){
            ecg_spo();
            ban_spo = false;
        }
        __wfi();
    }
    return 0;
}