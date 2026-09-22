# Week 6, Day 2 — DMA Fundamentals & ADC1+DMA2 Pipeline

**Board:** WeAct STM32F405RGT6 Blackpill
**Clock:** HSE = 8MHz, SYSCLK = 168MHz via PLL (PLLM=4, PLLN=168, PLLP=/2)
**Reference:** RM0090 — DMA controller chapter (search by bookmark title, not
section number; numbering shifts across revisions)

## Overview

This day proves the DMA engine works correctly, in two stages:

1. A minimal **memory-to-memory** transfer, with no peripheral involved at
   all — isolates "does DMA itself work" from any ADC/peripheral complexity.
2. A **continuous-mode ADC1 → DMA2 → buffer** pipeline, where the CPU issues
   a single `SWSTART` and then does nothing further until 100 samples are
   already sitting in memory.

The core architectural point this day establishes: DMA does not make the ADC
convert faster — it removes the CPU from the *data-movement* path. The CPU's
per-sample cost (poll `EOC`, read `DR`) drops to zero; the only CPU
involvement per batch is checking one completion flag (or, from Day 3
onward, handling one interrupt).

## Hardware / Wiring

| Signal        | Pin  | Notes                                          |
|---------------|------|-------------------------------------------------|
| ADC1 input    | PA0  | Analog mode. Connect a potentiometer wiper, or a jumper swept between 3.3V and GND, to see the buffer values move. |
| USART2 TX     | PA2  | AF7, connected to onboard ST-Link VCP or external USB-serial adapter |
| USART2 RX     | PA3  | AF7 (unused by this code, configured for completeness) |

`BRR = 365` — verify this matches your actual `APB1` clock and target baud
rate before reuse elsewhere; not re-derived in this session.

## DMA Controller/Stream/Channel Map Used This Day

| Peripheral | Controller | Stream | Channel |
|---|---|---|---|
| ADC1        | DMA2 | Stream0 | Channel0 |

DMA2 is required here specifically because ADC1 sits on **APB2**, and only
DMA2 has bus-matrix reach to APB2 peripherals — DMA1 can only reach APB1 and
cannot do memory-to-memory transfers at all. This is a hardware-fixed fact,
not a configuration choice.

## Register Sequence

### 1. `DMA2_M2M_Test()` — memory-to-memory sanity check

- `RCC->AHB1ENR` — enable `DMA2EN`. **DMA is on AHB1**, a different bus
  family from every peripheral clocked so far (APB1/APB2).
- `DMA2_Stream0->CR`:
  - `DIR = 10` (memory-to-memory)
  - `PINC = 1`, `MINC = 1` — **both** sides increment, since in M2M mode
    neither end is a fixed peripheral register.
  - `PSIZE = MSIZE = word` (arrays are `uint32_t`)
- `PAR` = source array address, `M0AR` = destination array address,
  `NDTR` = 10.
- `EN` set last. Transfer runs at full bus speed with no peripheral
  throttling it, so it may complete before the next instruction — poll
  `TCIF0`/`TEIF0` in `DMA2->LISR` (bits 5 and 3) immediately after enabling.

### 2. `DMA2_ADC1_Init()` — continuous ADC1 sampling via DMA

- `GPIOA->MODER` — PA0 to analog mode (`11`). Easy to miss: without this,
  the ADC samples whatever PA0's prior digital state was, not the intended
  analog signal.
- `ADC1->CR2`:
  - `CONT = 1` — continuous conversion.
  - `DMA = 1` — ADC actually issues DMA requests.
  - `DDS = 1` — **required alongside `CONT`.** Without it, the ADC only
    requests a DMA transfer after the *first* conversion, then stops
    requesting even though it keeps converting — buffer indices past 0
    never update.
  - `ADON = 1` — set last, powers on the ADC.
- `ADC1->SMPR2`, `ADC1->SQR3` — channel 0 selected, slowest sample time.
- `DMA2_Stream0->CR`:
  - `DIR = 00` (peripheral-to-memory)
  - `PINC = 0` — `ADC1->DR`'s address is fixed, must not increment.
  - `MINC = 1` — buffer address must advance each sample.
  - `PSIZE = MSIZE` = halfword — ADC data is 12-bit, right-aligned in a
    16-bit register.
- `PAR = &ADC1->DR`, `M0AR = buffer`, `NDTR = 100`.
- `EN` set last.

### 3. `main()`

- Calls `DMA2_ADC1_Init()` then `USART2_Init()`.
- Triggers `SWSTART` exactly **once** — `CONT` + `DDS` keep the pipeline
  running without further CPU action.
- Polls `DMA2->LISR` for `TCIF0` (bit 5) or `TEIF0` (bit 3) — Stream0's
  status lands in the **Low** interrupt status register (streams 0-3 → LISR,
  streams 4-7 → HISR).
- Once complete, dumps all 100 buffered values over UART.

## Bugs Found & Fixed

- **Dangling stack pointer:** `src`/`dest` (and later `buffer`) initially
  declared as local variables — their addresses became invalid the moment
  the enclosing function returned, since DMA held onto stack addresses that
  were no longer reserved. Fixed by moving to `static` or file scope.
- **`M0AR` truncation:** `M0AR = (uint16_t)buffer` truncated the 32-bit
  address to 16 bits. `M0AR`/`PAR` are always full 32-bit address registers
  regardless of the data width being transferred (that's `MSIZE`'s job, not
  the pointer cast's). Fixed to `(uint32_t)buffer`.
- **Missing `DDS`:** without it, continuous mode only issues one DMA request
  total, leaving `buffer[1..99]` at their initialized value.
- **Missing PA0 analog mode:** `GPIOAEN` alone doesn't configure the pin;
  `MODER` must explicitly be set to `11` for PA0.
- **`buffer` out of scope across files/functions:** initially declared
  inside `main()`, invisible to `DMA2_ADC1_Init()` — compile error. Fixed by
  promoting to file scope.
- **`USART2_Init()` never called:** `printf` hung indefinitely polling
  `TXE` on a disabled USART. Fixed by calling it in `main()` before the
  print loop.

## Debug Notes

- Debug sessions (not plain Run) are required to inspect `buffer` live.
  Use **Expressions view** (Window → Show View → Expressions) rather than
  relying on Variables view auto-detection for a file-scope array — add
  `buffer` as an expression and expand it.
- Break **before** the print loop, not inside it, to see the DMA-filled
  buffer in one shot rather than stepping index-by-index.
- "Target not available" during debug is a connection/session fault
  (stale session, USB power blip, or a genuinely hung poll loop) — fully
  terminate and restart the debug session rather than resuming.
