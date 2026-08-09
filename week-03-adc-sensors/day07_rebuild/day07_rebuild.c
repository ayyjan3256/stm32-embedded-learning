/*
 * Week 3, Day 7 — Rebuild From Memory (Final, Fixed)
 *
 * STM32F405 (Blackpill). Photoresistor divider on PA0, thermistor
 * divider on PA1, joystick button on PB0, internal chip temp on
 * ADC1 channel 16. USART2 on PA2/PA3 @ 115200 baud.
 *
 * Rebuilt from memory as a retention check, then corrected against
 * the working Day 2-6 code. Fixes applied during review:
 *   - PB0 PUPDR: corrected to pull-up (01), not pull-down
 *   - GPIOA->AFR[0]: changed from '=' to '|=' so PA2 and PA3's AF7
 *     bits don't overwrite each other
 *   - USART2 BRR/CR1 configuration: was missing entirely, causing
 *     the UART peripheral to never be enabled (TXE never sets,
 *     printf hangs forever on the first byte)
 *   - _write(): corrected return type from void to int
 *   - ADC123_COMMON->CCR: changed from '=' to '|=' to avoid wiping
 *     other common-register bits (e.g. ADC prescaler)
 *   - Internal temp channel: corrected from channel 2 (an unrelated,
 *     unconfigured external channel) to channel 16
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>

/* ---- UART byte I/O + printf retargeting ---- */

void uart_byte_write(uint8_t c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

uint8_t uart_byte_read(void)
{
    while (!(USART2->SR & USART_SR_RXNE));
    return (uint8_t)USART2->DR;
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        uart_byte_write(ptr[i]);
    }
    return len;
}

/* ---- Parameterized ADC read ---- */

uint16_t ADC_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return (uint16_t)ADC1->DR;
}

/* ---- Peripheral init (called once at startup) ---- */

void week3_final_init(void)
{
    /* Clock enables */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA0, PA1 -> analog mode (11) for photoresistor, thermistor */
    GPIOA->MODER |= (3 << 0);
    GPIOA->MODER |= (3 << 2);

    /* PA2/PA3 -> alternate function mode (10) for USART2 TX/RX */
    GPIOA->MODER &= ~(1 << 4);
    GPIOA->MODER |= (1 << 5);
    GPIOA->MODER &= ~(1 << 6);
    GPIOA->MODER |= (1 << 7);

    /* PB0 -> digital input (00) with pull-up (01) for joystick button */
    GPIOB->MODER = 0;
    GPIOB->PUPDR |= (1 << 0);
    GPIOB->PUPDR &= ~(1 << 1);

    /* AF7 for PA2 and PA3 -> USART2 */
    GPIOA->AFR[0] |= (7 << 8);
    GPIOA->AFR[0] |= (7 << 12);

    /* BRR for 115200 baud @ 16 MHz HSI */
    USART2->BRR = 139;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* ADC1 configuration: 12-bit resolution, single conversion,
       maximum sample time on channels 0 and 1 */
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    ADC1->SMPR2 |= (7 << 0);   /* channel 0 sample time */
    ADC1->SMPR2 |= (7 << 3);   /* channel 1 sample time */
    ADC1->SQR3 = 0;
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Internal temperature sensor + VREFINT enable (common register) */
    ADC123_COMMON->CCR |= ADC_CCR_TSVREFE;
}

/* ---- Main loop body ---- */

void week3_final_loop(void)
{
    uint16_t ch0 = ADC_Read(0);    /* PA0 - photoresistor       */
    uint16_t ch1 = ADC_Read(1);    /* PA1 - thermistor          */
    uint16_t ch16 = ADC_Read(16);  /* internal chip temp sensor */

    printf("%u,%u,%u\r\n", ch0, ch1, ch16);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    week3_final_init();

    while (1)
    {
        week3_final_loop();
        HAL_Delay(500);
    }
}
*/
