# Day 2 — Hardware Timers and Interrupts: TIM2 1 Hz Blink

## Overview
Replaces CPU-blocking delays and polling loops with a true hardware-driven 
timer interrupt. Configures TIM2 on the 84 MHz APB1 timer clock to generate 
an exact 1 Hz update event, triggering an Interrupt Service Routine (ISR) 
that toggles an LED.

## Files
- `day02_timer_interrupt.c` — MCU-side configuration for TIM2, prescaler math, 
  NVIC interrupt routing, and the `TIM2_IRQHandler` service routine.

## Hardware
- STM32F405RGT6 (Blackpill)
- External 8 MHz HSE crystal configured via PLL to 168 MHz system clock
- External LED connected to PB2

## Functionality

### Timer Configuration & Math
1. **Clock Setup:** System clock runs at 168 MHz. APB1 bus prescaler is set to 4, 
   which automatically doubles the timer clock input for TIM2 to **84 MHz**.
2. **Prescaler (`PSC`):** Set to `83` to scale the 84 MHz down to a 1 MHz tick rate 
   (1 tick = 1 microsecond).
3. **Auto-Reload (`ARR`):** Set to `999999` to define a 1,000,000-tick period, 
   achieving an exact 1 Hz rollover rate.

### Interrupt Architecture
- **Peripheral Level (`DIER`):** The Update Interrupt Enable (`UIE`) bit is set, 
  allowing TIM2 to forward its rollover flag to the core.
- **Core Level (`NVIC`):** `TIM2_IRQn` is enabled with preemption priority 0 via 
  CMSIS functions, allowing the CPU to accept the vector signal.
- **ISR Handling (`TIM2_IRQHandler`):** Explicitly inspects `TIM2->SR` for `TIM_SR_UIF`, 
  clears the flag using bitwise AND-NOT (`&= ~`), and performs an atomic XOR toggle 
  on `GPIOB->ODR`.

## Setup / Usage
1. Generate an STM32CubeIDE project for the STM32F405RGTx, ensuring the HSE input 
   frequency is set to `8 MHz` and SYSCLK is scaled to `168 MHz`.
2. Integrate the functions from `day02_timer_interrupt.c` into the appropriate 
   sections of `main.c`.
3. Build, flash, and verify a consistent 1-second interval toggle on the PB2 LED.