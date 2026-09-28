# Week 6 — DMA: Circular Mode, Double Buffering, and the ADC + UART Pipeline

**Board:** WeAct STM32F405RGT6 Blackpill
**Builds on:** Week 3's multi-channel ADC work, Week 2's UART, Week 4's EXTI button
**Style:** register-level C (CMSIS), no HAL peripheral drivers. HAL is used only for
`HAL_Init()`, `SystemClock_Config()` and `HAL_GetTick()`.

## Overview

Everything built before this week still required the CPU to execute an
instruction to move each piece of data: polling `EOC` and reading
`ADC1->DR`, polling `TXE` and writing `USART2->DR`, even interrupt-driven
code touched every item. This week removes the CPU from that path. Once
configured, a DMA stream moves data between a peripheral and memory with
no instruction executed per item.

The week ends with one pipeline: ADC1 scans three channels continuously,
DMA2 fills two alternating buffers in the background, the main loop
computes per-channel mean and standard deviation on each completed buffer,
and the result is formatted as a CSV line and transmitted by USART2 through
DMA1. There is no polling loop anywhere in the chain, and a button on EXTI
starts and stops the ADC stream without disturbing the UART stream.

## What Each Day Built

| Day | Topic | Result |
|---|---|---|
| 1 | DMA architecture (theory) | Notes on DMA1 vs DMA2 reach, stream vs channel, the three request mappings, and the `DMA_SxCR` fields |
| 2 | First transfers | Memory-to-memory copy on DMA2; single-channel ADC1 → DMA2 Stream0 in normal mode, 100 samples dumped over UART |
| 3 | Circular mode | Continuous background sampling; half/full-transfer flags for race-free reads; 3-channel scan with interleaved samples |
| 4 | Double buffering | `DBM` with `M0AR`/`M1AR`; buffer-ready decision from `CT` in the ISR; mean/std per completed buffer; deliberate slow-processing stress test |
| 5 | UART TX via DMA | USART2 on DMA1 Stream6; `tx_done` gating; combined ADC → CSV → UART pipeline |
| 6 | Integration and errors | `TEIF` handling, deliberate-bug tests, EXTI start/stop control, long stress run |
| 7 | Review | Configs rebuilt from memory, one-page reference sheet, this README |

## Hardware and Pin Map

| Signal | Pin | Notes |
|---|---|---|
| ADC1 channel 0 | PA0 | Analog input |
| ADC1 channel 1 | PA1 | Analog input |
| ADC1 channel 4 | PA4 | Analog input |
| USART2 TX / RX | PA2 / PA3 | AF7, 115200 baud |
| Start/stop button | PB0 | EXTI0, falling edge, internal pull-up |
| Error LED | PB2 | Turns on when a DMA transfer error fires |

Clock tree: PLL to 168 MHz from HSE, AHB /1, APB1 /4 (42 MHz), APB2 /2
(84 MHz).

## DMA Mappings Used This Week

| Peripheral | Controller | Stream | Channel |
|---|---|---|---|
| ADC1 | DMA2 | Stream0 | Channel0 |
| USART2_TX | DMA1 | Stream6 | Channel4 |
| USART2_RX | DMA1 | Stream5 | Channel4 (mapped, not used) |

ADC1 sits on APB2 and its request line is wired to DMA2 only, so DMA1
can never service it. USART2 sits on APB1, which is the DMA1 side. Only
DMA2 can do memory-to-memory transfers, which is why Day 2's first
transfer ran on DMA2.

## Register Naming — `PAR` vs `M0AR` Doesn't Flip With Direction

`PAR` always holds the **peripheral's** address, `M0AR` (and `M1AR`) always
hold **memory** addresses, regardless of direction. For the ADC,
`PAR = &ADC1->DR` and the buffers are the destination; for the UART,
`PAR = &USART2->DR` and `csv_buf` is the source. Only `DIR` changes
(`00` peripheral-to-memory, `01` memory-to-peripheral). `PINC` is `0` in
both cases because the peripheral's data register address never moves;
setting it to `1` would walk DMA past the data register into neighbouring
registers.

## The Two Enables on the ADC Side — `DMA` vs `DDS`

Both live in `ADC1->CR2` and they fail differently:

- `ADC_CR2_DMA` makes the ADC issue DMA requests at all. Without it the
  stream sits configured and never receives a request.
- `ADC_CR2_DDS` keeps the ADC issuing requests after every conversion in
  continuous mode. Without it only the first sample reaches the buffer.

`CONT` keeps conversions running and `ADON` powers the ADC. `SWSTART` is a
trigger, not configuration, so it runs last, after the DMA stream is
enabled, so that no early samples are lost.

## Normal, Circular, and Double-Buffer Mode

