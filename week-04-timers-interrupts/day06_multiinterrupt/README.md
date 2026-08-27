# Day 6 — Multi-Interrupt Priorities, Volatile Memory, and ISR-ADC

## Overview
Transitions from polling to a fully interrupt-driven, non-blocking architecture capable of handling multiple concurrent hardware events. Demonstrates the STM32's ability to juggle multiple sensors using Nested Vectored Interrupt Controller (NVIC) priorities, ensuring data collection continues seamlessly even when the main CPU loop is under heavy load.

## Files
- `day06_multitasking.c` — MCU-side configuration for dual timers (TIM2, TIM3), dual external interrupts (EXTI0, EXTI4), ADC1 background reads, NVIC priority assignments, and safe ISR-to-main data sharing using `volatile` variables.

## Hardware
- STM32F405RGT6 (Blackpill)
- KY-004 Button connected to PA0 (EXTI0)
- KY-031 Knock Sensor connected to PA4 (EXTI4)
- KY-018 Photoresistor connected to PA5 (ADC1_IN5)
- External LED connected to PA1 (Button Indicator)
- External LED connected to PB2 (Heartbeat Indicator)
- USART2 (PA2/PA3) for serial console output

## Functionality

### NVIC Priority & Interrupt Architecture
1. **Preemption Priorities:** Configured custom priority levels (0 to 3) in the NVIC. Proved that a higher-priority hardware event (Priority 0) can actively preempt and pause a lower-priority ISR mid-execution.
2. **Interrupt Latency:** Validated Cortex-M4 hardware behavior, including the 12-cycle latency for context saving (stack push) and vector fetching, as well as the 6-cycle tail-chaining optimization for back-to-back interrupts.
3. **EXTI Pending Register (`PR`):** Demonstrated the `rc_w1` (Read/Clear Write 1) hardware mechanism. Clearing an EXTI flag requires writing a `1` directly to the bit to prevent the accidental dropping of concurrent interrupts during read-modify-write operations.

### Context-Shared Data & Volatile Variables
- **The `volatile` Keyword:** Applied to variables shared between ISRs and the `main()` loop (`display_count`, `update_count`). This prevents the compiler's optimization passes (`-O1` through `-O3`) from caching the variables in CPU registers, forcing a fresh read from RAM so `main()` never misses an ISR update.
- **Decoupled Architecture:** EXTI ISRs silently collect data in the background. TIM2 (1 Hz) packages this data and sets a flag, allowing `main()` to process and print the information at its own pace without blocking hardware capture.

### Timer-Triggered ADC (ISR-ADC)
- Replaced blocking `ADC_Read()` loops with a 100 Hz `TIM3` update interrupt. 
- The ISR triggers an ADC `SWSTART` and performs a safe, 15-cycle micro-poll of the `EOC` flag (<0.1 µs), safely capturing analog data in the background without violating interrupt timing constraints.

## Setup / Usage
1. Generate an STM32CubeIDE project for the STM32F405RGTx (168 MHz SYSCLK).
2. Integrate the functions from `day06_multitasking.c` into the appropriate sections of `main.c`.
3. Add a 5,000,000-cycle empty `for` loop to the `while(1)` block to simulate a heavy CPU workload (like an AI model).
4. Build, flash, and monitor the UART terminal. Observe that despite the massive CPU delay in `main()`, hardware data collection (knocks, button toggles, ADC light reads) never misses a single event.
EOF
