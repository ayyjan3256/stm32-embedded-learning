# Week 6, Day 5 — UART TX via DMA, Combined ADC+UART Pipeline

## Overview

Days 2-4 freed the CPU from per-sample ADC polling. This day applies the
same idea to the transmit side: instead of `_write()`/`uart_write_byte()`
polling `TXE` and writing `USART2->DR` one character at a time, DMA1
Stream6 is handed a whole buffer and told to transmit it, freeing the CPU
the moment the transfer is issued.

The day closes by joining both halves into one pipeline: ADC1+DMA2
continuously fills a sample buffer in the background; each completed half
is formatted into a CSV-style text string; that string is handed to
USART2+DMA1 for transmission — with no polling loop anywhere in the whole
chain.

## DMA Mapping Used This Day

| Peripheral | Controller | Stream | Channel |
|---|---|---|---|
| USART2_TX | DMA1 | Stream6 | Channel4 |

USART2 sits on APB1, so this transfer runs on **DMA1**, not DMA2 — the
mirror case of ADC1 being DMA2-only, same bus-topology reasoning from Day 1.

## Register Naming — `PAR` vs `M0AR` Doesn't Flip With Direction

`PAR` always holds the **peripheral's** address, `M0AR` always holds the
**memory buffer's** address — regardless of which way data is flowing.
For this transfer: `PAR = &USART2->DR` (peripheral), `M0AR = csv_buf`
(memory). Only `DIR` changes to reflect memory-to-peripheral instead of
Day 2's peripheral-to-memory; the register-to-role assignment never does.

## The Second Enable — `USART_CR3_DMAT`

`USART2->CR3`'s `DMAT` bit tells the USART peripheral itself to actually
issue DMA requests. This is a *USART* register, not a DMA one — setting up
`DMA1_Stream6` perfectly and leaving `DMAT` unset means the stream sits
fully configured but never receives a request to act on. Same category of
easy-to-miss bug as `DDS` on the ADC side.

## Interrupt-Driven Completion

Polling `DMA1->HISR` for `TCIF6` works but blocks the CPU for the entire
transmission time — worse than the ADC case in wasted CPU, since UART bit
times are slow relative to DMA's own transfer speed. The interrupt-driven
version:

- `DMA_SxCR_TCIE` enabled in `DMA1_Stream6->CR`.
- `NVIC_EnableIRQ(DMA1_Stream6_IRQn)` — the peripheral-level enable alone
  is not enough; the NVIC must separately be told to listen for this
  specific interrupt line, or the ISR never executes despite the flag
  setting correctly.
- `DMA1_Stream6_IRQHandler` checks `TCIF6` in `DMA1->HISR`, clears it via
  **`DMA1->HIFCR`** (High Interrupt *Flag Clear* Register — separate from
  `HISR` itself, same split pattern as `LIFCR`/`LISR`), and sets a
  `volatile tx_done` flag. Minimal ISR body — no processing inside it.

## Restarting a Non-Circular Stream Safely

Unlike the ADC's circular buffer, `NDTR` here does **not** auto-reload —
each new transmission requires the sequence: disable `EN`, **wait for
hardware to actually clear it** (`EN` doesn't clear instantaneously —
hardware finishes any in-flight bus transaction first), reload `NDTR` to
the new length, reload `M0AR` if the source address changed, then
re-enable. Reconfiguring `PAR`/`M0AR`/`NDTR` while `EN` is still hardware-
asserted is undefined behavior per RM0090.

## CSV Formatting Without Float-Support Printf

Each `buffer[i]` is a raw 12-bit ADC reading (0-4095), rendered as decimal
text via `sprintf(&csv_buf[pos], "%u,", buffer[i])` — integer-only
formatting, so this sidesteps the toolchain's lack of default float
support entirely (no `print_fixed2`-style workaround needed here, since
nothing here is a float).

## Race Between Formatting and Transmission

`csv_buf` is a single, shared, file-scope buffer used by both the
`half_ready` and `full_ready` paths. The `sprintf` formatting loop only
runs once `tx_done` is confirmed true — otherwise a newly-completed batch
could start overwriting `csv_buf` with fresh digits while DMA is still
mid-transmission reading old bytes out of the same memory, producing
interleaved, corrupted output. Gating the *formatting* step (not just the
DMA re-enable) on `tx_done` is what prevents this.

## Half/Full Boundary Discipline Carries Over From Day 3

Each `half_ready`/`full_ready` branch only ever formats and sends its own
half of `buffer[]` (`[0..47]` or `[48..95]`) — reading the other half would
read data DMA might still be mid-write on, the same torn-read hazard Day 3
established. Because 48 is evenly divisible by 3 (the channel count), each
transmitted batch always begins at a fresh scan boundary — channel0,
channel1, channel4, repeating — with no partial-scan carryover between
batches.

## DMA's Effect Here: CPU Time, Not Peripheral Throughput

USART2 still transmits at the same fixed baud rate (`BRR=365`, ~115200
baud) whether DMA drives it or not — DMA does not make the wire faster.
What changes is that the CPU issues one `EN` toggle and is immediately
free, rather than being blocked polling `TXE` for the full duration of the
transmission. This is the same principle Day 2 established for the ADC
side (DMA doesn't speed up conversion, it frees the CPU from babysitting
it), now confirmed symmetrically on the transmit side for a different
underlying hardware constraint (baud rate instead of conversion time).
