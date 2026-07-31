/*
 * Week 3, Day 4 — Joystick: Dual-Axis ADC + Digital Button
 *
 * STM32F405 (Blackpill). KY-023 joystick: VRx on PA0, VRy on PA1,
 * SW (button) on PB0. USART2 on PA2/PA3 @ 115200 baud for output.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code written this session. Drop these pieces into
 * their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <stdio.h>
#include <stdlib.h>

#define JOY_CENTER   2048   /* replace with your measured resting value */
#define JOY_DEADZONE 500

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

void joystick_init(void)
{
    /* Clock enables */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA2/PA3 -> alternate function mode (10) for USART2 TX/RX */
    GPIOA->MODER &= ~(1 << 4);
    GPIOA->MODER |= (1 << 5);
    GPIOA->MODER &= ~(1 << 6);
    GPIOA->MODER |= (1 << 7);

    /* PA0, PA1 -> analog mode (11) for VRx, VRy */
    GPIOA->MODER |= (3 << 0);
    GPIOA->MODER |= (3 << 2);

    /* AF7 for PA2 and PA3 -> USART2 */
    GPIOA->AFR[0] |= (7 << 8);
    GPIOA->AFR[0] |= (7 << 12);

    /* PB0 -> digital input (00) with pull-up (01) for the joystick button */
    GPIOB->MODER &= ~(3 << 0);
    GPIOB->PUPDR |= (1 << 0);

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
}

/* ---- Direction classifier ---- */

char* joystick_direction(uint16_t x, uint16_t y)
{
    int16_t x_offset = (int16_t)x - JOY_CENTER;
    int16_t y_offset = (int16_t)y - JOY_CENTER;

    if (x_offset > -JOY_DEADZONE && x_offset < JOY_DEADZONE &&
        y_offset > -JOY_DEADZONE && y_offset < JOY_DEADZONE) {
        return "CENTER";
    }

    if (abs(x_offset) > abs(y_offset)) {
        return (x_offset > 0) ? "RIGHT" : "LEFT";
    } else {
        return (y_offset > 0) ? "UP" : "DOWN";
    }
}

/* ---- Main loop body ---- */

void joystick_loop(void)
{
    uint16_t x_val = ADC_Read(0);   /* PA0 - VRx */
    uint16_t y_val = ADC_Read(1);   /* PA1 - VRy */
    uint8_t  btn   = (GPIOB->IDR & (1 << 0)) ? 1 : 0;

    printf("X: %u  Y: %u  Direction: %s  BTN: %u\r\n",
           x_val, y_val, joystick_direction(x_val, y_val), btn);
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    joystick_init();

    while (1)
    {
        joystick_loop();
        delay_ms(100);
    }
}
*/
