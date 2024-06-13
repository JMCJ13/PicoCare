/**
 * @file ecg.c
 * @brief ECG signal acquisition using Raspberry Pi Pico
 * 
 * This file contains the implementation for reading ECG signals using the Raspberry Pi Pico's ADC
 * and detecting lead-off conditions using GPIO.
 * 
 * @author
 * @date
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"

/// GPIO pin for the ECG signal input (ADC0)
#define ECG_PIN 26

/// GPIO pin for LO+ (Lead Off Positive) detection
#define LO_PLUS_PIN 27

/// GPIO pin for LO- (Lead Off Negative) detection
#define LO_MINUS_PIN 28

/**
 * @brief Initialize the ADC hardware
 * 
 * This function initializes the ADC hardware and sets up the GPIO pin for the ECG signal.
 */
void init_adc() {
    adc_init();                     ///< Initialize the ADC hardware
    adc_gpio_init(ECG_PIN);         ///< Initialize the GPIO pin for ADC
    adc_select_input(0);            ///< Select ADC input 0 which corresponds to GPIO26
}

/**
 * @brief Initialize the GPIO hardware
 * 
 * This function initializes the GPIO pins for LO+ and LO- lead-off detection.
 */
void init_gpio() {
    gpio_init(LO_PLUS_PIN);         ///< Initialize the LO+ GPIO pin
    gpio_set_dir(LO_PLUS_PIN, GPIO_IN); ///< Set the LO+ pin as input
    gpio_pull_up(LO_PLUS_PIN);      ///< Enable pull-up resistor on the LO+ pin

    gpio_init(LO_MINUS_PIN);        ///< Initialize the LO- GPIO pin
    gpio_set_dir(LO_MINUS_PIN, GPIO_IN); ///< Set the LO- pin as input
    gpio_pull_up(LO_MINUS_PIN);     ///< Enable pull-up resistor on the LO- pin
}

/**
 * @brief Main function
 * 
 * This is the main function of the program. It initializes the standard I/O, ADC, and GPIO,
 * then enters an infinite loop to continuously read the ECG signal and check for lead-off conditions.
 * 
 * @return int Exit status
 */
int main() {
    stdio_init_all();               ///< Initialize all standard I/O
    init_adc();                     ///< Initialize ADC
    init_gpio();                    ///< Initialize GPIO for lead-off detection

    while (1) {
        bool lo_plus = gpio_get(LO_PLUS_PIN);   ///< Read the status of the LO+ pin
        bool lo_minus = gpio_get(LO_MINUS_PIN); ///< Read the status of the LO- pin

        if (lo_plus || lo_minus) {
            printf("Lead Off Detected: ");
            if (lo_plus) {
                printf("LO+ ");
            }
            if (lo_minus) {
                printf("LO- ");
            }
            printf("\n");
        } else {
            uint16_t raw_value = adc_read();  ///< Read the raw ADC value
            float voltage = raw_value * 3.3 / (1 << 12); ///< Convert raw value to voltage (assuming 3.3V reference and 12-bit ADC)
            printf("Raw ADC Value: %d, Voltage: %.2f V\n", raw_value, voltage);
        }
        sleep_ms(1000);               ///< Wait for 1 second
    }

    return 0;
}
