/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Week 5, Day 2 - I2C Bus Bring-Up & Device Probe
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void I2C1_Init(void);
int _write(int file, char *ptr, int len);
void uart_write_byte(char c);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */
  // 1. GPIO Configuration for I2C1 (PB6/PB7)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
  GPIOB->MODER |= (2<<12) | (2<<14);
  GPIOB->AFR[0] |= (4<<24) | (4<<28);
  GPIOB->OTYPER |= (1<<6) | (1<<7); // Open-drain
  GPIOB->OSPEEDR |= (3<<12) | (3<<14);
  GPIOB->PUPDR |= (1<<12) | (1<<14);

  // 2. UART2 Configuration for printf (PA2/PA3)
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  GPIOA->MODER |= (2<<4) | (2<<6);
  GPIOA->AFR[0] |= (7<<8) | (7<<12);
  USART2->BRR= 365;
  USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
  
  // 3. Initialize I2C Peripheral
  I2C1_Init();

  // 4. Bus Scanner Implementation
  for (int addr=0; addr<128; addr++){
      I2C1->CR1 |= I2C_CR1_START;
      while (!(I2C1->SR1 & I2C_SR1_SB));

      I2C1->DR = (addr<<1) | 0;
      while(!(I2C1->SR1 & I2C_SR1_ADDR) && !(I2C1->SR1 & I2C_SR1_AF));

      if(I2C1->SR1 & I2C_SR1_ADDR){
          volatile uint32_t temp = (I2C1->SR1);
          temp = I2C1->SR2;
          (void)temp;
          printf("Found device at address: 0x%02X\r\n", addr);
      }
      else{
          I2C1->SR1 &= ~(I2C_SR1_AF); // Clear Acknowledge Failure flag
      }

      I2C1->CR1 |= I2C_CR1_STOP;
      for(volatile int delay = 0; delay < 10000; delay++);
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
  }
}

void I2C1_Init(void){
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 &= ~(0x3F);
    I2C1->CR2 |= 42;

    I2C1->CCR &= ~(0xFFF);
    I2C1->CCR |= 210;

    I2C1->TRISE &= ~(0x3F);
    I2C1->TRISE |= 43;

    I2C1->CR1 |= I2C_CR1_PE;
}

void uart_write_byte(char c){
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len){
    for(int i=0; i<len; i++){
        uart_write_byte(ptr[i]);
    }
    return len;
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) { Error_Handler(); }
}
static void MX_GPIO_Init(void) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
void Error_Handler(void) { __disable_irq(); while (1) { } }
