# Week 2 Reference Sheet — UART / Serial Communication

## Setup
- Reference manual: **RM0090, Chapter 30** (USART)
- Debugger: ST-Link V2.1 (VCP built in, no separate USB-serial adapter)
- Wiring: ST-Link TXD/RXD crossed to board PA3(RX)/PA2(TX), shared GND
- Clock: 16 MHz HSI, no PLL (kept simple on purpose this week)
- Target baud: 115200

## Core concepts (Day 1)
- **Async UART:** no shared clock line — both sides must independently
  agree on baud rate in advance.
- **Oversampling:** multiple samples per bit period, used as a
  majority-vote noise filter (no clock to trust a single sample).
- **Frame structure:** idle-high line → start bit (falling edge = the
  only sync signal) → 8 data bits, LSB-first → stop bit (also a
  framing-error sanity check).
- **Fractional BRR:** integer-only dividers leave rounding error that
  compounds at high baud rates; the 4-bit fraction lands between two
  integer divisor steps for better precision.
- Registers used: `SR`, `DR`, `BRR`, `CR1`. Not used: `GTPR`
  (smartcard-only), `CK`/`CTS`/`RTS` (sync mode / hw flow control,
  not wired this week).

## GPIO / clock setup (Days 2-3)
```c
RCC->APB1ENR |= RCC_APB1ENR_USART2EN;   // USART2 is on APB1, not AHB1
RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;    // |= matters — shared register

GPIOA->MODER |= (1 << 5);   // PA3 -> alternate function (10)
GPIOA->MODER &= ~(1 << 4);
GPIOA->MODER |= (1 << 7);   // PA2 -> alternate function (10)
GPIOA->MODER &= ~(1 << 6);

GPIOA->AFR[0] |= (7 << 8);   // PA2 -> AF7
GPIOA->AFR[0] |= (7 << 12);  // PA3 -> AF7
```
MODER field for pin `n`: bits `[2n+1 : 2n]`. AFR field for pin `n`
(AFR[0] covers pins 0-7): bits `[4n+3 : 4n]`.

## BRR hand-calc (Day 3), 115200 baud @ 16 MHz
```
16,000,000 / (16 × 115200) = 8.6805
mantissa = 8
fraction = 0.6805 × 16 ≈ 11 (0xB)
BRR = (8 << 4) | 11 = 139 (0x8B)
```

## Polling I/O (Day 4)
```c
uint8_t uart_read_byte(void) {
    while (!(USART2->SR & USART_SR_RXNE));  // cleared by reading DR
    return USART2->DR;
}
void uart_write_byte(uint8_t c) {
    while (!(USART2->SR & USART_SR_TXE));   // cleared by writing DR
    USART2->DR = c;
}
```
- **TXE** clears on a write to `DR`. **RXNE** clears on a read of `DR`.
  Reading `SR` to check the flag does *not* consume it — different
  from SysTick's COUNTFLAG (Week 1), which cleared just by reading it.
- `uart_read_byte()` is blocking — will sit forever if nothing arrives.

## printf retargeting (Day 5)
```c
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) uart_write_byte(ptr[i]);
    return len;
}
```
- `printf` formats → `_write()` transmits. Two separate jobs.
- `file` (stdout/stderr fd) ignored — only one output device exists.
- Needs external (non-`static`) linkage: newlib's default `_write()`
  is a **weak** symbol; any normal (strong) symbol with the same name
  automatically overrides it at link time.
- Requires `#include <stdio.h>` in the calling file, or `printf` gets
  an implicit/mismatched declaration and the build fails.
- Return value is bookkeeping only — it does *not* gate whether bytes
  are sent (the loop already ran); it just misreports success to
  `printf`'s internals if wrong.

## pyserial basics (Day 6)
```python
import serial
ser = serial.Serial('COM4', 115200, timeout=2)   # timeout > MCU send interval

line = ser.readline()                # bytes, e.g. b'COUNT:0\n'
text = line.decode('ascii').strip()  # bytes -> str, drop \n
parts = text.split(':')              # ["COUNT", "0"]
count = int(parts[1])                # str -> real int
```
- COM port is the *receiving* end of the same wire the MCU transmits
  on — opening it from Python doesn't affect the MCU's independent
  loop.
- `readline()` returns `b''` (not an error) on timeout with no data —
  check `if line:` before parsing, or a later index will throw.
- `timeout` should be **longer** than the sender's interval, not
  equal to it, to avoid racing a timeout against an in-progress line.
- `.encode('ascii')` is the reverse of `.decode()`, used when writing
  a command back out: `ser.write((cmd + '\n').encode('ascii'))`.
- `ser.close()` releases the port without closing the terminal/script.

## Buffered command parsing (Day 7)
```c
char rx_buffer[32];
int  rx_index = 0;
// ...
char c = uart_read_byte();
if (c == '\n') {
    rx_buffer[rx_index] = '\0';       // required — see below
    if (strcmp(rx_buffer, "LED ON") == 0) { /* ... */ }
    rx_index = 0;
} else {
    rx_buffer[rx_index++] = c;
}
```
- Bytes arrive one at a time — accumulate into a buffer until `\n`,
  then the message is "complete."
- Null terminator is required because `strcmp()` (and C string
  functions generally) have no length info of their own — they scan
  until `'\0'`. Without it, a shorter new message can expose stale
  leftover bytes from a previous, longer message still sitting in the
  reused buffer.
- `strcmp()` returns **0 for equal** — not `1`/`true`. Use `== 0`.
- Known gotcha: a `\r\n` terminator (some terminal/OS configs) leaves
  a stray `\r` before the `\0`, so `strcmp` against `"LED ON"` fails
  even though the command looks correct on screen. Fix: strip both
  `\r` and `\n`, not just check for `\n`.

## Bugs actually hit this week (worth remembering)
- **`RCC->AHB1ENR` written twice with `=` instead of `|=`** — second
  write (GPIOB enable) silently overwrote the first (GPIOA enable),
  since they share one register. General rule: use `|=` on any
  enable-bit register that might be written more than once.
- **`USART2_CR1_...` vs `USART_CR1_...`** — CMSIS bitmask constants
  are prefixed with the peripheral *type* (`USART`), not the specific
  instance (`USART2`). Same pattern as `USART_SR_TXE`.
- **Missing `#include <stdio.h>`** caused an implicit-declaration
  build error for `printf` — compile-time, unrelated to UART/linker.
- **Python REPL auto-echo** — typing `ser.write(...)` interactively
  prints its return value (byte count) automatically, even with no
  `print()` call; doesn't happen when run as a `.py` script.

## Still open
- None — Day 3's BRR hand-calc was eventually walked through and
  verified during Day 7's rebuild-from-memory.
