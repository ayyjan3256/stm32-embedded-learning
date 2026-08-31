# Day 9 — Week 4 Rebuild and Multi-Sensor Telemetry

## Overview
Culmination of Week 4, integrating bare-metal timer interrupts, hardware-triggered ADC conversions, external interrupts (EXTI), and PWM generation into a single non-blocking architecture. Introduces advanced hardware features including the ADC Analog Watchdog for zero-CPU-cost threshold monitoring and PWM preload with gamma correction for perceptually linear output.

## File
`day09_timers_interrupts_rebuild.c` — complete bare-metal configuration for `TIM2`, `TIM3`, `TIM4`, `EXTI0`, `EXTI1`, hardware-triggered ADC, and UART telemetry.

## Hardware
* **STM32F405RGT6 (Blackpill)**
* Push button on `PA0`
* KY-031 Knock/Vibration sensor on `PA1`
* Photoresistor voltage divider on `PA4`
* LED on `PB6` (Current-limiting resistor to GND)
* USART2 (`PA2`/`PA3`) at 115200 baud for serial output

## Functionality
* Clock configuration and GPIO alternate function mapping are executed directly via `RCC` and `MODER`/`AFR` registers, bypassing hardware abstraction layers.
* EXTI lines 0 and 1 are configured for `PA0` and `PA1` to trigger on falling edges. Pending registers (`PR`) are explicitly cleared inside the respective interrupt service routines (ISRs) to prevent infinite interrupt loops.
* `TIM2` is configured as a standalone timebase using the Update Interrupt Enable (`UIE`).
* `TIM4` is configured for PWM generation on Channel 1 (`PB6`) with hardware preload (`ARPE` and `OCPE`) enabled to prevent mid-cycle pulse corruption. A logarithmic gamma correction table is computed dynamically to map linear increments to perceptually smooth LED dimming.
* `TIM3` is configured as a master trigger (TRGO on Update Event) for `ADC1`, enabling hardware-synchronized analog conversions at a fixed frequency without requiring CPU polling or software triggers.
* `ADC1` utilizes the Analog Watchdog (AWD) feature on Channel 4. The hardware evaluates conversions against programmed high (`HTR`) and low (`LTR`) thresholds, triggering a dedicated interrupt only when bounds are exceeded.
* A continuous serial stream transmits comma-separated telemetry (light level and vibration counts) for external data logging.

## Setup / Usage
1. Wire the sensors and LED as described above. Note that `PB6` is strictly required for `TIM4_CH1` output.
2. Flash the board with `day09_timers_interrupts_rebuild.c` integrated into a generated `main.c` (see note below).
3. Open a serial terminal at 115200 baud or run a Python serial logging script.
4. Expect output in the form of a continuous CSV stream:
   ```text
   2104,0
   2105,1
