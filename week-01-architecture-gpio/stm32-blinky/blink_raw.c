/**
 * blink_raw.c
 *
 * STM32F405 bare-metal LED blink — PC13 — using raw memory addresses only.
 * No CMSIS structs, no HAL. Every write below targets a specific physical
 * register address, computed by hand from RM0090 (reference manual).
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 *
 * Addresses used (see README.md for full derivation):
 *   RCC_AHB1ENR   = 0x40023830   (RCC base 0x40023800 + offset 0x30)
 *   GPIOC_MODER   = 0x40020800   (GPIOC base 0x40020800 + offset 0x00)
 *   GPIOC_BSRR    = 0x40020818   (GPIOC base 0x40020800 + offset 0x18)
 */

#include <stdint.h>

int main(void)
{
    /* 1. Enable the clock for GPIOC (bit 2 of RCC_AHB1ENR = GPIOCEN).
     *    Peripherals are off by default on reset — without this, every
     *    write below is a no-op: the GPIOC logic simply isn't clocked. */
    *((volatile uint32_t *)0x40023830) |= (1 << 2);

    /* 2. Configure PC13 as general-purpose output in GPIOC_MODER.
     *    Each pin occupies 2 bits: pin n -> bits (2n+1):(2n).
     *    PC13 -> bits 27:26. Value 01 = general purpose output mode.
     *    Clear the field first, then set only the '01' bit pattern —
     *    this avoids disturbing the other 15 pins packed into the
     *    same 32-bit register. */
    *((volatile uint32_t *)0x40020800) &= ~(0b11 << 26); /* clear bits 27:26 */
    *((volatile uint32_t *)0x40020800) |=  (0b01 << 26); /* set mode = output */

    /* 3. Toggle PC13 via GPIOC_BSRR.
     *    BSRR is write-only and self-clearing: writing 1 to bit n sets
     *    pin n high; writing 1 to bit (n+16) sets pin n low. Writing 0
     *    anywhere has no effect. This makes single-pin writes atomic —
     *    no read-modify-write, no race with an interrupt touching the
     *    same port. */
    while (1)
    {
        *((volatile uint32_t *)0x40020818) = (1 << 13);       /* PC13 = high */
        for (volatile int i = 0; i < 1000000; i++);           /* crude delay */

        *((volatile uint32_t *)0x40020818) = (1 << (13 + 16)); /* PC13 = low */
        for (volatile int i = 0; i < 1000000; i++);
    }
}
