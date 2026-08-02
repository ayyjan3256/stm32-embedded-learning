/*
 * Week 3, Day 5 — Streaming to Python: Three-Channel CSV Output
 *
 * STM32F405 (Blackpill). Photoresistor divider on PA0, thermistor
 * divider on PA1, internal chip temp sensor on ADC1 channel 16.
 * USART2 on PA2/PA3 @ 115200 baud, output as CSV for Python parsing.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>

/* ---- UART byte I/O + printf retargeting (from Week 2) ---- */

void uart_write_byte(char c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        uart_write_byte(ptr[i]);
    }
    return len;
}

/* ---- Parameterized ADC read (from Day 3) ---- */

uint16_t ADC_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return (uint16_t)ADC1->DR;
}

/* ---- Peripheral init (called once at startup) ---- */

void adc_stream_init(void)
{
    /* Clock enables */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA2/PA3 -> alternate function mode (10) for USART2 TX/RX */
    GPIOA->MODER &= ~(1 << 4);
    GPIOA->MODER |= (1 << 5);
    GPIOA->MODER &= ~(1 << 6);
    GPIOA->MODER |= (1 << 7);

    /* PA0, PA1 -> analog mode (11) for photoresistor, thermistor */
    GPIOA->MODER |= (3 << 0);
    GPIOA->MODER |= (3 << 2);

    /* AF7 for PA2 and PA3 -> USART2 */
    GPIOA->AFR[0] |= (7 << 8);
    GPIOA->AFR[0] |= (7 << 12);

    /* BRR for 115200 baud @ 16 MHz HSI */
    USART2->BRR = 139;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* ADC1 configuration: 12-bit resolution, single conversion,
       maximum sample time, channel selected per-call via ADC_Read() */
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    ADC1->SMPR2 = (7 << 0);
    ADC1->SQR3 = 0;
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Internal temperature sensor + VREFINT enable (common register) */
    ADC123_COMMON->CCR |= ADC_CCR_TSVREFE;
    delay_ms(1);   /* internal sensor needs ~10us to stabilize */
}

/* ---- Main loop body ---- */

void adc_stream_loop(void)
{
    uint16_t ch0 = ADC_Read(0);    /* PA0 - photoresistor       */
    uint16_t ch1 = ADC_Read(1);    /* PA1 - thermistor          */
    uint16_t ch2 = ADC_Read(16);   /* internal chip temp sensor */

    printf("%u,%u,%u\r\n", ch0, ch1, ch2);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    adc_stream_init();

    while (1)
    {
        adc_stream_loop();
        delay_ms(50);   /* ~20 Hz sample rate */
    }
}
*/
