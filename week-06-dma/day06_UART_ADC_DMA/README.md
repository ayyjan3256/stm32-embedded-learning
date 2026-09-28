# Week 6, Day 6 — Pipeline Integration and DMA Error Handling

**Board:** WeAct STM32F405RGT6 Blackpill
**Builds on:** Day 4 (double-buffered ADC1+DMA2) and Day 5 (USART2 TX via DMA1)

## Overview

This day joins the two independently proven DMA pipelines into one running
project, adds DMA error signaling, and adds a button-driven start/stop
control for the ADC stream.

```
ADC1 (scan ch0, ch1, ch4) -> DMA2 Stream0 (double buffer) -> buffer_a / buffer_b
        -> CSV formatting (sprintf) -> csv_buf -> DMA1 Stream6 -> USART2 TX
```

The CPU only formats text and toggles flags between two DMA engines. No
polling loop exists anywhere in the sampling or transmit path.

## Hardware / Pin Map

| Function | Pin | Notes |
|---|---|---|
| ADC ch0 / ch1 / ch4 | PA0 / PA1 / PA4 | Analog mode |
| USART2 TX / RX | PA2 / PA3 | AF7; RX unused by this pipeline |
| Error LED | PB2 | Driven directly from DMA error ISRs |
| Button (KY-004) | PB0 | EXTI0, pull-up, falling edge |

PA0 is an ADC channel, so the button cannot share it: analog mode and a
digital EXTI input on the same pin conflict.

## FIFO Control (`DMA_SxFCR`)

Each stream has an optional 4-word FIFO for burst transfers and for
reconciling mismatched `PSIZE`/`MSIZE`. Both pipelines here use matched
widths (halfword/halfword for the ADC, byte/byte for UART) and no bursts, so
the streams stay in **direct mode** (`DMDIS = 0`) and the FIFO is bypassed.

A genuine mismatch would look like SPI presenting byte-wide data while memory
wants four bytes packed per `uint32_t` slot: `PSIZE = byte`, `MSIZE = word`,
with the FIFO accumulating four peripheral-side transfers per memory write.

## DMA Error Handling

- **`TEIF`** sets when DMA makes an invalid or misaligned access (bad `PAR` /
  `M0AR`, or an address unaligned for the configured `PSIZE`/`MSIZE`).
- `DMA_SxCR_TEIE` is enabled on both active streams (`DMA2_Stream0`,
  `DMA1_Stream6`).
- Each ISR clears its own `TEIF` through the clear register
  (`LIFCR` for Stream0, `HIFCR` for Stream6). An uncleared flag leaves the
  ISR re-triggering.
- The handler toggles an LED (PB2) rather than sending a UART message: if
  the UART/DMA path is what failed, reporting through that same path is
  circular. A direct GPIO write depends on nothing that could itself be
  broken.

**Fault-injection test:** temporarily setting
`DMA2_Stream0->PAR = 0xFFFFFFFF` produced the expected `TEIF0`, and the LED
lit. Restore `PAR = &ADC1->DR` afterward.

## Double-Buffer Ownership (from Day 4)

`DMA2_Stream0` runs with `CIRC`, `DBM`, and `TCIE`. `TCIF0` fires once per
completed buffer. `CT` reports where DMA is *headed*, so inside the ISR the
buffer that just finished is the opposite one:

- `CT == 1` (now writing `buffer_b`) -> `buffer_a` is ready
- `CT == 0` (now writing `buffer_a`) -> `buffer_b` is ready

## UART Transmit Path (from Day 5)

- `USART2->CR3` `DMAT` enables the USART's DMA requests.
- `DMA1 Stream6 / Channel 4` (`CHSEL = 4`), memory-to-peripheral, byte width.
- `sprintf` runs only inside the `tx_done` gate, so `csv_buf` is never
  overwritten while DMA is still reading it out.
- A restart follows the sequence: clear `EN`, wait for hardware to actually
  clear it, set `M0AR` and `NDTR`, set `EN`.
- At 115200 baud a ~500-character CSV line takes roughly 43 ms to send, while
  a 99-sample buffer fills in roughly 1 ms. The UART is saturated, so most
  completed buffers are dropped by the `tx_done` gate. The wire runs
  continuously at its baud limit.

## Startup Message

`USART2_DMA1_Init()` starts a one-time transmission of `message`. The ADC
pipeline is much faster than the UART, so without a pause the first
buffer-ready event would reconfigure `DMA1_Stream6` and truncate `message`
mid-transmission. `tx_done` is set to 0 while the startup message is in
flight, and a one-time `HAL_Delay(2000)` before `SWSTART` lets it finish.
This delay runs once at boot, not in the steady-state loop.

## Button Start/Stop

- The EXTI0 ISR debounces against `ms_ticks` (50 ms) and toggles
  `adc_running`. The ISR does no DMA work itself.
- The main loop detects the *change* in `adc_running` (edge, not level).
  Stop: clear `EN` on `DMA2_Stream0` and wait for it to actually clear.
  Start: reload `NDTR`, set `EN`, re-issue `SWSTART`.
- `ms_ticks` is incremented in `SysTick_Handler` in `stm32f4xx_it.c`, next to
  the existing `HAL_IncTick()` call (which `HAL_Delay()` depends on). The
  manual `SysTick_Init()` uses `LOAD = 167999` for a 1 ms tick at 168 MHz.
