# Day 6 — Resolution, Sample Time, and Noise

## Overview
Investigates three ADC configuration tradeoffs on real hardware: output
resolution vs. conversion speed, sample time vs. speed, and the effect
of averaging on measurement noise. Uses the Cortex-M4's built-in DWT
cycle counter to measure real conversion rates rather than relying on
datasheet figures alone.

## File
- `day06_adc_tuning.c` — resolution-switching helper, DWT-based rate
  measurement, and an averaging read function.

## Hardware
- STM32F405RGT6 (Blackpill)
- Photoresistor voltage divider on PA0
- USART2 (PA2/PA3) at 115200 baud for serial output

## Functionality

### Resolution comparison
`ADC_SetResolution()` sets the RES bits in `ADC1->CR1` using a
clear-then-set pattern, since RES is a 2-bit field and a plain OR could
combine incorrectly with a previous setting. The same sensor is read at
12, 10, 8, and 6-bit resolution in sequence, confirming that each lower
resolution result is approximately the top bits of the full 12-bit
result (e.g. the 8-bit value approximates the 12-bit value right-shifted
by 4).

### Sample rate measurement (DWT)
The Cortex-M4's cycle counter (`DWT->CYCCNT`) is enabled once at startup
and used to measure the real time taken for 1000 back-to-back ADC
conversions, converted to a conversions-per-second figure. This was run
at both the slowest sample time (480 cycles) and fastest (3 cycles) to
compare real measured throughput against the theoretical cycle-count
ratio from the reference manual. Measured speedup was smaller than the
theoretical ratio due to fixed software overhead in the polling read
function, which becomes proportionally larger as ADC-side cycles shrink.

### Noise and averaging
With sample time restored to 480 cycles, the photoresistor was read
continuously with no delay to observe natural ADC jitter on a
completely still sensor. `ADC_ReadAveraged()` takes multiple readings
and returns their mean, reducing the visible effect of that jitter
without eliminating its underlying causes (power supply ripple, thermal
noise, digital switching noise, input impedance effects).

## Setup / Usage
1. Flash the board with `day06_adc_tuning.c` integrated into a
   generated `main.c` (see note below). The resolution comparison and
   DWT measurement blocks are intended to run once at startup, not
   inside the main loop.
2. Open a serial terminal at 115200 baud.
3. To reproduce the noise/averaging comparison, run the unaveraged read
   in a tight loop first and record the observed value range, then
   switch to `ADC_ReadAveraged()` and compare the new range.

## Note on File Contents
This file contains only the code written for this session and omits
CubeIDE/CubeMX-generated boilerplate (`HAL_Init`, `SystemClock_Config`,
`Error_Handler`, `USER CODE` markers, etc.). The listed functions and
configuration lines should be placed into their corresponding sections
of a generated `main.c` to build.
