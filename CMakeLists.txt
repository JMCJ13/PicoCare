cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD "pico")

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(Bluetooth)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(ejecutable
	main.c
	hc06.c
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(ejecutable 
	pico_stdlib
	pico_flash
	pico_util
	pico_sync
	pico_multicore
	hardware_dma
	hardware_irq
	hardware_timer
	hardware_uart
	hardware_gpio
	hardware_flash
	hardware_rtc
	hardware_exception
	hardware_adc
)

pico_enable_stdio_usb(ejecutable 1)
pico_enable_stdio_uart(ejecutable 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(ejecutable)