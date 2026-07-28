# Week 1 Reference Sheet — STM32F405, Bare-Metal GPIO + SysTick

Board: WeAct STM32F405RGT6 Blackpill. Test LED wired on **PB2** (not PC13).

---

## Memory Map Essentials

| Region | Base address |
|---|---|
| Flash | `0x08000000` |
| SRAM | `0x20000000` |
| Peripheral base | `0x40000000` |
| AHB1 base (GPIO, DMA) | `PERIPH_BASE + 0x00020000` |
| GPIOB base | `0x40020400` |
| GPIOC base | `0x40020800` |
| RCC base | `0x40023800` |
| SysTick base (core, not AHB) | `0xE000E010` |

---

## Clock Tree

```
HSI (16MHz) or HSE (crystal) -> [PLL, optional] -> SYSCLK -> AHB prescaler -> HCLK
                                                              -> APB1 prescaler -> APB1 peripherals
                                                    HCLK -> APB2 prescaler -> APB2 peripherals
```

- Every peripheral clock is **off by default**. Writes to an unclocked peripheral are silently ignored — no fault, no error, address is valid but peripheral logic isn't powered.
- **AHB1** = fast bus, core-adjacent (GPIO, DMA) — needs to be fast since pin I/O should be immediate.
- **APB1 / APB2** = slower, lower-power buses (USART, I2C, SPI, most timers).

---

## GPIO Register Cheat Sheet (per pin `n`)

| Register | Purpose | Bit(s) for pin n |
|---|---|---|
| MODER | mode: `00`=input, `01`=output, `10`=alt func, `11`=analog | `[2n+1 : 2n]` |
| OTYPER | 0=push-pull, 1=open-drain | `[n]` |
| BSRR | atomic set/reset (write-only) | set=`[n]`, reset=`[n+16]` |
| ODR | current output state (read-modify-write) | `[n]` |

**RCC AHB1ENR bit → port:** bit 0 = GPIOA, bit 1 = GPIOB, bit 2 = GPIOC (sequential by letter).

**Bit patterns (the entire vocabulary for register work):**
```c
reg |= (1 << n);            // set bit n, leave others untouched
reg &= ~(1 << n);           // clear bit n, leave others untouched
if (reg & (1 << n)) { }     // check bit n, no modification
reg |= (1<<a) | (1<<b);     // set multiple bits
reg &= ~((1<<a) | (1<<b));  // clear multiple bits
```

---

## Why BSRR Beats ODR

ODR is **read-modify-write**: read the whole register, modify one bit in a CPU-side copy, write the whole thing back — three steps with a gap. If an interrupt fires in that gap and also touches ODR, the original code's stale write-back can silently undo the interrupt's change (race condition).

BSRR is a **single atomic write**: writing 0 to any bit means "leave that pin alone," not "clear it." No read step, so no window for interference. Always prefer BSRR for single-pin writes.

**Open-drain use case:** shared multi-master buses (I2C). Every device can only pull the line low or release it (float high via a shared pull-up) — no two devices can ever short the line by fighting over high/low simultaneously. Push-pull can't guarantee this safety on a shared bus.

---

## `volatile`

Required on any pointer/struct member mapped to hardware. Without it, the compiler may reorder, coalesce, or delete accesses — it assumes "nothing reads this location again, so the write is pointless," which is true for RAM but false for hardware (each access can cause a physical side effect, or the value can change on its own from the hardware side).

Confirmed via CMSIS: `__IO` in ST's struct definitions expands to `volatile`.

Proven directly by compiling on godbolt.org at `-O2`: without `volatile`, multiple sequential writes to the same address collapse to one (or vanish); with `volatile`, all writes appear in the assembly, in order.

---

## CMSIS Is Not Magic

```c
GPIOC->MODER
```
resolves to exactly:
```
((GPIO_TypeDef*)GPIOC_BASE)->MODER  =  GPIOC_BASE + offset_of(MODER in struct)
                                     =  0x40020800 + 0x00
```
Same numbers you calculate by hand from RM0090 — CMSIS just chains `#define`s (`PERIPH_BASE` → `AHB1PERIPH_BASE` → `GPIOC_BASE`) and uses struct member order to encode offsets, instead of a manual lookup table.

---

## SysTick

- Registers: **CTRL** (control/status), **LOAD** (reload value), **VAL** (current count).
- Writing to VAL clears it to 0 and clears COUNTFLAG (documented hardware side effect).
- Counts down from LOAD to 0, auto-reloads, sets **COUNTFLAG** (CTRL bit 16) on reaching 0.
- **Reading CTRL clears COUNTFLAG.** A polling loop must do a fresh live read every iteration — a saved copy will never update, since the clear-on-read already happened at the moment of the copy.

