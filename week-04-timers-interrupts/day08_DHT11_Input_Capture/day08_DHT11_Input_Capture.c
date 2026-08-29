

#include "main.h"
#include <stdio.h>

/* --- GLOBAL VARIABLES --- */
volatile uint16_t capture_times[82];
volatile uint8_t edge_count = 0;
volatile uint8_t data_ready = 0;

/* Placeholder variables for multi-sensor integration: can be referred from day 07*/
volatile uint32_t vibration_freq_hz = 0; 
volatile uint16_t adc_result = 0;        

/* --- FUNCTION PROTOTYPES --- */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void dht11_start_signal(void);
void uart_write_byte(uint8_t c);
int _write(int file, char *ptr, int len);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // --- UART CONFIGURATION (PA2/PA3) ---
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
    GPIOA->MODER |= (2<<4) | (2<<6);       
    GPIOA->AFR[0] |= (7<<8) | (7<<12);     
    USART2->BRR = 365;                     // 115200 Baud @ 42MHz PCLK
    USART2->CR1 |= (1<<13) | (1<<3) | (1<<2); // UE, TE, RE

    // --- TIMER 4 CONFIGURATION (Base) ---
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;
    GPIOB->OTYPER |= (1<<8);               // PB8 Open-Drain (just in case)
    TIM4->PSC = 83;                        // 1 MHz clock (1 tick = 1 us)
    TIM4->ARR = 0xFFFF;                    // Max out ARR to prevent early rollover
    TIM4->CR1 |= TIM_CR1_CEN;              // Enable Timer

    printf("\r\n=================================\r\n");
    printf("DHT11 Multi-Sensor Hub Booting...\r\n");
    printf("=================================\r\n");

    uint32_t last_dht11_read_time = 0;

    /* Infinite loop */
    while (1)
    {
        // 1. NON-BLOCKING TRIGGER: Fire every 2000 ms
        if (HAL_GetTick() - last_dht11_read_time >= 2000) 
        {
            dht11_start_signal();
            last_dht11_read_time = HAL_GetTick(); // Reset timestamp
        }

        // 2. DATA PROCESSING: Check if interrupt captured 40 bits
        if (data_ready == 1) 
        {
            data_ready = 0; // Clear flag immediately
            uint8_t dht_data[5] = {0, 0, 0, 0, 0};
            
            for (int i = 0; i < 40; i++) {
                uint16_t rising_time = capture_times[2*i+3];
                uint16_t falling_time = capture_times[2*i+4];
                uint16_t pulse_duration = (uint16_t)(falling_time - rising_time);

                uint8_t byte_index = i / 8;
                dht_data[byte_index] <<= 1;
                if (pulse_duration > 50) {
                    dht_data[byte_index] |= 1;
                }
            }
            
            // Checksum Validation
            uint8_t calculated_checksum = (uint8_t)(dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3]);
            
            if (calculated_checksum == dht_data[4]) {
                uint8_t humidity = dht_data[0];
                uint8_t temperature = dht_data[2];
                
                // Final Command Center Printout!
                printf("Temp: %u C | Humid: %u%% | Vib/s: %lu | Light: %u\r\n", 
                       temperature, humidity, vibration_freq_hz, adc_result);
            }
            else {
                // Ignore silent parity drops from physical hardware noise
            }
        }
    }
}

// --- DHT11 START SIGNAL & CLEAN SLATE PROTOCOL ---
void dht11_start_signal (void){
    TIM4->DIER &= ~TIM_DIER_CC3IE; // Deaf to self-interrupts
    edge_count = 0;
    
    // Yell WAKE UP (Pull PB8 LOW for 18ms)
    GPIOB->MODER &= ~(3 << 16);
    GPIOB->MODER |=  (1 << 16);
    GPIOB->BSRR = (1<<24);
    HAL_Delay(18); 
    GPIOB->BSRR= (1<<8);

    // Hand over to TIM4_CH3 (AF2)
    GPIOB->MODER &= ~(3 << 16);
    GPIOB->MODER |= (2<<16);
    GPIOB->AFR[1] &= ~(0xF<<0);
    GPIOB->AFR[1] |= (2<<0);

    TIM4->CCMR2 &= ~(3 << 0);
    TIM4->CCMR2 |= (1<<0);         // TI3 -> IC3
    
    TIM4->CCER &= ~(0xF<<8);
    TIM4->CCER |= (1<<8) | (1<<9); // CC3E=1 (Enable), CC3P=1 (Falling)

    TIM4->SR &= ~TIM_SR_CC3IF;     // Wipe phantom edges
    
    // Now listen!
    TIM4->DIER |= TIM_DIER_CC3IE;
    NVIC_EnableIRQ(TIM4_IRQn);
    NVIC_SetPriority(TIM4_IRQn, 0);
}

// --- INTERRUPT SERVICE ROUTINE ---
void TIM4_IRQHandler(void){
    if (TIM4->SR & TIM_SR_CC3IF){
        capture_times[edge_count] = TIM4->CCR3;
        TIM4->CCER ^= (1<<9); // Toggle CC3P (Falling <-> Rising)
        edge_count++;

        if(edge_count >= 82){
            data_ready = 1;
            edge_count = 0;
            TIM4->DIER &= ~(1<<3); // Close gate to process array
        }
    }
}

// --- UART PRINTF REDIRECTION ---
void uart_write_byte(uint8_t c){
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len){
    for (int i=0; i<len; i++){
        uart_write_byte(ptr[i]);
    }
    return len;
}

/* SystemClock_Config and MX_GPIO_Init omitted for brevity - keep your auto-generated ones */
