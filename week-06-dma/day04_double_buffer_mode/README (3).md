# Week 6, Day 4 — Double Buffering (Ping-Pong Mode)

**Board:** WeAct STM32F405RGT6 Blackpill
**Builds on:** Day 3's circular, multi-channel ADC1+DMA2 pipeline

## Overview

Day 3 solved the torn-read problem by logically splitting one buffer into
two guarded halves (`HTIF`/`TCIF`). This day solves the *same* problem with
a genuinely different mechanism: **two entirely separate, full-size
buffers**, with DMA automatically swapping between them once each one
completely fills. Instead of "half a buffer is now safe," the guarantee
becomes "an *entire* buffer is now safe" — no boundary math, no partial
data, ever.

This matters most for processing that needs the **whole** dataset at once
to be correct (not just faster) — a computation like standard deviation
needs to see every sample together; Day 3's half-at-a-time scheme can
compute a mean per half but can't cleanly compute one statistic across an
artificially split dataset without extra bookkeeping.

## Double Buffer Mode Mechanism

- **`M0AR` / `M1AR`** — two independent memory addresses, not one buffer
  and its second half. DMA fills `M0AR` completely, one full `NDTR` count,
  then automatically switches to `M1AR`, then back, forever.
- **`DBM`** (`DMA_SxCR`) — enables this mode. Requires **`CIRC` set
  underneath** — DBM layers on circular mode, it doesn't replace it.
- **`CT`** (`DMA_SxCR`, read-only) — tells you which buffer DMA is
  *currently* writing: `CT=0` → writing `M0AR`, `CT=1` → writing `M1AR`.
  This single bit replaces Day 3's `HTIF`/`TCIF` half-tracking entirely.

## Buffer/NDTR Sizing — Why 99, Unlike Day 3's 96

In double buffer mode, `TCIF` fires once per **full** buffer completion —
there is no half-transfer concept, so `HTIE` is not used at all this day.
That removes Day 3's "must also be divisible by 2" constraint. The only
requirement left is **divisible by the channel count** (3), so each buffer
holds a whole number of complete scans. 99/3 = 33 clean scans — no
half-split concern, so 99 works here even though it didn't for Day 3.

## Register Sequence — `DMA2_ADC1_Init()`

Builds on Day 3's ADC1 scan-mode config unchanged. DMA-side changes:
- `DMA_SxCR_TCIE` only — **not** `HTIE`, since half-transfer has no meaning
  in double buffer mode.
- `DMA_SxCR_CIRC` — still required, DBM depends on it.
- `DMA_SxCR_DBM` — enables the two-buffer swap.
- `M0AR = buffer_a`, `M1AR = buffer_b` — two full, independent, file-scope
  arrays of matching size (same dangling-pointer risk as every DMA buffer
  this week if declared locally).

## ISR — `DMA2_Stream0_IRQHandler`

```c
if (DMA2->LISR & DMA_LISR_TCIF0)
{
    DMA2->LIFCR |= DMA_LIFCR_CTCIF0;

    if (DMA2_Stream0->CR & DMA_SxCR_CT)
        buffer_a_ready = 1;   // CT==1: DMA just switched TO b, so a just finished
    else
        buffer_b_ready = 1;   // CT==0: DMA just switched TO a, so b just finished
}
```

**The inversion to get right:** `CT` reports where DMA is *headed*, not
where it just was. Reading `CT` inside the ISR tells you the *new* target —
the buffer that just completed is always the **other** one. Same class of
one-step-back reasoning as Day 3's `HTIF`/`TCIF` checkpoint question.

## Per-Buffer Processing — `process_buffer()`

Computes mean and standard deviation over an entire completed buffer in one
pass — the Week 3 anomaly-signal concept, now fed by a fully CPU-idle DMA
pipeline instead of polling. Uses the established fixed-point-over-UART
print pattern (`%d.%02d`), since the toolchain's `printf` lacks float
support by default; `sqrtf` requires `<math.h>`.

## Deliberate Stress Test (Day 4 Hour 6)

`process_buffer()` includes an intentional `HAL_Delay(2000)` — long enough
to outlast DMA's fill time for the *other* buffer at this sample rate. This
was added specifically to observe double buffering's real limitation:

**Double buffering solves ownership (which addresses are safe to touch),
not throughput (whether the consumer keeps up with the producer).** If
processing one buffer takes longer than DMA takes to fill the next one, the
consumer starts missing or reprocessing stale batches — silently, with no
error flag anywhere. This is a structurally separate failure mode from a
torn read, and no DMA configuration (circular or double-buffered) has any
mechanism to slow the incoming stream to match a slow consumer.

**Remove the `HAL_Delay(2000)` for normal operation** — it is left in this
version specifically to demonstrate the stress-test symptom, not as
production behavior.

## Known Bugs Found & Fixed This Session

- Earlier draft mismatched buffer size (99) against `NDTR` (100) — an
  off-by-one buffer overflow risking corruption of adjacent variables
  (specifically the `_ready` flags, which sat right next to the buffer in
  memory — a bug that would have looked like unexplained flag behavior
  rather than an obvious memory fault). Fixed by matching `NDTR` and buffer
  length exactly (99/99 here, since DBM has no half-split constraint).

## Status

Double-buffer swap confirmed via `CT`-driven ISR logic. Mean/std computed
correctly per completed buffer. Stress test intentionally included to
demonstrate the falling-behind failure mode discussed above — expect
visibly stale or infrequent output while `HAL_Delay(2000)` remains in
`process_buffer()`.

## Next

Day 5 — UART TX via DMA, freeing the CPU from byte-by-byte transmission the
same way Days 2-4 freed it from ADC polling.
