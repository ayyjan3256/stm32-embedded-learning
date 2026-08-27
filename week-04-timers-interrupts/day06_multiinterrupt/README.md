## Day 6: Multi-Interrupt Priorities, Volatile Memory, and ISR-ADC

**Goal:** Transition from polling to a fully interrupt-driven, non-blocking architecture capable of handling multiple concurrent hardware events while the main CPU loop is under heavy load.

**Hardware Configurations:**
*   **PA0 (EXTI0):** KY-004 Button (Priority 1)
*   **PA4 (EXTI4):** KY-031 Knock Sensor (Priority 2)
*   **PA5 (ADC1):** KY-018 Photoresistor (Timer-triggered)
*   **PA1 (GPIO Out):** Button Indicator LED
*   **PB2 (GPIO Out):** TIM2 1Hz Heartbeat LED
*   **PA2/PA3 (USART2):** 115200 Baud Console

**Key Concepts Demonstrated:**
1.  **NVIC Preemption vs. Subpriority:** Configured Preemption Priorities (0 to 3) to allow critical interrupts to actively pause lower-priority ISRs mid-execution. 
2.  **EXTI Pending Register (PR):** Demonstrated the `rc_w1` (Read/Clear Write 1) hardware mechanism. Clearing an EXTI flag requires writing a `1` directly to the bit to prevent accidental clearing of concurrent interrupts.
3.  **Context-Shared Volatile Variables:** Used `volatile` to prevent the compiler's `-O1/-O2/-O3` optimizer from caching variables updated inside ISRs, ensuring the `main()` loop always reads the freshest RAM data.
4.  **Timer-Triggered ADC:** Replaced the blocking `ADC_Read()` with a `TIM3` interrupt firing at 100Hz. Polling `EOC` inside the ISR is acceptable here because the conversion takes ~15 CPU cycles (<0.1 µs), which does not violate interrupt timing constraints.
5.  **Interrupt Latency:** Documented the Cortex-M4 12-cycle latency (context saving and vector fetch) and 6-cycle tail-chaining optimization.
6.  **The Non-Blocking Proof:** Added a 5,000,000-cycle blocking loop to `main()`. Proved that while software reporting (`printf`) was delayed by the CPU load, background data collection (knocks, button toggles, ADC reads) never missed a single hardware event.