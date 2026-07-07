# STM32F405 Bare-Metal LED Blink (PC13)

Two implementations of the same LED blink on an STM32F405 (LQFP64, PC13 LED),
written two ways to show the same operation at two levels of abstraction:

- **`blink_raw.c`** — direct writes to physical memory addresses, computed by
  hand from the reference manual (RM0090). No CMSIS, no HAL.
- **`blink_cmsis.c`** — the same three operations using ST's CMSIS peripheral
  structs (`RCC->`, `GPIOC->`). Functionally identical to the raw version —
  same addresses, same bits, just named instead of typed as hex.

## Why this exists

On this chip, program registers aren't a separate abstraction — they're
addresses in the same flat memory space as RAM and flash
(memory-mapped I/O). Writing to `0x40020818` *is* writing to the GPIOC
output-set/reset latch; there's no OS, driver, or translation layer
in between. CMSIS's `GPIOC->BSRR` compiles down to the exact same
instruction as `*(volatile uint32_t*)0x40020818 = ...` — it's the same
memory write, wrapped in a named C struct for readability.

## The three operations, and why each is needed

**1. Enable the peripheral clock (`RCC_AHB1ENR`, bit 2)**
Every peripheral on this chip is clock-gated off by default at reset,
to save power. GPIOC's digital logic is not "running" — writes to it are
silently ignored — until its clock is explicitly enabled. This is a common
beginner trap: the failure mode is total silence, no error, nothing.

| Register     | Address      | Base + offset               |
|--------------|--------------|------------------------------|
| RCC_AHB1ENR  | `0x40023830` | RCC base `0x40023800` + `0x30` |

**2. Configure PC13 as output (`GPIOC_MODER`, bits 27:26)**
`MODER` packs mode config for all 16 pins on the port into one 32-bit
register, 2 bits per pin (pin *n* → bits `(2n+1):(2n)`). PC13 is pin 13 →
bits 27:26. Values: `00`=input, `01`=general-purpose output, `10`=alternate
function, `11`=analog. The code clears the field first, then sets it, so
the other 15 pins' configuration is left untouched (read-modify-write).

| Register    | Address      | Base + offset                |
|-------------|--------------|-------------------------------|
| GPIOC_MODER | `0x40020800` | GPIOC base `0x40020800` + `0x00` |

**3. Toggle the pin (`GPIOC_BSRR`)**
`BSRR` is write-only and self-clearing — it is not a stored state, it's a
one-shot command register. Writing `1` to bit *n* sets pin *n* high;
writing `1` to bit `n+16` sets it low. Writing `0` anywhere does nothing —
it's not "clear," it's "leave alone." This makes single-pin writes atomic:
no read-modify-write cycle, so no race condition if an interrupt touches
the same port mid-operation.

| Register   | Address      | Base + offset                |
|------------|--------------|-------------------------------|
| GPIOC_BSRR | `0x40020818` | GPIOC base `0x40020800` + `0x18` |

## CMSIS struct ↔ raw address mapping

CMSIS (`stm32f405xx.h`) defines `GPIOC` as a pointer to a `GPIO_TypeDef`
struct, cast onto `GPIOC_BASE`:

```c
#define GPIOC ((GPIO_TypeDef *) GPIOC_BASE)   // GPIOC_BASE = 0x40020800

typedef struct {
    __IO uint32_t MODER;    // offset 0x00
    __IO uint32_t OTYPER;   // offset 0x04
    __IO uint32_t OSPEEDR;  // offset 0x08
    __IO uint32_t PUPDR;    // offset 0x0C
    __IO uint32_t IDR;      // offset 0x10
    __IO uint32_t ODR;      // offset 0x14
    __IO uint32_t BSRR;     // offset 0x18
    __IO uint32_t LCKR;     // offset 0x1C
    __IO uint32_t AFR[2];   // offset 0x20, 0x24
} GPIO_TypeDef;
```

`__IO` expands to `volatile` — telling the compiler this memory can change
outside program flow (or must not have its writes optimized away), which
matters because hardware registers don't behave like ordinary variables.

Struct member order determines byte offset (each member is 4 bytes), so
`GPIOC->BSRR` resolves at compile time to `GPIOC_BASE + 0x18` —
`0x40020800 + 0x18 = 0x40020818` — the identical address used in
`blink_raw.c`. The struct is a naming convenience over the same memory
map, not a different mechanism.

## Toolchain / target

- MCU: STM32F405RGT6 (LQFP64)
- Written and tested in STM32CubeIDE
- No HAL calls used in either file — `SystemClock_Config()` (HSI-based)
  and `HAL_Init()` were left in place from the generated project for
  SysTick/clock setup, but GPIO is configured and driven entirely by
  the code shown here.
