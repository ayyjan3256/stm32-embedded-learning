/*
 * Week 3, Day 3 — Second Channel + Internal Temperature Sensor
 *
 * STM32F405 (Blackpill). Photoresistor divider on PA0, thermistor
 * divider on PA1, internal chip temp sensor on ADC1 channel 16.
 * USART2 on PA2/PA3 @ 115200 baud for serial output.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>
#include <math.h>

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

/* ---- Parameterized ADC read (any regular channel, single conversion) ---- */

uint16_t ADC_Read(uint8_t channel)
{
    ADC1->SQR3 = channel;   /* full overwrite, not |=, so no channel selection
                                carries over between calls */
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (!(ADC1->SR & ADC_SR_EOC));
    return (uint16_t)ADC1->DR;
}

/* ---- Peripheral init (called once at startup) ---- */

void adc_multichannel_init(void)
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

    /* PA0, PA1 -> analog mode (11) for ADC1 channels 0 and 1 */
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

    /* Internal temperature sensor + VREFINT enable (common register,
       shared across ADC1/2/3 -- not part of ADC1's own CR2) */
    ADC123_COMMON->CCR |= ADC_CCR_TSVREFE;
    delay_ms(1);   /* internal sensor needs ~10us to stabilize; 1ms covers it */
}

/* ---- Conversion helpers ---- */

/* Internal chip temperature sensor: linear formula from RM0090,
   using typical calibration constants (V25 ~ 0.76V, Avg_Slope ~ 2.5mV/C) */
float internal_temp_c(uint16_t raw)
{
    float vsense = (raw / 4095.0f) * 3.3f;
    return ((vsense - 0.76f) / 0.0025f) + 25.0f;
}

/* External NTC thermistor: voltage divider -> resistance -> Steinhart-Hart.
   Assumes thermistor-to-3.3V, fixed-resistor-to-GND wiring; generic 10k
   NTC coefficients, not datasheet-specific. */
float external_temp_c(uint16_t raw)
{
    const float R_FIXED = 10000.0f;
    float r_thermistor = R_FIXED * ((4095.0f / raw) - 1.0f);
    float log_r = logf(r_thermistor);
    float temp_k = 1.0f / (0.0010295f + 0.0002391f * log_r
                            + 0.0000001568f * log_r * log_r * log_r);
    return temp_k - 273.15f;
}

/* ---- Main loop body ---- */

void adc_multichannel_loop(void)
{
    uint16_t light_val = ADC_Read(0);    /* PA0 - photoresistor        */
    uint16_t temp_val  = ADC_Read(1);    /* PA1 - thermistor           */
    uint16_t itemp_raw = ADC_Read(16);   /* internal chip temp sensor  */

    float ext_temp_c = external_temp_c(temp_val);
    float chip_temp_c = internal_temp_c(itemp_raw);

    printf("Light: %u  Ext Temp Raw: %u  Ext Temp C: %.2f\r\n",
           light_val, temp_val, ext_temp_c);
    printf("ITemp Sensor Raw: %.2f\r\n", chip_temp_c);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    adc_multichannel_init();

    while (1)
    {
        adc_multichannel_loop();
        delay_ms(2000);
    }
}
*/
