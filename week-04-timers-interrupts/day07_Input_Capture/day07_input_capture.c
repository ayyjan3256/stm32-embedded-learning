/* Week 4, Day 7 — Hardware Timers: Input Capture and Microsecond Precision
 *
 * STM32F405 (Blackpill). TIM3 configured on APB1 (84 MHz timer clock).
 * Prescaler (PSC) = 83, Auto-Reload (ARR) = 0xFFFF for 1 us ticks and max period.
 * Channel 1 (PA6) routed via AF2 to capture hardware timer snapshots on falling edges.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>

/* ---- Global Shared Variables ---- */
volatile uint32_t gap_duration_us = 0;
volatile uint32_t last_period_us = 0;
volatile uint32_t vibration_freq_hz = 0;
volatile uint8_t  new_pulse_captured = 0;

/* ---- Timer 3 Interrupt Service Routine ---- */
void TIM3_IRQHandler(void) 
{
    /* Check if the Capture/Compare 1 interrupt flag is set */
    if(TIM3->SR & TIM_SR_CC1IF)
    {
        static uint16_t last_capture = 0;
        
        /* Reading CCR1 automatically clears the CC1IF flag */
        uint16_t current_capture = TIM3->CCR1;
        
        /* 16-bit unsigned math natively handles ARR rollover */
        gap_duration_us = (uint16_t)(current_capture - last_capture);
        last_capture = current_capture;
        
        /* Software filter: Ignore switch bounce (<20us) and idle time (>50ms) */
        if (gap_duration_us > 20 && gap_duration_us < 50000) 
        {
            last_period_us = gap_duration_us;
            vibration_freq_hz = 1000000 / gap_duration_us;
            new_pulse_captured = 1;
        }
    }
}

/* ---- Peripheral Init (called once at startup in main()) ---- */
void input_capture_init(void)
{
    /* 1. Enable Clocks for TIM3, GPIOA, and USART2 */
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* 2. Configure PA6 for TIM3_CH1 (Alternate Function 2) */
    GPIOA->MODER &= ~(3 << 12);
    GPIOA->MODER |=  (2 << 12);
    GPIOA->AFR[0] &= ~(0xF << 24);
    GPIOA->AFR[0] |=  (2 << 24);

    /* 3. Configure USART2 on PA2/PA3 for serial output */
    GPIOA->MODER |= (2 << 4) | (2 << 6);       
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);     
    USART2->BRR = 365;                     // 115200 Baud @ 42MHz APB1
    USART2->CR1 |= (1 << 13) | (1 << 3) | (1 << 2); // UE, TE, RE

    /* 4. Configure TIM3 for 1 MHz Tick Rate */
    TIM3->PSC = 83;
    TIM3->ARR = 0xFFFF;

    /* 5. Configure Input Capture Mode */
    TIM3->CCMR1 &= ~(3 << 0);
    TIM3->CCMR1 |=  (1 << 0);              // CC1S = 01 (Input on TI1)
    
    TIM3->CCER &= ~(3 << 0);
    TIM3->CCER |=  (3 << 0);               // CC1P = 1 (Falling Edge), CC1E = 1 (Enable)

    /* 6. Enable Interrupts and Start Timer */
    TIM3->DIER |= (1 << 1);                // CC1IE (Capture/Compare 1 Interrupt Enable)
    NVIC_SetPriority(TIM3_IRQn, 1);
    NVIC_EnableIRQ(TIM3_IRQn);
    
    TIM3->CR1 |= TIM_CR1_CEN;              // Start Timer
}

/* ---- Custom SysTick Delay Functions ---- */
void SysTick_Init(void)
{
    SysTick->LOAD = 167999;
    SysTick->VAL = 0;
    SysTick->CTRL = (1 << 0) | (1 << 2);
}

void delay_ms(uint32_t ms)
{
    for (int i = 0; i < ms; i++){
        while(!(SysTick->CTRL & (1 << 16)));
    }
}

/* ---- UART Retargeting for printf ---- */
void uart_write_byte(uint8_t c)
{
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++){
        uart_write_byte(ptr[i]);
    }
    return len;
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    SysTick_Init();
    input_capture_init();

    while (1)
    {
        // Check if the background ISR captured a valid mechanical pulse
        if (new_pulse_captured == 1)
        {
            new_pulse_captured = 0;
            printf("Pulse Period: %lu us | Freq: %lu Hz\r\n", last_period_us, vibration_freq_hz);
        }
        
        delay_ms(500); // Slow down the terminal output loop
    }
}
