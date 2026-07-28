/*
 * Week 2, Day 7 — Bidirectional Command Console (MCU side)
 *
 * STM32F405 (Blackpill), USART2 on PA2/PA3 @ 115200 baud, LED on PB2.
 * Reads "LED ON" / "LED OFF" commands over UART, toggles PB2, and
 * sends a confirmation string back via printf.
 *
 * NOTE: this file omits STM32CubeIDE/CubeMX boilerplate (HAL_Init,
 * SystemClock_Config, Error_Handler, USER CODE markers, etc.) and
 * shows only the code actually written this week. Drop these pieces
 * into their corresponding sections of a generated main.c to build.
 */

#include "main.h"
#include <string.h>
#include <stdio.h>

/* ---- Day 4/5: byte-level UART I/O + printf retargeting ---- */

uint8_t uart_read_byte(void)
{
    while (!(USART2->SR & USART_SR_RXNE));
    return USART2->DR;
}

void uart_write_byte(uint8_t c)
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

/* ---- Peripheral init (Days 2-3, called once at startup) ---- */

void uart_led_init(void)
{
    /* Clock enables. |= matters: GPIOA and GPIOB share AHB1ENR — using
       plain '=' on the second write silently clears the first enable. */
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;

    /* PA2/PA3 -> alternate function mode (10) for USART2 TX/RX */
    GPIOA->MODER |= (1 << 5);
    GPIOA->MODER &= ~(1 << 4);
    GPIOA->MODER |= (1 << 7);
    GPIOA->MODER &= ~(1 << 6);

    /* PB2 -> general purpose output (01), drives the LED */
    GPIOB->MODER |= (1 << 4);
    GPIOB->MODER &= ~(1 << 5);

    /* AF7 for PA2 (bits 11:8) and PA3 (bits 15:12) -> USART2 */
    GPIOA->AFR[0] |= (7 << 8);
    GPIOA->AFR[0] |= (7 << 12);

    GPIOA->OSPEEDR |= (3 << 4);
    GPIOA->OSPEEDR |= (3 << 6);

    /* BRR for 115200 baud @ 16 MHz HSI:
       16,000,000 / (16 * 115200) = 8.6805
       mantissa = 8, fraction = 0.6805 * 16 ≈ 11 (0xB)
       BRR = (8 << 4) | 11 = 139 (0x8B) */
    USART2->BRR = 139;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/* ---- Day 7: buffer + dispatch loop ---- */

void command_console_run(void)
{
    char rx_buffer[32];
    int  rx_index = 0;

    while (1)
    {
        char c = uart_read_byte();

        if (c == '\n')
        {
            rx_buffer[rx_index] = '\0';

            if (strcmp(rx_buffer, "LED ON") == 0)
            {
                GPIOB->BSRR = (1 << 2);      // set PB2 high
                printf("LED: ON\n");
            }
            else if (strcmp(rx_buffer, "LED OFF") == 0)
            {
                GPIOB->BSRR = (1 << 18);     // reset PB2 low
                printf("LED: OFF\n");
            }
            else
            {
                printf("Unknown Command\n");
            }

            rx_index = 0;
        }
        else
        {
            rx_buffer[rx_index] = c;
            rx_index++;
        }
    }
}

/* ---- Usage in main() ---- */
/*
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    uart_led_init();
    command_console_run();   // never returns
}
*/
