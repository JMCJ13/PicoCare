/**
 * @file hc06.h
 * @brief Declaraciones de funciones y definiciones para el manejo del modulo bluetooth HC-06 y la flash.
 * 
 * Este archivo contiene las declaraciones de funciones y las definiciones necesarias para
 * inicializar, enviar los datos por UART al modulo HC-06
 */

#ifndef hc06_H
#define hc06_H

#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "hardware/irq.h"
#include "hardware/flash.h"
#include "hardware/dma.h"
#include "hardware/uart.h"
#include "hardware/irq.h"
#include "hardware/rtc.h"
#include <stdio.h>
#include <string.h>

// Define la UART y sus parámetros
#define UART_ID uart1
#define BAUD_RATE 38400
#define UART_TX_PIN 4
#define UART_RX_PIN 5

#define FLASH_TARGET_OFFSET 1024*1024 ///< Offset de 1MB para no sobrescribir el programa en la flash

/**
 * @brief Configura la UART para la comunicación con modulo HC-06.
 * 
 * Inicializa la UART con los parámetros definidos para la comunicación con el módulo HC-06
 */
void setup_hc06();

/**
 * @brief Handler de interrupción para UART
 * 
 * 
 */
void on_uart_rx();

/**
 * @brief Envia los datos por UART al modulo HC-06.
 * 
 * @param nmea_buffer Puntero a la cadena con el mensaje a enviar.
 */
void enviar_datos(char *message);

/**
 * @brief Lee datos de la memoria flash y los envia al modulo HC-06.
 */
void read_data_flash();

/**
 * @brief Borra los datos de la flash que ya fueron enviados por el modulo HC-06
 */
void clean_data_flash();

/**
 * @brief Escribe datos en la memoria flash.
 * 
 * @param data Puntero a los datos a escribir.
 * @param length Longitud de los datos.
 */
void write_to_flash(const char *data, size_t length);

/**
 * @brief  
 * @return true si el GPS está listo, false en caso contrario.
 */
uint8_t get_flash_st();

#endif // HC06_H