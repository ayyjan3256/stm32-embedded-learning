/*
 * Week 4, Day 2 — Hardware Timer Interrupt: TIM2 1 Hz LED Toggle
 *
 * STM32F405 (Blackpill). TIM2 configured on APB1 (84 MHz timer clock).
 * Prescaler (PSC) = 83, Auto-Reload (ARR) = 999999 for exact 1 Hz update events.
 * Onboard LED on PB2 toggles inside the TIM2 update interrupt service routine.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"

/* ---- Timer 2 Interrupt Service Routine ---- */

void TIM2_IRQHandler(void)
{
    /* Check if the update interrupt flag (UIF) is set */
    if (TIM2->SR & TIM_SR_UIF)
    {
        /* Clear the flag explicitly to prevent infinite re-entry */
        TIM2->SR &= ~(TIM_SR_UIF);

        /* Toggle the LED connected to PB2 */
        GPIOB->ODR ^= (1 << 2);
    }
}

/* ---- Peripheral init (called once at startup in main()) ---- */

void timer2_init(void)
{
    /* Enable clocks for GPIOB and TIM2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* Configure PB2 as general-purpose output */
    GPIOB->MODER |= (1 << 4);
    GPIOB->MODER &= ~(1 << 5);

    /* Timer math: 84 MHz clock / (83 + 1) = 1 MHz tick rate (1 us ticks) */
    TIM2->PSC = 83;
    
    /* 1,000,000 ticks * 1 us = 1 second period */
    TIM2->ARR = 999999;
    
    /* Clear counter to start from a clean slate */
    TIM2->CNT = 0;

    /* Enable Update Interrupt in the timer peripheral */
    TIM2->DIER |= TIM_DIER_UIE;

    /* Start the timer counter */
    TIM2->CR1 |= TIM_CR1_CEN;

    /* Enable TIM2 interrupt line in the Nested Vectored Interrupt Controller (NVIC) */
    NVIC_EnableIRQ(TIM2_IRQn);
    NVIC_SetPriority(TIM2_IRQn, 0);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    timer2_init();

    while (1)
    {
        // Infinite loop remains completely empty. 
        // CPU processing is fully event-driven by the timer interrupt.
    }
}
*/