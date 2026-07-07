/**
 * blink_cmsis.c
 *
 * STM32F405 bare-metal LED blink — PC13 — using ST's CMSIS peripheral
 * structs (RCC->, GPIOC->) instead of raw addresses.
 *
 * This does exactly the same thing as blink_raw.c, at the exact same
 * physical addresses — CMSIS just wraps those addresses in named C
 * structs so registers can be accessed by name instead of hex offset.
 * See README.md for the address-to-struct-member mapping.
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 */

#include "stm32f4xx.h"   /* pulls in stm32f405xx.h: RCC, GPIOC, GPIO_TypeDef, etc. */

int main(void)
{
    /* 1. Enable GPIOC's clock.
     *    RCC->AHB1ENR is the same register as raw address 0x40023830.
     *    Bit 2 = GPIOCEN. */
    RCC->AHB1ENR |= (1 << 2);

    /* 2. Configure PC13 as general-purpose output.
     *    GPIOC->MODER is the same register as raw address 0x40020800.
     *    PC13 -> bits 27:26. 01 = general purpose output mode. */
    GPIOC->MODER &= ~(0b11 << 26); /* clear bits 27:26 */
    GPIOC->MODER |=  (0b01 << 26); /* set mode = output */

    /* 3. Toggle PC13 via BSRR.
     *    GPIOC->BSRR is the same register as raw address 0x40020818. */
    while (1)
    {
        GPIOC->BSRR = (1 << 13);        /* PC13 = high */
        for (volatile int i = 0; i < 1000000; i++);

        GPIOC->BSRR = (1 << (13 + 16)); /* PC13 = low */
        for (volatile int i = 0; i < 1000000; i++);
    }
}
