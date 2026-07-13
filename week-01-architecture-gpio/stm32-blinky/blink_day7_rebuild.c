/**
 * blink_day7_rebuild.c
 *
 * STM32F405 bare-metal LED blink — PC13 — written from a blank project
 * with no reference material open except RM0090 for individual bit
 * positions when needed. This is the Week 1 capstone: clock enable,
 * GPIO output configuration, a hand-written SysTick millisecond delay,
 * and a blink loop, combining everything from Days 1-6 into one clean
 * file with zero HAL GPIO calls and zero busy-loop timing.
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 * Clock: HSI, 16 MHz, no PLL.
 */

#include "stm32f4xx.h"

void SysTick_Init(void)
{
    SysTick->LOAD = 15999;               /* 1ms tick at 16 MHz: (16e6/1000)-1 */
    SysTick->VAL  = 0;                   /* clear stale count / COUNTFLAG */
    SysTick->CTRL = (1 << 0) | (1 << 2); /* ENABLE + CLKSOURCE = processor clock */
}

void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        while (!(SysTick->CTRL & (1 << 16))) /* poll COUNTFLAG; read clears it */
        {
        }
    }
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    SysTick_Init();

    /* GPIOC clock enable (AHB1ENR bit 2 = GPIOCEN). */
    RCC->AHB1ENR |= (1 << 2);

    /* PC13 as general-purpose output: MODER bits 27:26 = 01. */
    GPIOC->MODER &= ~(0b11 << 26);
    GPIOC->MODER |=  (0b01 << 26);

    while (1)
    {
        GPIOC->BSRR = (1 << 13);        /* PC13 high — atomic, single write */
        delay_ms(500);
        GPIOC->BSRR = (1 << (13 + 16)); /* PC13 low */
        delay_ms(500);
    }
}
