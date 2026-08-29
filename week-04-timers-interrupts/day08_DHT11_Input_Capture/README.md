# Week 4: Bare-Metal Protocol Reverse Engineering & Input Capture

## Overview
This project implements a custom, bare-metal C driver for the proprietary 1-wire protocol of the DHT11 Temperature & Humidity sensor on the STM32F4. It abandons simple blocking loops in favor of highly accurate hardware timer Input Capture to measure microsecond-level pulse widths.

## Hardware Used
* STM32F4-series Microcontroller
* DHT11 Temperature & Humidity Sensor
* 10kΩ Pull-Up Resistor
* PB8 mapped to Timer 4, Channel 3

## Core Engineering Achievements

### 1. Microsecond Input Capture (The Stopwatch)
Configured `TIM4_CH3` via Alternate Function (AF2) registers to map directly to PB8. Utilizing a prescaler of 83 to divide the 84MHz APB1 clock down to precisely 1 MHz (1 tick = 1 µs), allowing for deterministic hardware measurements unaffected by CPU bottlenecks. 

### 2. Dynamic Polarity Toggling
Implemented a state-flipping Interrupt Service Routine (ISR) that dynamically toggles the `CC3P` (Edge Polarity) bit in the `CCER` register using a bitwise XOR (`^=`). This captures exact hardware timestamps for both the start and end of every logic level.

### 3. Bitwise Decoding & Checksum Validation
Translated 80 hardware pulse durations back into 40 distinct binary bits by verifying whether the `HIGH` pulse width exceeded a 50µs threshold. Assembled the final payload using left bit-shifts (`<<= 1`) and validated data integrity using the sensor's native 8-bit checksum parity byte.

### 4. Non-Blocking System Architecture
Removed forced CPU paralysis (`delay_ms()`) by replacing the `while(1)` architecture with a `HAL_GetTick()` non-blocking timebase loop. This guarantees that multiple sensors (ADC photoresistors, TIM3 knock sensors, and TIM4 environmental sensors) can operate concurrently in parallel without freezing the system core.
