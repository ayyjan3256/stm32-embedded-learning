# Week 6, Day 3 — Circular Mode & Race-Free Multi-Channel Sampling

**Board:** WeAct STM32F405RGT6 Blackpill
**Builds on:** Day 2's ADC1+DMA2 peripheral-to-memory pipeline

## Overview

Day 2's pipeline sampled 100 values once and stopped. This day makes it run
**forever**, refilling the same buffer continuously with zero CPU
intervention after the initial `SWSTART` — and adds a second sensor
dimension: multi-channel scan mode, with DMA writing interleaved
channel samples round-robin into one buffer.

The central problem this day solves: once a buffer is being continuously
overwritten by DMA in the background, how do you read from it without ever
reading data DMA is mid-write on (a **torn read**)? The answer is the
half-transfer / transfer-complete flag pair (`HTIF`/`TCIF`), which gate
access to exactly the half of the buffer DMA has *just finished*, never the
half it's currently touching.

## Circular Mode

Setting `CIRC` in `DMA_SxCR` changes the post-`NDTR=0` behavior: instead of
disabling the stream (Day 2), `NDTR` auto-reloads to its original value and
the stream keeps running indefinitely, wrapping back to the start of the
buffer. Combined with `ADC1->CR2`'s `CONT`+`DDS` (already required from
Day 2), the whole pipeline runs forever with a single `SWSTART`.

**Tradeoff to be explicit about:** circular mode silently overwrites the
oldest sample once the buffer wraps. Fine for live monitoring (only the
current value matters); dangerous for anything that must not lose data
(e.g. a full audio recording, start to finish) — a slow consumer doesn't
error, it just silently loses samples with nothing to catch it.

## Multi-Channel Scan Mode

Three ADC1 channels sampled in rotation, interleaved into one buffer by
DMA in the exact sequence order defined by the sequence registers.

| Signal | Pin | ADC Channel | Scan Position |
|---|---|---|---|
| ch0 | PA0 | 0 | 1st |
| ch1 | PA1 | 1 | 2nd |
| ch4 | PA4 | 4 | 3rd |

- `ADC1->CR1` `SCAN=1` — without this, the ADC ignores the sequence
  registers entirely and just reconverts a single channel.
- `ADC1->SQR1` bits `L[3:0] = count − 1` → `L = 2` for 3 channels.
- `ADC1->SQR3` — 5 bits per sequence position: position1=ch0,
  position2=ch1, position3=ch4.
- Each additional analog pin (PA1, PA4) needs its own `GPIOA->MODER` bits
  set to analog mode — easy to forget once PA0 alone already "works."

Verified on hardware: printing `buffer[i]` and grouping by `i % 3` showed
three visibly distinct signal characters (one tightly clustered ~2030-2070,
one swinging the full 0-4095 range, one moderately drifting) — clear
evidence the three channels are landing in their correct, repeating slots
rather than being scrambled together.

## Buffer/NDTR Sizing — Why 96, Not 99 or 100

Two constraints must hold simultaneously:

1. **Divisible by 3** — so a full buffer (`NDTR` samples) contains a whole
   number of complete 3-channel scans, not a partial scan at the end.
2. **Divisible by 2** — so `NDTR`'s half (where `HTIF` fires) is a clean,
   round number, not a fraction.

99 satisfies (1) but not (2) — it's odd, so "half" isn't a whole number,
and hardware flooring the half-point lands `HTIF` mid-scan anyway (49 isn't
divisible by 3), even though the *whole* buffer was clean. **96 satisfies
both:** 96/3 = 32 scans (whole buffer), 96/2 = 48, and 48/3 = 16 scans
(half buffer) — both boundaries land on complete scans. Any multiple of 6
works for the same reason.

## Register Sequence — `DMA2_ADC1_Init()`

Builds on Day 2's config, with these additions:
- `DMA_SxCR_HTIE`, `DMA_SxCR_TCIE` — enable both interrupt sources.
- `DMA_SxCR_CIRC` — auto-reload `NDTR` instead of stopping.
- `NVIC_EnableIRQ(DMA2_Stream0_IRQn)` — required separately from the
  peripheral-level enable bits; without it the flags still set in `LISR`
  but the ISR function never runs.
- `NDTR = 96`, `buffer[96]` sized to match exactly.

## ISR — `DMA2_Stream0_IRQHandler`

```c
void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_HTIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CHTIF0;  // clear via LIFCR, a separate register
        half_ready = 1;
    }
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
        full_ready = 1;
    }
}
```

- **Flags are cleared in `DMA_LIFCR`, a register distinct from `LISR`
  itself** — writing 1 to the matching bit there clears the status bit.
  Skipping this makes the ISR fire once and then re-fire forever.
- ISR does the minimum possible: set a flag, nothing else. All real
  processing happens in the main loop.
- `half_ready`/`full_ready` are `volatile` — without it, the compiler may
  cache a stale read in a loop that checks them, hanging forever despite
  the ISR firing correctly.

## Race-Free Reasoning

`HTIF0` fires when `NDTR` counts from 96 down to 48 — meaning DMA has just
**finished** writing indices 0-47 and moved on to 48-95. So 0-47 is safe to
read *right now*, precisely because DMA is structurally guaranteed to be
elsewhere. Symmetrically, `TCIF0` (full wrap) means 48-95 just finished and
DMA is back at 0-47. Each flag tells you which half was *just* completed —
always the opposite half of wherever DMA is currently writing — which is
what makes the read structurally race-free rather than just usually-safe.

## Known Bugs Found & Fixed This Session

- **Buffer/`NDTR` mismatch:** original code had `buffer[99]` but
  `NDTR = 100` — DMA would have written one element past the array's end,
  into adjacent memory (in this case, directly adjacent to the
  `half_ready`/`full_ready` flags), risking silent corruption disguised as
  bizarre flag behavior. Fixed by matching both to 96 (see sizing section
  above).
