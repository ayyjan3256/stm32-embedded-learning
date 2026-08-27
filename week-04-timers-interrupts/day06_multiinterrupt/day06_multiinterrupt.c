/*
 * Week 4, Day 6 — Multi-Interrupt Priorities, Volatile Variables, and ISR-ADC
 *
 * STM32F405 (Blackpill). 
 * Hardware Mapping:
 * - PA0 (EXTI0, Priority 1): KY-004 Button
 * - PA1 (GPIO Out): Button indicator LED
 * - PA4 (EXTI4, Priority 2): KY-031 Knock Sensor (or KY-002)
 * - PA5 (ADC1_IN5): KY-018 Photoresistor
 * - PB2 (GPIO Out): TIM2 1Hz Heartbeat LED
 * - PA2/PA3: USART2 for printf console
 *
 * Key Concepts:
 * - Volatile variables for safe ISR-to-main data sharing.
 * - Non-blocking architecture: heavy main() workload does not delay interrupts.
 * - Hardware timer (TIM3, Priority 3) triggering non-blocking ADC reads at 100Hz.
 * - Preemption priorities dictate execution order when hardware events overlap.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>

/* ---- Shared Global Variables ---- */
/* 'volatile' forces the CPU to read RAM on every pass instead of caching in a register */
volatile uint32_t ms_ticks = 0;
volatile uint32_t vibration_count = 0;
volatile uint32_t display_count = 0;
volatile uint8_t  update_count = 0;
volatile uint16_t adc_result = 0;

/* ---- Interrupt Service Routines ---- */

void SysTick_Handler(void)
{
    ms_ticks++;
}

void TIM2_IRQHandler(void)
{
    /* TIM2 (1 Hz) packages the data and signals the main loop */
    if (TIM2->SR & TIM_SR_UIF) {
        TIM2->SR &= ~(TIM_SR_UIF);
        
        display_count = vibration_count;
        vibration_count = 0;
        update_count = 1;
        
        GPIOB->ODR ^= (1 << 2); /* Toggle Heartbeat LED */
    }
}

void TIM3_IRQHandler(void)
{
    /* TIM3 (100 Hz) triggers the ADC in the background */
    if (TIM3->SR & TIM_SR_UIF) {
        TIM3->SR &= ~(TIM_SR_UIF);
        
        ADC1->CR2 |= ADC_CR2_SWSTART;
        while (!(ADC1->SR & ADC_SR_EOC)); /* Safe 15-cycle micro-poll */
        adc_result = ADC1->DR;
    }
}

void EXTI0_IRQHandler(void)
{
    /* Priority 1 Button Press */
    if (EXTI->PR & (1 << 0)) {
        EXTI->PR = (1 << 0); /* Write 1 to clear */
        
        static uint32_t last_pressed_time = 0;
        if ((ms_ticks - last_pressed_time) > 50) {
            last_pressed_time = ms_ticks;
            GPIOA->ODR ^= (1 << 1); /* Toggle PA1 Button LED */
        }
    }
}

void EXTI4_IRQHandler(void)
{
    /* Priority 2 Knock Sensor */
    if (EXTI->PR & (1 << 4)) {
        EXTI->PR = (1 << 4); /* Write 1 to clear */
        vibration_count++;   /* Silently count knocks */
    }
}

/* ---- Peripheral init (called once at startup in main()) ---- */

void SysTick_Init(void)
{
    SysTick->LOAD = 167999; /* 168MHz / 1000 = 1ms tick */
    SysTick->VAL = 0;
    SysTick->CTRL = (1 << 0) | (1 << 1) | (1 << 2); /* Enable, Interrupt, SysCLK */
}

void delay_ms(uint32_t ms)
{
    for (int i = 0; i < ms; i++) {
        while (!(SysTick->CTRL & (1 << 16)));
    }
}

void uart_write_byte(uint8_t c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++) {
        uart_write_byte(ptr[i]);
    }
    return len;
}

void Day6_Hardware_Init(void)
{
    /* 1. Clocks */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM3EN | RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_ADC1EN;

    /* 2. GPIO Modes */
    GPIOA->MODER &= ~(3 << 0); /* PA0 Input (Button) */
    GPIOA->MODER &= ~(3 << 8); /* PA4 Input (Knock) */
    
    GPIOA->MODER &= ~(3 << 2);
    GPIOA->MODER |=  (1 << 2); /* PA1 Output (Button LED) */
    
    GPIOB->MODER &= ~(3 << 4);
    GPIOB->MODER |=  (1 << 4); /* PB2 Output (Heartbeat LED) */
    
    GPIOA->MODER |= (3 << 10); /* PA5 Analog Mode (ADC1_IN5) */
    
    /* UART2 Pins (PA2 TX, PA3 RX) */
    GPIOA->MODER |= (2 << 4) | (2 << 6);
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);
    USART2->BRR = 365; /* 115200 at 42MHz APB1 */
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

    /* 3. Pull-ups */
    GPIOA->PUPDR &= ~(3 << 0);
    GPIOA->PUPDR |=  (1 << 0); /* PA0 Pull-up */
    
    GPIOA->PUPDR &= ~(3 << 8);
    GPIOA->PUPDR |=  (1 << 8); /* PA4 Pull-up */

    /* 4. ADC Setup (Channel 5) */
    ADC1->CR2 |= ADC_CR2_ADON;
    ADC1->SQR3 = 5;

    /* 5. TIM2 Setup (1 Hz) */
    TIM2->PSC = 8399;
    TIM2->ARR = 9999;
    TIM2->CNT = 0;
    TIM2->DIER |= TIM_DIER_UIE;
    TIM2->CR1 |= TIM_CR1_CEN;
    NVIC_SetPriority(TIM2_IRQn, 0); /* Highest Preemption */
    NVIC_EnableIRQ(TIM2_IRQn);

    /* 6. TIM3 Setup (100 Hz for ADC) */
    TIM3->PSC = 8399;
    TIM3->ARR = 99;
    TIM3->DIER |= TIM_DIER_UIE;
    TIM3->CR1 |= TIM_CR1_CEN;
    NVIC_SetPriority(TIM3_IRQn, 3); /* Lowest Preemption */
    NVIC_EnableIRQ(TIM3_IRQn);

    /* 7. EXTI0 (Button on PA0) */
    SYSCFG->EXTICR[0] &= ~(0xF << 0);
    EXTI->IMR |= (1 << 0);
    EXTI->FTSR |= (1 << 0);
    NVIC_SetPriority(EXTI0_IRQn, 1);
    NVIC_EnableIRQ(EXTI0_IRQn);

    /* 8. EXTI4 (Knock on PA4) */
    SYSCFG->EXTICR[1] &= ~(0xF << 0);
    EXTI->IMR |= (1 << 4);
    EXTI->FTSR |= (1 << 4);
    NVIC_SetPriority(EXTI4_IRQn, 2);
    NVIC_EnableIRQ(EXTI4_IRQn);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    SysTick_Init();
    Day6_Hardware_Init();
    
    printf("System Initialized. Starting Day 6 Multitasking...\r\n");

    while (1)
    {
        // Fake Heavy Workload: Force CPU to calculate instead of instantly checking flag
        for (volatile uint32_t i = 0; i < 5000000; i++) {
            // CPU is busy crunching numbers, but hardware interrupts still fire!
        }

        // Check if TIM2 packaged new data
        if (update_count == 1) {
            update_count = 0;
            printf("[%lu ms] Knocks: %lu | Light: %u\r\n", ms_ticks, display_count, adc_result);
        }
    }
}
*/