# Day 5: Hardware Timers, PWM, and Timer Interrupts

## 🎯 Objective
Generate a stable, CPU-independent Pulse Width Modulation (PWM) signal to control an LED's brightness. Integrate this with a Command Line Interface (CLI) to dynamically adjust the duty cycle, and utilize Timer Interrupts to create a non-blocking "breathing" animation.

## 🛠️ Hardware Setup
* **MCU:** STM32F405 (Blackpill)
* **PWM Output:** `PA0` (connected to `TIM2_CH1`) to an external LED.
* **UART/USB Bridge:** `PA2` (TX), `PA3` (RX) @ 115200 Baud.

## 🧠 Key Concepts Explored

### 1. Alternate Function (AF) Routing
Physical pins must be handed over to the hardware peripherals. `PA0` was configured to `AF1`, directly bridging the physical copper trace to the internal Timer 2 hardware.

### 2. Time Base (PSC and ARR)
Generating a 1 kHz PWM signal from an 84 MHz timer clock requires exact division:
* **`PSC` (Prescaler):** Set to 83, dividing the 84 MHz clock down to 1 MHz (1 tick = 1 microsecond).
* **`ARR` (Auto-Reload Register):** Set to 999. The timer counts 1,000 steps at 1 MHz, resetting exactly 1,000 times per second (1 kHz).

### 3. Duty Cycle (CCR1)
The Capture/Compare Register acts as the threshold. If `ARR` is 999, setting `CCR1` to 250 keeps the pin HIGH for 25% of the cycle, yielding 25% perceived brightness.

### 4. Timer Interrupts (`TIM_DIER_UIE`)
Instead of trapping the CPU in a `while(1)` delay loop to animate the LED, the Timer was configured to fire an interrupt every time it resets. The `TIM2_IRQHandler` utilizes a static counter to run mathematics in the background every 10 milliseconds, creating a smooth "breathing" fade effect that is completely immune to blocking code in the main loop.

## 🚀 Takeaways
* **Hardware offloading:** The Cortex-M4 core now does exactly 0% of the work required to keep the LED illuminated at a specific dimming level.
* **Interrupt Priorities:** Assigned priority `0` to the UART (so we never drop a keystroke) and priority `1` to the Timer.
* **Seamless CLI Integration:** Parsed integer values directly from the serial terminal via `sscanf` and wrote them directly to the `CCR1` register on the fly. Manual overrides instantly disable the background breathing effect.
