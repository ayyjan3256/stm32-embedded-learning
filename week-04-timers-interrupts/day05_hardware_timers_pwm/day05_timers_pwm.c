/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
#define BUFFER_SIZE 64
char rx_buffer[BUFFER_SIZE];
volatile uint8_t rx_index = 0;

volatile uint8_t command_ready = 0;

volatile uint8_t breathing_mode = 0;
int16_t current_brightness = 0;
int16_t fade_step = 10;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void uart_write_byte(uint8_t c);
int _write(int file, char *ptr, int len);

/* USER CODE BEGIN PFP */
void PWM_Init(void);
void USART_RX_Interrupt_Init(void);
/* USER CODE END PFP */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */
  PWM_Init();
  USART_RX_Interrupt_Init();
  
  printf("\r\n--- STM32 PWM & CLI Booted ---\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (command_ready == 1){

		  uint32_t new_dim = 0;
		  
		  if((strcmp(rx_buffer,"LED ON")) == 0){
			  TIM2->CCR1 = 999; 
			  breathing_mode = 0;
			  printf("LED is ON (100%%)\r\n");
		  }
		  else if((strcmp(rx_buffer,"LED OFF")) == 0){
			  TIM2->CCR1 = 0; 
			  breathing_mode = 0;
			  printf("LED is OFF (0%%)\r\n");
		  }
		  else if((strcmp(rx_buffer,"STATUS")) == 0){
			  printf("The Current Duty Cycle is: %lu /1000\r\n", TIM2->CCR1);
		  }
		  else if(sscanf(rx_buffer,"DIM %lu", &new_dim) == 1){
			  	if(new_dim <= 100){
			  		 breathing_mode = 0;
			  		 TIM2->CCR1 = new_dim * 10;
			  		 printf("LED dimmed to %lu percent\r\n", new_dim);
			  	}
			  	else{
			  		printf("ERROR: DIM value MUST be between 0 and 100\r\n");
			  	}
		  }
		  else if (strcmp(rx_buffer, "BREATHE ON") == 0) {
		      breathing_mode = 1;
		      printf("Breathing mode activated!\r\n");
		  }
		  else if (strcmp(rx_buffer, "BREATHE OFF") == 0) {
		      breathing_mode = 0;
		      TIM2->CCR1 = 0; 
		      printf("Breathing mode deactivated!\r\n");
		  }
		  else if (strlen(rx_buffer) > 0){
			  printf("UNKNOWN COMMAND: %s\r\n", rx_buffer);
		  }
		  command_ready = 0;
	  }
  }
  /* USER CODE END 3 */
}

/* USER CODE BEGIN 4 */
void PWM_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	GPIOA->MODER |= (2<<0);
	GPIOA->AFR[0] |= (1<<0);

	TIM2->PSC = 83;
	TIM2->ARR = 999;

	TIM2->CCR1 = 250;
	TIM2->CCMR1 |= (6<<4) | (1<<3);
	TIM2->CCER |= TIM_CCER_CC1E;
	TIM2->CR1 |= TIM_CR1_CEN;

	TIM2->DIER |= TIM_DIER_UIE;

	NVIC_EnableIRQ(TIM2_IRQn);
	NVIC_SetPriority(TIM2_IRQn, 1);
}

void TIM2_IRQHandler(void){
	if(TIM2->SR & TIM_SR_UIF){
		TIM2->SR &= ~TIM_SR_UIF;
		
		if(breathing_mode == 1){
			 static uint8_t tick_counter = 0;
			 tick_counter++;

			 if(tick_counter >= 10){
				  tick_counter = 0;
				  current_brightness += fade_step;

				  if(current_brightness >= 1000){
					  current_brightness = 1000;
					  fade_step = -fade_step;
				  }
				  else if (current_brightness <= 0){
					  current_brightness = 0;
					  fade_step = -fade_step;
				   }
			 }
			 TIM2->CCR1 = current_brightness;
		}
	}
}

void USART_RX_Interrupt_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

	GPIOA->MODER |= (2<<4) | (2<<6);
	GPIOA->AFR[0] |= (7<<8) | (7<<12);

	USART2->BRR = 365;
	USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE;

	NVIC_EnableIRQ(USART2_IRQn);
	NVIC_SetPriority(USART2_IRQn, 0);
}

void USART2_IRQHandler(void){
	if(USART2->SR & USART_SR_RXNE){
		uint8_t c = USART2->DR;
		uart_write_byte(c);

		if (c == '\r' || c == '\n'){
			uart_write_byte('\n');
			rx_buffer[rx_index] = '\0';
			rx_index = 0;
			command_ready = 1;
		}
		else{
			if(rx_index < BUFFER_SIZE - 1){
				rx_buffer[rx_index++] = c;
			}
		}
	}
}

void uart_write_byte(uint8_t c){
	while(!(USART2->SR & USART_SR_TXE));
	USART2->DR = c;
}

int _write(int file, char *ptr, int len){
	for (int i = 0; i < len; i++){
		uart_write_byte(ptr[i]);
	}
	return len;
}
/* USER CODE END 4 */

// (SystemClock_Config, MX_GPIO_Init, and Error_Handler remain unchanged)
