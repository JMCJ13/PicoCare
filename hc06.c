/**
 * @file hc06.c
 * @brief Implementación de las funciones para el manejo del modulo bluetooth HC-06
 * 
 * Este archivo contiene las declaraciones de funciones y las definiciones necesarias para
 * inicializar, enviar los datos por UART al modulo HC-06
 */

#include "hc06.h"

uint8_t ind_flash = 0;

uint32_t maxsize = 100*(4*FLASH_PAGE_SIZE); //el tamaño máximo de FLASH que se va a usar son 100kB

/**
 * @brief Configura la UART para la comunicación con el GPS.
 * 
 * Inicializa la UART con los parámetros definidos para la comunicación con el módulo HC-06.
 */
void setup_hc06() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_hw_flow(UART_ID, false, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    // Habilitar las interrupciones para UART
    uart_set_irq_enables(UART_ID, true, false);
}

/**
 * @brief Handler de interrupción para UART
 * 
 * 
 */
void on_uart_rx() {
    multicore_launch_core1(read_data_flash);
}

/**
 * @brief Envia los datos por UART al modulo HC-06.
 * 
 * @param nmea_buffer Puntero a la cadena con el mensaje a enviar.
 */
void enviar_datos(char *message){
    uart_puts(UART_ID, message);
}

/**
 * @brief Lee datos de la memoria flash.
 */
void read_data_flash() {
    //se indica que se esta leyendo la flash
    ind_flash = 0;

    char *data_flash = (char *)(XIP_BASE + FLASH_TARGET_OFFSET);
    char *token = strtok(data_flash, "\n");
    uint16_t token_count = 0;
    while (token != NULL){
        enviar_datos(token);
        token = strtok(NULL, ",");
        token_count++;
    }

    //se indica que la flash a sido leida y debe ser borrada
    ind_flash = 2;
}
    
/**
 * @brief Borra los datos de la flash que ya fueron enviados por el modulo HC-06
 */
void clean_data_flash(){
    // Desactivar interrupciones mientras se escribe en la flash
    uint32_t ints = save_and_disable_interrupts();
    multicore_reset_core1();
    // Borrar el sector de destino antes de escribir
    flash_range_erase(FLASH_TARGET_OFFSET, maxsize);
    // Restaurar interrupciones
    restore_interrupts(ints);
    //se indica que se pueden seguir guardando datos en la flash
    ind_flash = 0;
}

/**
 * @brief Escribe datos en la memoria flash.
 * 
 * @param data Puntero a los datos a escribir.
 * @param length Longitud de los datos.
 */
void write_to_flash(const char *data, size_t length) {
    char buffer[maxsize];
    uint16_t cont = 0;

    // Asegurarse de que la longitud de los datos sea múltiplo del tamaño de página de flash
    uint8_t padded_data[maxsize];
    memset(padded_data, 0xFF, maxsize);

    const char *datar = (const char *)(XIP_BASE + FLASH_TARGET_OFFSET);

    uint32_t flash_write_offset = strlen(datar) + length + 2;

    if (flash_write_offset < maxsize){
        snprintf(buffer, flash_write_offset, "%s%s", datar, data);
        memcpy(padded_data, buffer, flash_write_offset);
        // Desactivar interrupciones mientras se escribe en la flash
        uint32_t ints = save_and_disable_interrupts();
        multicore_reset_core1();
        // Borrar el sector de destino antes de escribir
        flash_range_erase(FLASH_TARGET_OFFSET, maxsize);
        // Escribir los datos a la memoria flash
        flash_range_program(FLASH_TARGET_OFFSET, padded_data, maxsize);
        // Restaurar interrupciones
        restore_interrupts(ints);
    }
}

/**
 * @brief  Retorna la bandera del status de la flahs
 * @return 0 no se la leido la flash, 1 la flash esta en proceso de lectura, 2 la flash ya fue leida.
 */
uint8_t get_flash_st(){
    return ind_flash;
}