| | Normal | Circular | Double buffer |
|---|---|---|---|
| At `NDTR = 0` | Hardware clears `EN`, `TCIF` sets, stream stops | `NDTR` reloads, `EN` stays set | `NDTR` reloads, target swaps `M0AR` ↔ `M1AR`, `TCIF` fires, `EN` stays set |
| Used for | UART TX of one line, single ADC batch | Live monitoring where only the newest N samples matter | Whole-buffer statistics per window |
| Solves | Moving a fixed block with no CPU per item | The gap from re-arming a stream after every batch | Torn reads, by giving a full-size stable buffer while DMA fills the other |
| Does not solve | Continuity between transfers | Overwriting unread data if processing lags, and torn reads without flag gating | Falling behind: if processing is slower than one fill, both buffers are overwritten |

The ADC stream uses double-buffer mode. The UART stream uses normal mode.

## Half/Full Gating (Day 3) and `CT` (Day 4)

In circular mode `HTIF` sets when `NDTR` passes half its reload value and
`TCIF` sets at the wrap. Processing `[0..half)` on `HTIF` and `[half..N)`
on `TCIF` means DMA is always writing the other half, so a read never
lands on a sample being written. Each half is 48 items in the Day 3
buffer, which is divisible by 3 so every half starts on channel 0.

Double-buffer mode replaces the split with two separate buffers.
**Hardware** swaps the target when `NDTR` hits 0 and reloads it; `CT` is a
read-only status bit reporting where DMA is now. The ISR reads it and never
drives it:

- `CT == 1`: DMA is now writing `buffer_b`, so `buffer_a` just finished.
- `CT == 0`: DMA is now writing `buffer_a`, so `buffer_b` just finished.

Each buffer is 99 items (33 conversion sets × 3 channels), again a multiple
of 3, so every buffer begins at channel 0 and the data is interleaved as
`ch0, ch1, ch4, ch0, ch1, ch4, ...`. The ISR only sets a `volatile` ready
flag. All processing happens in the main loop.

## Status Registers Belong to the Controller, Split Low/High

Each stream has six flags, and eight streams need 48 bits. A 32-bit
register cannot hold them, so the flags are split across two registers
holding four streams each: `LISR`/`LIFCR` for streams 0-3 and
`HISR`/`HIFCR` for streams 4-7. DMA2 Stream0 is the first stream of the
low register (`DMA2->LISR`, `TCIF0`) and DMA1 Stream6 is the third stream of
the high register (`DMA1->HISR`, `TCIF6`). These registers belong to the
controller, not the stream. `DMA2_Stream0->LISR` does not exist.

Flags are cleared by writing 1 to the matching bit in the clear register,
using plain assignment (`DMA2->LIFCR = DMA_LIFCR_CTCIF0;`). The NVIC name
must match the handler name: `DMA2_Stream0_IRQn` for
`DMA2_Stream0_IRQHandler`, `DMA1_Stream6_IRQn` for
`DMA1_Stream6_IRQHandler`. A mismatch means the flag sets and the ISR
never runs.

## UART TX via DMA

`USART2->CR3`'s `DMAT` bit tells the USART to issue DMA requests. It is a
USART register, not a DMA one, and leaving it unset means a fully
configured stream never receives a request. The TX stream is one-shot:
`CIRC = 0`, `MINC = 1`, `PINC = 0`, byte-wide `PSIZE`/`MSIZE`, `CHSEL = 4`.
With `CIRC` on, `NDTR` would reload and the stream would retransmit the
buffer forever.

`USART2_Send(data, len)` is the only place a transmission starts:

1. Return immediately if `tx_done` is 0 (previous transfer still running).
2. Confirm `EN == 0`. Hardware clears it when `NDTR` reaches 0, and
   `tx_done` is only set after that.
3. Set `tx_done = 0` before enabling, so the completion ISR cannot be
   overwritten by a late write.
4. Clear stale flags in `DMA1->HIFCR`.
5. Write `M0AR` and `NDTR` (the actual string length), then set `EN` last.

`NDTR` must be reloaded on every send because normal mode leaves it at 0.
Only circular and double-buffer mode reload it in hardware.

`TCIF6` means DMA has handed the last byte to the USART, not that the last
byte has finished shifting out on the wire. Checking `USART2->SR` `TC` is
required if the line-idle moment matters; this pipeline does not need it.

## Race Between Formatting and Transmission

`csv_buf` is a single shared buffer and it is the DMA source while a
transmission is in flight. `process_buffer()` therefore checks `tx_done`
**before** running `sprintf`. If the UART is still busy the buffer is
counted in `dropped_tx` and skipped. Checking only before re-enabling the
stream, after formatting, would let a new batch overwrite `csv_buf` while
DMA is still reading old bytes from it, producing interleaved, corrupted
output.

## Error Handling

`TEIE` is enabled on both streams, and both ISRs check `TEIF`. A transfer
error clears `EN` in hardware, so the stream is stopped when the handler
runs. The handlers clear the flag, set `dma_error`, and turn the PB2 LED on.
The UART handler also sets `tx_done` so transmission can recover, since the
aborted transfer will never raise `TCIF`.

Day 6 included a deliberate-bug exercise (an invalid `PAR`, or `NDTR = 0`
before enabling) to confirm the handler fires. A stalled-but-not-errored
stream looks different from a correctly idle one: stalled means `EN = 1`
with `NDTR` stuck at a non-zero value and `TCIF` never setting again, while
a correctly finished normal-mode stream has `EN = 0` and `NDTR = 0`.

