#include "main.h"
#include <stdio.h>
#include <string.h>

/* Private define ------------------------------------------------------------*/
#define BUFFER_SIZE 64

/* Private variables ---------------------------------------------------------*/
char rx_buffer[BUFFER_SIZE];
uint8_t rx_index = 0;
volatile uint8_t command_ready = 0;

volatile uint32_t blink_delay = 500; // Default blink delay (500ms)
uint32_t last_blink_time = 0;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void USART_RX_Interrupt_Init(void);
void uart_write_byte(uint8_t c);

// Retarget printf to UART
int _write(int file, char *ptr, int len) {
    for (int i = 0; i < len; i++) {
        uart_write_byte(ptr[i]);
    }
    return len;
}

void uart_write_byte(uint8_t c) {
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    USART_RX_Interrupt_Init();

    printf("\r\n--- STM32 CLI Booted ---\r\n");

    while (1)
    {
        // --- 1. CLI COMMAND PARSING ---
        if (command_ready == 1) 
        {
            uint32_t new_delay = 0;

            // Check for exact matches
            if (strcmp(rx_buffer, "LED ON") == 0) {
                GPIOB->BSRR = (1 << 2);
                printf("Turned LED ON\r\n");
            }
            else if (strcmp(rx_buffer, "LED OFF") == 0) {
                GPIOB->BSRR = (1 << 18);
                printf("Turned LED OFF\r\n");
            }
            else if (strcmp(rx_buffer, "STATUS") == 0) {
                if (GPIOB->ODR & (1 << 2)) {
                    printf("LED is ON\r\n");
                } else {
                    printf("LED is OFF\r\n");
                }
            }
            // Check for dynamic variables using sscanf
            else if (sscanf(rx_buffer, "DELAY %lu", &new_delay) == 1) {
                blink_delay = new_delay;
                printf("Blink delay updated to: %lu ms\r\n", blink_delay);
            }
            // Catch invalid commands (ignoring empty enter keystrokes)
            else if (strlen(rx_buffer) > 0) {
                printf("UNKNOWN COMMAND: %s\r\n", rx_buffer);
            }

            command_ready = 0; // Wait for next command
        }

        // --- 2. NON-BLOCKING LED TASK ---
        if ((HAL_GetTick() - last_blink_time) >= blink_delay) {
            last_blink_time = HAL_GetTick(); 
            GPIOB->ODR ^= (1 << 2); // Toggle LED at the dynamic speed
        }
    }
}

void USART_RX_Interrupt_Init(void) 
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    // PB2 as Output
    GPIOB->MODER |= (1 << 4);

    // PA2 (TX) and PA3 (RX) as Alternate Function
    GPIOA->MODER |= (2 << 6) | (2 << 4);
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);

    // 42MHz APB1 / 115200 Baud
    USART2->BRR = 365; 
    
    // Enable UART, TX, RX, and RXNE Interrupt
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;

    // Enable NVIC Vector
    NVIC_EnableIRQ(USART2_IRQn);
    NVIC_SetPriority(USART2_IRQn, 0);
}

void USART2_IRQHandler(void) 
{
    if (USART2->SR & USART_SR_RXNE) 
    {
        uint8_t c = (uint8_t)USART2->DR;
        uart_write_byte(c); // Echo back to terminal

        if (c == '\r' || c == '\n') {
            uart_write_byte('\n'); // Drop terminal to new line
            rx_buffer[rx_index] = '\0';
            rx_index = 0;
            command_ready = 1; // Alert main loop
        } 
        else {
            if (rx_index < BUFFER_SIZE - 1) {
                rx_buffer[rx_index++] = c;
            }
        }
    }
}

void SystemClock_Config(void)
{
    // Implementation omitted for brevity (Use your standard 168MHz HSE config here)
}
