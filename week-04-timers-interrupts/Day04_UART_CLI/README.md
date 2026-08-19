# Day 4: UART RX Interrupts & Command Line Interface (CLI)

## 🎯 Objective
Build a two-way, non-blocking Command Line Interface (CLI) over USART2. This project utilizes hardware RX interrupts to buffer incoming serial data in the background, safely passing it to the main loop to parse hardcoded commands and dynamic arguments.

## 🛠️ Hardware Setup
* **MCU:** STM32F405 (Blackpill)
* **UART/USB Bridge:** `PA2` (TX), `PA3` (RX) @ 115200 Baud
* **Outputs:** `PB2` (Onboard LED)

## 🧠 Key Concepts Explored

### 1. The RXNE Interrupt (`USART_CR1_RXNEIE`)
Instead of trapping the CPU in a `while()` loop waiting for incoming serial bytes, the UART peripheral is configured to fire an interrupt the exact moment the `RXNE` (Read Data Register Not Empty) flag goes high.

### 2. ISR as the "Data Gatherer"
Inside `USART2_IRQHandler`, the code strictly acts as a data collector. It reads `USART2->DR` (which auto-clears the `RXNE` flag), echoes the character back to the terminal, and stores it in a volatile array. 

### 3. Null Terminators & Race Condition Prevention
When the ISR detects a Carriage Return (`\r`), it caps the array with a null terminator (`\0`) to create a valid C string. It then raises a volatile flag (`command_ready = 1`). This prevents the main loop from trying to parse a half-typed word.

### 4. Main Loop as the "Command Executor"
The `while(1)` loop monitors the `command_ready` flag. When raised, it acts on the string without blocking the system:
* **`strcmp` (Static Commands):** Checks for exact string matches like `"LED ON"` or `"LED OFF"`.
* **`sscanf` (Dynamic Arguments):** Parses strings like `"DELAY 250"`, extracting the integer (`250`) and safely ignoring bad user input.

### 5. Non-Blocking Tasks (`HAL_GetTick`)
Instead of using `HAL_Delay()`, which pauses the entire CPU and breaks the CLI responsiveness, a background time-tracking variable (`last_blink_time`) uses `HAL_GetTick()` to blink the LED asynchronously at the speed determined by the `DELAY` command.

## 🚀 Takeaways
* **Separation of Concerns:** Never put slow functions like `printf` or `strcmp` inside an ISR. The ISR gathers; the main loop executes.
* **Baud Rate Math:** Changing the APB1 clock from 16 MHz (HSI) to 42 MHz (HSE/PLL) requires recalculating `USARTDIV` to prevent data corruption (gibberish/framing errors).
* **Industrial Standard:** This interrupt-driven buffer-and-flag architecture is exactly how embedded AT-command parsers (like Wi-Fi, GPS, and Bluetooth modules) are built.