## Start/Stop Control

The button ISR (PB0, EXTI0) debounces against `HAL_GetTick()` at 50 ms and
toggles a `volatile adc_running` flag. It does no DMA work itself. The main
loop notices the change and runs one of two routines.

**Stop:** clear `ADON`, clear the stream's `EN`, wait until `EN` reads 0
(hardware finishes any in-flight transfer first), and clear the ready
flags. The UART stream is untouched.

**Restart:**

1. Clear stale flags in `DMA2->LIFCR`.
2. Clear `CT` (writable only while `EN = 0`) so the first target is `M0AR`.
3. Reload `NDTR`, which holds a partial count after a manual stop.
4. Clear `ADC1->SR` and toggle the ADC `DMA` bit so requests restart after
   any overrun.
5. Set the stream's `EN`, then `ADON`, wait a short `tSTAB`, then
   `SWSTART` last.

Powering the ADC off and on also restarts the scan sequence at the first
channel, keeping the buffer aligned to channel 0.

## Throughput — Calculated From the Configuration

These figures follow from the register settings, not from a logic-analyser
capture.

| Quantity | Value |
|---|---|
| ADC clock | 84 MHz / 4 = 21 MHz (the default /2 gives 42 MHz, above the 36 MHz limit) |
| Sample time | 480 cycles + 12 conversion = 492 cycles = 23.4 µs per conversion |
| One 3-channel set | about 70 µs (about 14.2 kHz per channel) |
| One 99-item buffer | 33 sets, about 2.3 ms to fill |
| UART rate | 115200 baud, 10 bits per byte = 11,520 bytes/s |
| Worst-case raw CSV line | 99 × `"4095,"` = 495 bytes, about 43 ms to send |

The UART is roughly 18 times slower than the ADC fill rate in raw mode, so
most buffers arrive while the previous line is still being sent, and the
main loop drops them and counts them in `dropped_tx`. This is the
fell-behind case from Day 4, appearing on the transmit side. DMA does not
make the wire faster. It frees the CPU from waiting on it. Setting
`OUTPUT_STATS` to `1` sends `mean,std` per channel instead: a line of at
most 30 bytes, about 2.6 ms, which is close to the buffer fill time.

## Files

| File | Purpose |
|---|---|
| `main.c` | Complete final pipeline: ADC1 + DMA2 double-buffer, USART2 + DMA1, EXTI start/stop, error handlers |
| `reference-sheet.md` | One-page cheat sheet: clock enables, `SxCR` field map, status/clear registers, mode comparison |

## Build and Run

1. Build the CubeMX project with `main.c` in `Core/Src` and flash the
   Blackpill (SWD or DFU).
2. Open the serial port at **115200 8N1**. A hello line prints first, then
   one CSV line per transmitted buffer. Values are 12-bit raw readings in
   the order channel 0, 1, 4.
3. Press the PB0 button to stop and restart ADC sampling. The UART keeps
   running independently.
4. In a debugger, watch `dropped_tx`, `overrun_count`, `dma_error`,
   `ch_mean[]` and `ch_std[]`.

`#define OUTPUT_STATS` selects the output format: `0` for raw samples
(one line per buffer, compatible with the Week 3 live-plot script), `1` for
per-channel mean and standard deviation.

## Known Limitations

- **UART-bound output.** In raw mode most buffers are dropped, as
  calculated above. The ADC side is unaffected; only the transmit side
  skips.
- **Fell-behind is detected, not prevented.** `overrun_count` increments
  when both buffers are ready at once. Double buffering removes torn reads
  but not the requirement that processing keeps pace with the fill rate.
- **Errors stop the stream.** A DMA transfer error clears `EN` in hardware.
  The LED and `dma_error` report it, but nothing restarts the ADC stream
  automatically; the button stop/start cycle restores it.
- **Clock assumption.** `BRR = 365` assumes PCLK1 = 42 MHz, which follows
  from the PLL configuration in `SystemClock_Config()`. Changing the clock
  tree means recomputing `BRR`.

## Lessons From This Week

- ADC1 is on APB2 and is serviced by DMA2 only; the request line is wired
  that way.
- `DMA` makes the ADC issue requests, `DDS` keeps it issuing them, and
  `SWSTART` runs last.
- `PINC = 0` for a fixed data register, and `PAR` holds the peripheral
  address in either direction.
- `NDTR` auto-reloads in circular and double-buffer mode, and must be
  reloaded manually for a one-shot transfer.
- Status and clear registers belong to the controller, streams 0-3 in the
  low register and 4-7 in the high, and the NVIC name must match the
  handler name.
- `CT` is a status bit. Hardware does the swapping.
- Masks should say what they mean: `~(1<<9)` for `PINC`, not `~(3<<9)`,
  which also clears `MINC`.
- Gate the *formatting* step on `tx_done`, not only the DMA re-enable.
- `volatile` flags must be declared before the ISR that uses them.

## Next

Week 7 builds on this pipeline. Week 8's capstone feeds windows of
DMA-collected samples to the TinyML model in real time, using the
double-buffer structure built here.
