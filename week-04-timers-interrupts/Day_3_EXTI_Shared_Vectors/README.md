# Day 3: Bare-Metal External Interrupts (EXTI) & Shared Vectors

## 🎯 Objective
Transition from CPU-blocking polling methods to an **interrupt-driven** architecture. This project configures external hardware interrupts (EXTI) on the STM32F405 to detect button presses instantly, utilizing shared NVIC vectors and software debouncing.

## 🛠️ Hardware Setup
* **MCU:** STM32F405 (Blackpill)
* **Inputs:** `PA5` & `PA7` (External Buttons/Sensors)
* **Outputs:** `PB2` (Onboard LED for visual confirmation)

## 🧠 Key Concepts Explored

### 1. SYSCFG (System Configuration Controller)
Unlike basic GPIO, configuring an interrupt requires routing the physical pin to the EXTI controller. The `SYSCFG_EXTICR` registers act as the "multiplexer," linking `PA5` to `EXTI5` and `PA7` to `EXTI7`.

### 2. EXTI Edge Triggers & Unmasking
Interrupts must be explicitly armed. The EXTI registers dictate *how* the interrupt fires:
* **FTSR (Falling Trigger):** Fires when a button is pressed (pulled to GND).
* **IMR (Interrupt Mask Register):** "Unmasks" (enables) the specific EXTI line to allow the signal through to the NVIC.

### 3. NVIC & Shared Vectors
`EXTI9_5_IRQn` was enabled in the NVIC, allowing lines 5 through 9 to interrupt the CPU. Because pins 5 through 9 share the exact same hardware vector (`EXTI9_5_IRQHandler`), the CPU must check the **Pending Register (`EXTI->PR`)** to determine which specific pin knocked on the door.

### 4. Write-1-to-Clear & Safe Flag Handling
To acknowledge an interrupt, you must write a `1` to the specific bit in `EXTI->PR`. Crucially, you must *only* clear the flag for the pin that fired to avoid swallowing concurrent interrupts.

### 5. Software Debouncing via SysTick
Mechanical buttons "bounce" electrically, causing rapid-fire interrupts. A software debounce lock was implemented using `HAL_GetTick()` to ignore subsequent triggers for a fixed 200ms window.

## 🚀 Takeaways
* **ISRs must be fast:** Keep Interrupt Service Routines as short as possible. Set flags, then get out.
* **Interrupts free the CPU:** The `while(1)` loop is now entirely free to execute other code, as it no longer wastes cycles polling button states.
