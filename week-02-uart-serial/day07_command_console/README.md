# Day 7 — Bidirectional Command Console (Week 2 mini project)

## Goal
Wire together everything from Week 2 into one working project: the PC
sends a text command over serial, the MCU parses it and toggles an
LED, then sends a confirmation string back.

## Files
- `day07_uart_command_console.c` — MCU side (STM32F405, USART2 on
  PA2/PA3, LED on PB2). Trimmed to the code actually written this
  week — CubeMX-generated boilerplate (`HAL_Init`, `SystemClock_Config`,
  `Error_Handler`, USER CODE markers) is omitted; drop these functions
  into the corresponding sections of a generated `main.c` to build.
- `console.py` — PC side (Python + pyserial)

## What each side does

**MCU (`day07_uart_command_console.c`)**
- Init: enables GPIOA/GPIOB/USART2 clocks, configures PA2/PA3 as AF7
  (USART2 TX/RX), PB2 as output (LED), BRR set for 115200 baud @ 16 MHz
  HSI (mantissa 8, fraction 11 → `0x8B` / `139`).
- Main loop: reads one byte at a time via `uart_read_byte()`, storing
  each into `rx_buffer` until a `\n` arrives. On `\n`, the buffer is
  null-terminated and compared with `strcmp()` against `"LED ON"` /
  `"LED OFF"`. Matching command toggles PB2 via `BSRR` and sends a
  confirmation line back via `printf()`/`_write()` (Day 5). No match →
  `"Unknown Command\n"`.

**PC (`console.py`)**
- Opens the given COM port at 115200 baud.
- Prompts for a command, sends it with an appended `\n` (the MCU's
  terminator), then reads and prints the MCU's response line.
- Type `exit` to break the loop and close the serial connection
  cleanly (`ser.close()`) without closing the terminal itself.

## Possible Pitfall
If the PC sends `\r\n` instead of `\n` (some OS/terminal serial
configs do this), the MCU's buffer ends up with a trailing `\r`
before the null terminator, so `strcmp()` against `"LED ON"` fails
and everything falls through to `"Unknown Command"` even though the
command looks correct on screen. Not an issue with `console.py` as
written (it only appends `'\n'`), but worth knowing if a different
terminal program is used against this same MCU code. Fix would be
stripping both `\r` and `\n` before the `strcmp()` checks.

## Bug encountered during bring-up
Originally `RCC->AHB1ENR` was written twice using plain `=` instead of
`|=` — the second write (GPIOB enable) silently overwrote the first
(GPIOA enable), since AHB1ENR is a shared register. Fixed by using
`|=` for all enable-bit writes to registers that may be written more
than once. `main.c` above already reflects the fix.

## Setup / usage
1. Drop `day07_uart_command_console.c`'s functions into a CubeMX-
   generated `main.c` (init call + `command_console_run()` in `main()`,
   rest in the USER CODE 0 section), then flash to the board
   (STM32CubeIDE project, Days 2-3 wiring: ST-Link crossed TX/RX to
   PA3/PA2, shared GND).
2. Confirm your board's COM port in Device Manager (Ports → STMicro-
   electronics STLink Virtual COM Port).
3. Update `PORT = 'COM4'` in `console.py` if different.
4. `pip install pyserial` if not already installed.
5. Run: `python console.py`
6. Type `LED ON` or `LED OFF` and press Enter — watch the physical LED
   and the confirmation line printed back. Type `exit` to quit.
