# Day 7 — Hardware Timers: Input Capture and Microsecond Precision

## Overview
Transitions from simple edge counting (EXTI) to precise duration measurement using hardware Input Capture. Configured TIM3 to capture the exact microsecond a physical event occurs, allowing the system to calculate the frequency and period of mechanical switch bounces without blocking the CPU.

## Files
- `day07_input_capture.c` — MCU-side configuration for TIM3 Input Capture on Channel 1, Alternate Function GPIO routing, UART printing, and 16-bit unsigned arithmetic for time-delta calculations.

## Hardware
- STM32F405RGT6 (Blackpill)
- KY-031 Knock Sensor connected to PA6 (TIM3_CH1 via AF2)
- USB Logic Analyzer (Channel 0 connected to PA6)
- USART2 (PA2/PA3) for 115200 Baud Console Output

## Functionality

### Input Capture Architecture
1. **Clock Setup:** TIM3 (on the 84 MHz APB1 bus) is scaled using a prescaler (`PSC`) of 83, creating a 1 MHz timer where 1 tick = 1 microsecond.
2. **Alternate Function Routing:** PA6 is routed to TIM3_CH1 using the Alternate Function Low Register (`AFR[0]`), bypassing standard GPIO mode.
3. **Capture/Compare Registers (`CCMR1` & `CCER`):** 
   - `CC1S = 01` configures the timer channel as an input listening to `TI1`.
   - `CC1P = 1` configures the edge detector to trigger on a falling edge.
4. **Hardware Capture Mechanism:** When a falling edge hits PA6, the hardware automatically copies the free-running `CNT` register into `CCR1` and sets the `CC1IF` interrupt flag.

### Data Processing & Filtering
- **16-bit Unsigned Math:** The duration between edges is calculated using `(uint16_t)(current_capture - last_capture)`. This exploits the C language's unsigned integer overflow behavior to natively handle cases where the timer hits the 65,535 `ARR` ceiling and rolls back to zero.
- **Software Filtering:** Added logic to ignore chattering bounces (< 20 us) and long idle periods (> 50,000 us), providing a clean, stable frequency readout of the dominant mechanical vibration.

## Setup / Usage
1. Generate an STM32CubeIDE project for the STM32F405RGTx (168 MHz SYSCLK).
2. Integrate the functions from `day07_input_capture.c` into the appropriate sections of `main.c`.
3. Connect a logic analyzer to PA6 and set it to trigger on a falling edge.
4. Build, flash, tap the sensor, and observe the period/frequency output on the Putty terminal.
5. Use the logic analyzer cursors to measure the gap between physical falling edges and verify it exactly matches the STM32's microsecond calculation.
