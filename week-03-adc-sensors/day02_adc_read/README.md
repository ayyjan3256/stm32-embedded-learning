# Day 2 — First ADC Read (Photoresistor via Voltage Divider)

## Overview
First working analog-to-digital conversion of the program. Reads a
photoresistor connected through a voltage divider on pin PA0, converts
the result to a corresponding voltage, and streams both values to a PC
terminal over UART.

## File
- `day02_adc_read.c` — ADC1 configuration and read function, plus the
  UART output path used to report results.

## Hardware
- STM32F405RGT6 (Blackpill)
- Photoresistor and 10 kΩ resistor wired as a voltage divider, with the
  midpoint connected to PA0
- USART2 (PA2/PA3) at 115200 baud for serial output

## Functionality
1. Enables the clocks for GPIOA and ADC1, and configures PA0 for analog
   input mode.
2. Configures ADC1 for 12-bit resolution, single-conversion mode, with
   the maximum sample time selected for channel 0. This is appropriate
   for a high-impedance analog source such as a resistive divider.
3. `ADC_Read()` triggers a conversion via software start, waits for the
   end-of-conversion flag, and returns the 12-bit result.
4. The raw value is converted to a voltage using the relationship
   `Voltage = (raw / 4095) × 3.3`, based on the ADC's 12-bit range and
   the board's 3.3 V reference.
5. Both the raw ADC value and the calculated voltage are printed once
   every 200 ms via `printf`, retargeted to UART2.

## Setup / Usage
1. Wire the photoresistor voltage divider to PA0 as described above.
2. Flash the board with `day02_adc_read.c` integrated into a generated
   `main.c` (see note below).
3. Enable floating-point support in `printf` by adding `-u _printf_float`
   under Project Properties → C/C++ Build → Settings → MCU GCC Linker →
   Miscellaneous, then rebuild.
4. Open a serial terminal (e.g. PuTTY) at 115200 baud on the board's
   COM port.
5. Expect continuous output in the form:
   ```
   Raw: 2731  Voltage: 2.20V
   ```
   Values will change in response to light level at the sensor.

## Note on File Contents
This file contains only the code written for this session and omits
CubeIDE/CubeMX-generated boilerplate (`HAL_Init`, `SystemClock_Config`,
`Error_Handler`, `USER CODE` markers, etc.). The listed functions and
configuration lines should be placed into their corresponding sections
of a generated `main.c` to build.
