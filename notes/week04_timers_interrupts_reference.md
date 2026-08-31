# Week 4 Reference Sheet — Bare-Metal Timers, Interrupts, & Hardware Triggers

### Setup
* **Reference manual:** RM0090, Chapters 10 (Interrupts/EXTI), 13 (ADC), 14-17 (Timers)
* **Wiring:** PA0 (Push button, EXTI0), PA1 (Vibration sensor, EXTI1), PA4 (Photoresistor, ADC1_IN4), PB6 (LED, TIM4_CH1)
* **Clock:** 168 MHz SYSCLK. Timers 2, 3, and 4 reside on APB1. APB1 base clock is 42 MHz, but internal hardware multipliers double the timer clock to **84 MHz**.

### Core Concepts
* **Interrupt-Driven Architecture:** Replaces blocking `while()` polling. The CPU executes the main loop continuously and is preempted by the Nested Vectored Interrupt Controller (NVIC) only when hardware flags are raised.
* **EXTI vs Input Capture:** EXTI triggers a software routine asynchronously upon detecting a voltage edge. Input Capture utilizes a hardware timer to autonomously record the exact microsecond timestamp of an edge into a `CCR` register, bypassing software latency.
* **Open-Drain Configuration:** Required for single-wire, bi-directional protocols (like DHT11). Prevents short circuits by utilizing a physical pull-up resistor to hold the line high (1). Devices communicate by pulling the line to ground (0) or releasing it.
* **DHT11 Protocol Timing:** MCU pulls low for 18ms to wake. Sensor responds with 40 bits. `0` = 26–28 µs high pulse. `1` = 70 µs high pulse.
* **Interrupt Latency:** The Cortex-M4 requires exactly 12 clock cycles to push the current execution state to the stack and branch to the Interrupt Service Routine (ISR).

### Timer Math & PWM Setup
* **Formula:** `f_timer = f_bus / (PSC + 1)`
* To achieve a 1 MHz (1 µs) tick rate from an 84 MHz bus, the prescaler must be 83 (`TIMx->PSC = 83`).
* **PWM Preload:** Modifying a timer's Capture/Compare Register (`CCR`) mid-cycle generates corrupted pulse widths. Setting `ARPE` (Auto-Reload Preload Enable) and `OCPE` (Output Compare Preload Enable) buffers the write operation until the timer rolls over to 0.

```c
TIM4->PSC = 83;
TIM4->ARR = 999;             // 1 kHz PWM frequency
TIM4->CCMR1 |= (6 << 4);     // PWM Mode 1
TIM4->CR1 |= (1 << 7);       // ARPE: Enable auto-reload preload
TIM4->CCMR1 |= (1 << 3);     // OC1PE: Enable output compare preload
```

### EXTI Routing & The "Sticky Flag"
* General purpose I/O pins must be explicitly mapped to EXTI lines using the System Configuration (`SYSCFG`) registers.
* The `volatile` keyword is mandatory for any variable modified inside an ISR and read in `main()`, preventing the compiler from caching the variable in a core register.

```c
// Mapping PA0 to EXTI0
SYSCFG->EXTICR[0] &= ~(0xF << 0); 
EXTI->IMR |= (1 << 0);            // Unmask EXTI0
EXTI->FTSR |= (1 << 0);           // Trigger on falling edge

void EXTI0_IRQHandler(void) {
    if (EXTI->PR & (1 << 0)) {
        // ... execution logic ...
        EXTI->PR = (1 << 0);      // Clear flag by writing 1
    }
}
```

* **Clearing mechanisms differ by peripheral:** EXTI Pending Registers (`PR`) are cleared by writing a `1` (`= (1<<x)`). Timer Status Registers (`SR`) are cleared by writing a `0` (`&= ~(1<<x)`). Reading an ADC Data Register (`DR`) clears its `EOC` flag automatically.

### Hardware Triggers & Analog Watchdog
* **Hardware Trigger (TRGO):** Links peripherals directly. `TIM3` generates an Update Event, and `ADC1` listens to that event to start a conversion. The CPU is bypassed entirely.
* **Analog Watchdog:** Evaluates ADC conversions against High Threshold (`HTR`) and Low Threshold (`LTR`) limits in hardware. Triggers an interrupt only upon a limit violation, reducing CPU wake cycles.

```c
// TIM3 Trigger Setup
TIM3->CR2 |= (2 << 4);           // MMS = 010 (Update Event generates TRGO)

// ADC1 Hardware Trigger & Watchdog Setup
ADC1->CR2 |= (1 << 28) | (8 << 24); // EXTEN = 01 (Rising Edge), EXTSEL = 1000 (TIM3 TRGO)
ADC1->HTR = 3000;
ADC1->LTR = 1000;
ADC1->CR1 |= (4 << 0) | (1 << 9) | (1 << 23) | (1 << 6); // AWDCH=4, AWDSGL, AWDEN, AWDIE
```

### Python Serial Parsing (Data Collection)

```python
import serial
ser = serial.Serial('COM4', 115200, timeout=1)
log_file = open('week4_normal.csv', 'w') # 'w' overwrites, 'a' appends

while True:
    if ser.in_waiting:
        try:
            # decode('utf-8', errors='ignore') prevents crashes on startup garbage bytes
            raw = ser.readline().decode('utf-8', errors='ignore').strip()
            parts = raw.split(',')
            
            if len(parts) == 2:          # Strict column validation
                log_file.write(f"{parts[0]},{parts[1]}\n")
                log_file.flush()         # Force write to disk immediately
        except ValueError:
            pass
```

### Encountered Bugs
* **The "Ghost Handler" Trap:** Setting `UIE` in the peripheral and enabling the IRQ in the NVIC without defining the corresponding `IRQHandler` function. Hardware raises the flag, the CPU branches to a missing function, and crashes into the infinite `Default_Handler` loop.
* **ISR Spelling / Typo Failures:** Naming a handler `EXTIO_IRQHanlder` (using 'O' instead of '0', and spelling 'Hanlder' instead of 'Handler'). The compiler throws no errors because it treats it as a custom function, resulting in the same `Default_Handler` crash as the Ghost Handler trap.
* **Missing Flag Clears:** Failing to explicitly clear `EXTI->PR` before exiting the ISR. The CPU exits, immediately sees the flag is still high, and re-enters the ISR infinitely, freezing `main()`.
* **Unsigned Integer Math Wrapping:** Using `uint8_t` for an animation direction variable. When `0 - 1` is evaluated, the unsigned integer wraps to `255`, causing out-of-bounds array memory corruption. Standard signed `int` must be used when negative traversal is required.
* **PWM ARR Ceiling vs Floating Point Math:** Generating a gamma correction table with a maximum multiplier of `1000.0f` while the timer's `ARR` ceiling is `999`. Writing `1000` to the `CCR` causes the timer hardware to miss the compare match for exactly one cycle, resulting in a distinct visual "blink" at peak brightness. Math constraints must perfectly match hardware register limits.
