/*
 * Week 3, Day 6 — Resolution, Sample Time, and Noise
 *
 * STM32F405 (Blackpill). Photoresistor divider on PA0.
 * USART2 on PA2/PA3 @ 115200 baud for output.
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

/* ---- Resolution control ---- */

void ADC_SetResolution(uint8_t res)
{
    ADC1->CR1 &= ~(3 << 24);   /* clear RES bits first -- 2-bit field,
                                   a plain OR could combine incorrectly
                                   with whatever was previously set */
    ADC1->CR1 |= (res << 24);
}

/* ---- Averaging filter ---- */

uint16_t ADC_ReadAveraged(uint8_t channel, uint8_t samples)
{
    uint32_t sum = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
        sum += ADC_Read(channel);
    }
    return (uint16_t)(sum / samples);
}

/* ---- Peripheral init (called once at startup) ---- */

void adc_tuning_init(void)
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

    /* PA0 -> analog mode (11) for photoresistor */
    GPIOA->MODER |= (3 << 0);

    /* AF7 for PA2 and PA3 -> USART2 */
    GPIOA->AFR[0] |= (7 << 8);
    GPIOA->AFR[0] |= (7 << 12);

    /* BRR for 115200 baud @ 16 MHz HSI */
    USART2->BRR = 139;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

    /* ADC1 configuration: 12-bit resolution, single conversion,
       maximum sample time (480 cycles), channel 0 default */
    ADC1->CR1 = 0;
    ADC1->CR2 = 0;
    ADC1->SMPR2 = (7 << 0);
    ADC1->SQR3 = 0;
    ADC1->CR2 |= ADC_CR2_ADON;
}

/* ---- One-time diagnostics (run before while(1), not inside it) ---- */

void resolution_comparison(void)
{
    ADC_SetResolution(0);
    uint16_t r12 = ADC_Read(0);

    ADC_SetResolution(1);
    uint16_t r10 = ADC_Read(0);

    ADC_SetResolution(2);
    uint16_t r8 = ADC_Read(0);

    ADC_SetResolution(3);
    uint16_t r6 = ADC_Read(0);

    printf("12-bit: %u  10-bit: %u  8-bit: %u  6-bit: %u\r\n", r12, r10, r8, r6);

    ADC_SetResolution(0);   /* restore 12-bit before normal operation resumes */
}

void measure_conversion_rate(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    uint32_t start = DWT->CYCCNT;
    for (int i = 0; i < 1000; i++)
    {
        ADC_Read(0);
    }
    uint32_t end = DWT->CYCCNT;

    uint32_t elapsed_cycles = end - start;
    float elapsed_seconds = elapsed_cycles / 16000000.0f;   /* 16 MHz HSI CPU clock */
    float conversions_per_second = 1000.0f / elapsed_seconds;

    printf("Elapsed: %.4fs  Rate: %.2f conversions/sec\r\n",
           elapsed_seconds, conversions_per_second);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    adc_tuning_init();

    resolution_comparison();     // one-time, prints once at boot
    measure_conversion_rate();   // one-time, prints once at boot

    while (1)
    {
        // Unaveraged, for observing raw jitter:
        // uint16_t val = ADC_Read(0);

        // Averaged, for comparison:
        uint16_t val = ADC_ReadAveraged(0, 16);

        printf("%u\r\n", val);
        delay_ms(200);
    }
}
*/