**LOAD formula:**
```
LOAD = (SYSCLK_Hz / ticks_per_second) - 1
```
Off-by-one because counting `LOAD, LOAD-1, ..., 1, 0` inclusive is `LOAD + 1` total cycles, not `LOAD`.

At 16 MHz HSI, 1ms tick: **LOAD = 15999**.

CTRL bits used: `ENABLE` = bit 0, `CLKSOURCE` = bit 2 (1 = processor clock), `TICKINT` = bit 1 (interrupt enable — left off, polling-only this week).

Max single countdown at 16 MHz (24-bit LOAD, max `2^24 - 1`): ~1.05 seconds before overflow. Chain `delay_ms()` calls for longer delays.

**Note:** RM0090's SysTick coverage (§10.5) is thin since it's ARM core architecture, not ST silicon. CMSIS `core_cm4.h` and the ARM Cortex-M4 Generic User Guide / PM0214 are better references for exact bit layout.

---

## Boot Sequence — Power-On to `main()`

1. **Hardware** (before any instruction executes): reads vector table position 0 → loads into **SP**. Reads position 1 → loads into **PC**, jumps there.
2. **`Reset_Handler`** runs: re-asserts SP (`ldr sp, =_estack`) — redundant with step 1, purely defensive/self-documenting.
3. Calls **`SystemInit()`** — FPU enable, optional external-memory/vector-table-offset setup (both no-ops on this board's config).
4. Copies **`.data`** from Flash (LMA, `_sidata`) to RAM (VMA, `_sdata`→`_edata`) — word-by-word read+write loop. Needed because initialized globals must be *writable* (RAM) but must *start with a specific value that survives power-off* (stored in Flash) — no single memory type provides both.
5. Zeroes **`.bss`** (`_sbss`→`_ebss`) — write-only loop, no source address, since C guarantees uninitialized globals start at 0.
6. Calls **`__libc_init_array`** — C/C++ runtime step (static constructors; mostly inert for plain C).
7. Calls **`main()`**. By this point: stack valid, all globals correctly initialized, clock baseline clean.

**Interview-ready one-liner:** *"A global variable has a correct initial value the first time main() reads it because Reset_Handler copies .data from Flash to RAM and zeroes .bss, before main() is ever called — unconditionally, on every reset."*

---

## NVIC (conceptual — hands-on starts Week 4)

**Vectored:** each interrupt source has a dedicated vector table slot; hardware jumps directly to the correct handler via lookup — no software search needed. Faster than a hypothetical single generic handler that checks flags sequentially (`if/else` chain).

**Nested:** priority levels let a higher-priority interrupt preempt a currently-running lower-priority handler, then resume the original afterward.

---

## Linker Script (`.ld`)

- **MEMORY block:** defines physical regions — `FLASH: ORIGIN = 0x08000000`, `RAM: ORIGIN = 0x20000000`.
- **SECTIONS block** maps logical program parts to those regions:
  - `.isr_vector` — vector table, placed first in Flash at `0x08000000`.
  - `.text` — compiled code (all functions).
  - `.data` — initialized globals. Needs **two addresses**: LMA (Flash, permanent initial value) and VMA (RAM, writable runtime location).
  - `.bss` — uninitialized/zero globals. RAM only, no Flash storage needed.

---

## Function Placement in CubeIDE-Generated `main.c`

Your own function **definitions** must live at **file scope**, outside `main()`. Valid spots:
- `USER CODE BEGIN 0` (before `main`), or
- `USER CODE BEGIN 4` (after `main`)

Function **prototypes** (declarations) go in `USER CODE BEGIN PFP`, before `main()` — required for any function called before its full definition appears further down the file.

**Common mistake to avoid:** defining a function *inside* `main()`'s body (e.g., inside `USER CODE BEGIN 1` or `USER CODE BEGIN Init`) is invalid nested function definition in C — even if it happens to compile/run inconsistently, move it out to file scope.

---

## Board-Specific Notes

- Test LED: **PB2**, not PC13. Confirm active-high vs active-low behavior if reusing this pin for new status indicators later.
- ST-Link debug reset vs true power-on reset can leave stale peripheral register state (e.g., SysTick TICKINT) between flashes — when debugging something timing-related, a full USB unplug/replug is a stronger test than a debugger-triggered reset.
