# STM32F405 Bare-Metal LED Blink (PC13) — Extended

Extends the original `blink_raw.c` / `blink_cmsis.c` pair with three more
files covering Days 4, 6, and 7 of Week 1. Same board, same LED, same
memory-mapped-I/O philosophy — see the original README.md for the base
address derivations (RCC_AHB1ENR, GPIOC_MODER, GPIOC_BSRR) referenced
throughout.

## Files

| File | Day | Covers |
|---|---|---|
| `blink_raw.c` | 1–3 | Raw-address GPIO blink |
| `blink_cmsis.c` | 1–3 | Same, via CMSIS structs |
| `blink_day4_breakage_demos.c` | 4 | Working 3-blink-pause pattern + documented deliberate failures |
| `blink_day6_systick_delay_raw.c` | 6 | Raw-address SysTick-based `delay_ms()`, replaces HAL_Delay |
| `blink_day6_systick_delay_cmsis.c` | 6 | Same, via CMSIS `SysTick->` struct |
| `blink_day7_rebuild.c` | 7 | Full from-memory rebuild — the Week 1 capstone |

## Day 4 — Controlled failure

`blink_day4_breakage_demos.c` runs a working "3 fast blinks, then a long
pause" pattern built from `GPIOC->BSRR` + `HAL_Delay`. Below the working
code, three experiments are documented as commented-out single-line
changes, each with the actual observed hardware behavior and the
underlying reason:

1. **Remove the clock enable** — LED goes dark, no error. Proves
   peripheral writes to an unclocked GPIO port are silent no-ops, not
   faults — the address is valid, the peripheral logic simply isn't
   powered.
2. **Wrong MODER encoding** (`10` Alternate Function instead of `01`
   Output) — LED goes dark. Proves BSRR writes have no effect once the
   pin is routed away from GPIO output duty.
3. **HAL_Delay + raw registers coexisting** — confirms HAL and
   bare-metal register access aren't mutually exclusive; they're
   independent mechanisms (HAL_Delay depends on SysTick configured
   inside `HAL_Init()`, GPIO writes don't depend on HAL at all).

## Day 6 — Real millisecond timing

The two `blink_day6_systick_delay_*.c` files replace `HAL_Delay()` (and
the even cruder `for(volatile int i...)` busy-loop from Days 1–3) with a
hand-written `SysTick_Init()` / `delay_ms()` pair, driven directly by the
Cortex-M4 core's SysTick timer rather than a compiler/optimization-level-
dependent loop.

**Why this matters:** the busy-loop delay from earlier days ties timing
to "however many cycles this loop happens to take on this exact build" —
change compilers, optimization flags, or clock config, and the same
delay constant blinks at a different rate. SysTick ties the delay to an
actual hardware-counted unit of time (milliseconds), independent of any
of that.

**LOAD value derivation** (16 MHz HSI, 1ms tick):
```
LOAD = (SYSCLK_Hz / ticks_per_second) - 1
     = (16,000,000 / 1000) - 1
     = 15999
```
The `-1` is a genuine off-by-one, not decoration: SysTick counts
`LOAD, LOAD-1, ..., 1, 0` before reloading, which is `LOAD + 1` total
cycles — so `LOAD` itself must be one less than the raw cycle count you
want.

**Polling correctness note:** `SysTick->CTRL`'s COUNTFLAG bit (bit 16)
is cleared by hardware the instant it's read. `delay_ms()` must perform
a fresh, live read of `SysTick->CTRL` on every loop iteration — caching
the value in a local variable would freeze it at whatever COUNTFLAG
happened to be at that one instant, since the clearing side effect
already fired on the read that produced the cached copy.

**Raw vs. CMSIS, same tradeoff as Day 1:** `blink_day6_systick_delay_raw.c`
uses `#define`d raw addresses (`0xE000E010` etc.) for SysTick specifically
to make the register-level mechanics explicit; `_cmsis.c` uses
`SysTick->CTRL/LOAD/VAL` from `core_cm4.h`, which resolves to the exact
same addresses at compile time.

**Reference note:** SysTick is ARM Cortex-M4 core architecture, not
STM32-specific silicon — RM0090 (§10.5) documents it only lightly. CMSIS's
`core_cm4.h` and ARM's Cortex-M4 Generic User Guide / PM0214 are more
complete references for its exact bit layout.

## Day 7 — Rebuild from memory

`blink_day7_rebuild.c` is the Week 1 capstone: written from a blank
CubeIDE project with no code reference open, RM0090 permitted only for
individual bit-position lookups. It combines every piece from the week —
clock-gated peripheral enable, direct MODER/BSRR manipulation, and a
SysTick-backed `delay_ms()` — into one clean, from-scratch file with zero
HAL GPIO calls and zero compiler-dependent busy-loop timing.

This file represents the target level of independent recall for the
whole week: given just a blank project and the reference manual for bit
positions, reproduce a fully working, correctly-timed bare-metal blink
without consulting prior code.
