##Day 4 — USART2 Bare-Metal UART (PA2/PA3)

Board: WeAct STM32F405RGT6 Blackpill
Debugger: ST-Link V2.1 (10-pin, has VCP support)
Clock: 16 MHz HSI, no PLL

## What this code does

Configures USART2 on PA2 (TX) / PA3 (RX) entirely via raw registers, no HAL
UART calls. Implements blocking `uart_write_byte()` / `uart_read_byte()` via
TXE/RXNE polling, and runs a simple echo loop in `main()` — whatever byte
arrives on RX gets sent straight back out on TX.

## Wiring

ST-Link V2.1 (10-pin: 3V3, SWDIO, SWCLK, GND, SWO, NRST, TXD, RXD, GND, 5V)
connected via breadboard to the Blackpill:

- ST-Link TXD -> board PA3 (USART2_RX)
- ST-Link RXD -> board PA2 (USART2_TX)
- Shared GND

**Board-specific note:** this Blackpill's onboard 8-pin SWD/UART header is
silkscreened "UART RX -> A9, TX -> A10" — that header is wired to **USART1**
(PA9/PA10), not USART2. This code deliberately uses PA2/PA3 via breadboard
instead of that header. If reusing the onboard header directly, the clock
enable moves to `RCC->APB2ENR` (USART1EN) instead of `APB1ENR`, and every
`USART2->` reference becomes `USART1->`.

## BRR / baud rate math

`USARTDIV = f_CK / (16 × baud)`, where `f_CK` is APB1's clock (16 MHz here,
since AHB/APB1 dividers are both DIV1). Mantissa = integer part, fraction
field = round(decimal remainder × 16), packed as `(mantissa << 4) | fraction`.

- **115200 baud:** USARTDIV = 8.6875 → mantissa=8, fraction=11 → BRR = `0x8B`
  (139 decimal) → actual ≈ 115108 baud, ~0.08% error
- **9600 baud:** USARTDIV = 104.1666 → mantissa=104, fraction=3 → BRR = `0x683`
  (1667 decimal) → actual ≈ 9598 baud, ~0.02% error

This code uses `0x8B` (115200).

## Register sequence, in order

1. `RCC->AHB1ENR` — enable GPIOA clock
2. `RCC->APB1ENR` — enable USART2 clock (USART2 is on APB1, not AHB1 — a
   slower peripheral bus, unlike GPIO)
3. `GPIOA->MODER` — PA2/PA3 to alternate function mode (`10`, not `01`
   like a GPIO output pin)
4. `GPIOA->AFR[0]` — PA2/PA3 to AF7, which selects USART2 specifically
   (MODER=10 alone only says "some peripheral controls this pin," AFR
   says which one)
5. `GPIOA->OSPEEDR` — high speed, since edge slew rate starts to matter
   against the bit period at 115200 baud in a way it didn't for a slow
   LED blink
6. `USART2->BRR` — baud rate value
7. `USART2->CR1` — UE (peripheral enable), TE (transmitter), RE (receiver)

Clock enables must come before any GPIOA/USART2 register write — writes to
an unclocked peripheral are silently ignored, no fault raised.

## Challenges

- Enabling TE causes USART2 to send one idle frame immediately (documented
  behavior, RM0090 §30.3) — this is also the moment PA2 stops floating and
  settles to a driven idle-high level.
- TXE is cleared by **writing** DR. RXNE is cleared by **reading** DR.
  Neither is cleared by reading SR — unlike SysTick's COUNTFLAG (Week 1),
  which clears just from reading CTRL. Safe to poll SR repeatedly without
  losing the flag.
- `uart_read_byte()` is blocking — it spins forever if no byte ever arrives.
  Fine for this week's polling-only approach; Week 4 (NVIC/interrupts) is
  the path to a non-blocking version.
- A multimeter reading ~3.3V on both PA2 and PA3 doesn't confirm the link is
  working — both idle high, and a multimeter is far too slow to catch actual
  byte transitions at 115200 baud (~87 µs/byte).
- An extended "nothing shows up in PuTTY" session turned out to be a loose
  breadboard jumper, not a code bug — worth a physical wiggle-test on the
  wires before assuming the register config is wrong.
