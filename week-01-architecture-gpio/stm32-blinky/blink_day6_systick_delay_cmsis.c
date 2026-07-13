/**
 * blink_day6_systick_delay_cmsis.c
 *
 * STM32F405 bare-metal LED blink — PC13 — same as
 * blink_day6_systick_delay_raw.c, but using CMSIS's SysTick_Type struct
 * (SysTick->CTRL / ->LOAD / ->VAL, from core_cm4.h) instead of raw
 * addresses. Functionally identical: SysTick->CTRL compiles to exactly
 * the same instruction as *(volatile uint32_t*)0xE000E010, same as the
 * GPIOC-struct-vs-raw-address relationship shown in blink_cmsis.c /
 * blink_raw.c for Day 1.
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 * Clock: HSI, 16 MHz, no PLL.
 */

#include "stm32f4xx.h"

void SysTick_Init(void)
{
    /* LOAD = (SYSCLK_Hz / ticks_per_second) - 1 = (16,000,000/1000)-1 = 15999 */
    SysTick->LOAD = 15999;
    SysTick->VAL  = 0;                              /* clear stale count + COUNTFLAG */
    SysTick->CTRL = (1 << 2) | (1 << 0);             /* CLKSOURCE=processor clock, ENABLE=1 */
    /* TICKINT (bit 1) left off — polling only. */
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        /* Reading SysTick->CTRL clears COUNTFLAG (bit 16) as a side
         * effect — every check must be a live read of the real
         * register, never a cached copy. */
        while (!(SysTick->CTRL & (1 << 16)))
        {
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    SysTick_Init();

    RCC->AHB1ENR |= (1 << 2);
    GPIOC->MODER &= ~(0b11 << 26);
    GPIOC->MODER |=  (0b01 << 26);

    while (1)
    {
        GPIOC->BSRR = (1 << 13);
        delay_ms(500);
        GPIOC->BSRR = (1 << (13 + 16));
        delay_ms(500);
    }
}
