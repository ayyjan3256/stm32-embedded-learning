/**
 * blink_day6_systick_delay_raw.c
 *
 * STM32F405 bare-metal LED blink — PC13 — replacing HAL_Delay with a
 * hand-written millisecond delay backed directly by the Cortex-M4's
 * SysTick timer. GPIO is raw-address (see blink_raw.c); SysTick here
 * is also raw-address, computed by hand from the ARM Cortex-M4 core
 * peripheral map (SysTick is core architecture, not STM32-specific
 * silicon — RM0090's coverage of it is thin; PM0214 / the CMSIS core
 * header are the better references, see README.md).
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 * Clock: HSI, 16 MHz, no PLL (see SystemClock_Config in the generated
 * project). LOAD value below is derived specifically for 16 MHz.
 *
 * SysTick registers used (base 0xE000E010):
 *   CTRL = 0xE000E010   (offset 0x00) — control & status
 *   LOAD = 0xE000E014   (offset 0x04) — reload value
 *   VAL  = 0xE000E018   (offset 0x08) — current count
 */

#include <stdint.h>
#include "stm32f4xx.h"   /* still used for RCC_AHB1ENR_GPIOCEN-style
                             GPIO access via CMSIS in this file's GPIO
                             portion — SysTick itself is raw below to
                             show the address-level equivalent */

#define SYSTICK_CTRL (*(volatile uint32_t *)0xE000E010)
#define SYSTICK_LOAD (*(volatile uint32_t *)0xE000E014)
#define SYSTICK_VAL  (*(volatile uint32_t *)0xE000E018)

#define SYSTICK_CTRL_ENABLE    (1 << 0)
#define SYSTICK_CTRL_CLKSOURCE (1 << 2)   /* 1 = processor clock (HCLK) */
#define SYSTICK_CTRL_COUNTFLAG (1 << 16)  /* read-only; reading CTRL clears it */

void SysTick_Init(void)
{
    /* LOAD = (SYSCLK_Hz / ticks_per_second) - 1
     * At 16 MHz, for a 1ms tick: (16,000,000 / 1000) - 1 = 15999.
     * Off-by-one because counting LOAD..0 inclusive is LOAD+1 cycles. */
    SYSTICK_LOAD = 15999;
    SYSTICK_VAL  = 0;                                  /* clear stale count + COUNTFLAG */
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_ENABLE;
    /* TICKINT (bit 1) intentionally left off — polling only, no interrupt. */
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        /* Fresh read every iteration is required — reading CTRL is what
         * clears COUNTFLAG, so a cached copy would never update. */
        while (!(SYSTICK_CTRL & SYSTICK_CTRL_COUNTFLAG))
        {
            /* spin until this 1ms tick elapses */
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    SysTick_Init();

    /* GPIOC clock enable + PC13 as output — same as blink_raw.c. */
    RCC->AHB1ENR |= (1 << 2);
    GPIOC->MODER &= ~(0b11 << 26);
    GPIOC->MODER |=  (0b01 << 26);

    while (1)
    {
        GPIOC->BSRR = (1 << 13);        /* PC13 high */
        delay_ms(500);
        GPIOC->BSRR = (1 << (13 + 16)); /* PC13 low */
        delay_ms(500);
    }
}
