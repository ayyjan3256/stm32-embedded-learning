/*
 * Week 2, Day 5 — printf retargeting via _write()
 *
 * Builds on Day 4's uart_write_byte()/uart_read_byte(). Today's addition
 * is _write(), the newlib syscall hook that lets printf() transmit over
 * UART instead of doing nothing (or hitting the default stub).
 *
 * Config assumed already done (Days 2-3):
 *   - PA2/PA3 set to AF7 (USART2_TX/RX)
 *   - USART2 clock enabled via RCC->APB1ENR
 *   - BRR set for 115200 baud @ 16 MHz HSI
 *   - CR1: UE, TE, RE bits set
 */

#include "main.h"
#include <stdio.h>

/* ---- Day 4 functions, unchanged ---- */

void uart_write_byte(uint8_t c)
{
    while (!(USART2->SR & USART_SR_TXE)); // wait until transmit register is free
    USART2->DR = c;
}

uint8_t uart_read_byte(void)
{
    while (!(USART2->SR & USART_SR_RXNE)); // wait until a byte has arrived
    return USART2->DR;
}

/* ---- Day 5 addition ---- */

/*
 * newlib calls this from inside printf() once formatting is done.
 * file  - fd being written to (stdout/stderr); ignored, only one output device exists
 * ptr   - pointer to the already-formatted byte buffer
 * len   - number of bytes in that buffer
 *
 * Must have external linkage (no 'static') so this strong symbol
 * overrides newlib's weak default _write() stub at link time.
 */
int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        uart_write_byte(ptr[i]);
    }
    return len;
}

int main(void)
{
    /* ... existing init: GPIO, RCC, USART2 config from Days 2-3 ... */

    int count = 0;

    while (1)
    {
        printf("COUNT:%d\n", count);
        count++;
        for (volatile int i = 0; i < 1000000; i++); // crude delay, same as Day 4
    }
}
