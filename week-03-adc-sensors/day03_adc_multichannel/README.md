# Day 3 — Second Channel, Internal Temperature Sensor

## Overview
Extends Day 2's single-channel ADC read to a second external sensor and
the microcontroller's internal temperature sensor. Demonstrates
parameterized channel selection on a shared ADC, and converts both an
external thermistor and the internal sensor to a temperature in degrees
Celsius.

## File
- `day03_adc_multichannel.c` — parameterized ADC read function, internal
  temperature sensor setup, and both sensor conversion formulas.

## Hardware
- STM32F405RGT6 (Blackpill)
- Photoresistor voltage divider on PA0 (from Day 2)
- NTC thermistor voltage divider (10 kΩ fixed resistor) on PA1
- USART2 (PA2/PA3) at 115200 baud for serial output

## Functionality
1. `ADC_Read()` accepts a channel number and overwrites SQR3 on each
   call, allowing a single function to service any channel without
   channel selection carrying over between calls.
2. PA0 and PA1 are both configured for analog input mode, alongside the
   existing USART2 alternate function configuration on PA2/PA3.
3. The internal temperature sensor and voltage reference are enabled
   via the ADC common control register (`ADC123_COMMON->CCR`), which is
   separate from the per-ADC control registers used for external
   channels. A short delay follows to allow the sensor to stabilize
   before its first conversion.
4. The internal sensor's raw value is converted to voltage, then to
   degrees Celsius using the reference manual's linear formula and the
   device's calibration constants.
5. The external thermistor's raw value is converted to resistance based
   on its position in a voltage divider, then to degrees Celsius using
   the Steinhart-Hart equation with generic 10 kΩ NTC coefficients.
6. All three readings — photoresistor, external temperature, and
   internal chip temperature — are printed once every 2 seconds.

## Setup / Usage
1. Wire both sensor dividers as described above.
2. Flash the board with `day03_adc_multichannel.c` integrated into a
   generated `main.c` (see note below).
3. Confirm `-u _printf_float` is still set under Project Properties →
   C/C++ Build → Settings → MCU GCC Linker → Miscellaneous.
4. Open a serial terminal at 115200 baud.
5. Expect output in the form:
   ```
   Light: 3421  Ext Temp Raw: 1802  Ext Temp C: 23.14
   ITemp Sensor Raw: 27.60
   ```

## Accuracy Note
The Steinhart-Hart coefficients used for the external thermistor are
generic values for common 10 kΩ NTC thermistors, not values from a
manufacturer datasheet for the specific component used. Readings should
be treated as approximate. The internal sensor's calibration constants
are taken from the reference manual's typical values and may vary
slightly by device revision.

## Note on File Contents
This file contains only the code written for this session and omits
CubeIDE/CubeMX-generated boilerplate (`HAL_Init`, `SystemClock_Config`,
`Error_Handler`, `USER CODE` markers, etc.). The listed functions and
configuration lines should be placed into their corresponding sections
of a generated `main.c` to build.
