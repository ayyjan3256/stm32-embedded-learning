/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  *
  * Week 2, Day 4 — USART2 bare-metal, polling-based TX/RX, echo loop.
  * Board: WeAct STM32F405RGT6 Blackpill.
  * UART wiring: ST-Link V2.1 (10-pin) TXD -> PA3 (RX), RXD -> PA2 (TX),
  *              shared GND, via breadboard. USART2 on PA2/PA3, AF7.
  * Clock: 16 MHz HSI, no PLL. APB1 = 16 MHz (DIV1), unreduced.
  * Baud: 115200 -> BRR = 0x8B (mantissa=8, fraction=11), ~0.08% error.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  Blocking transmit of a single byte over USART2.
 *         Waits for TXE (transmit data register empty) before loading DR.
 *         TXE is cleared by hardware the instant DR is written.
 */
void uart_write_byte(uint8_t c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

/**
 * @brief  Blocking receive of a single byte over USART2.
 *         Waits for RXNE (read data register not empty) before reading DR.
 *         RXNE is cleared by hardware the instant DR is read.
 *         NOTE: this function blocks indefinitely if no byte ever arrives.
 *         Interrupt-driven RX (Week 4, NVIC) removes this limitation.
 */
uint8_t uart_read_byte(void)
{
    while (!(USART2->SR & USART_SR_RXNE));
    return USART2->DR;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  RCC->APB1ENR |= (1 << 17);   // USART2EN
  RCC->AHB1ENR |= (1 << 0);    // GPIOAEN
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  /* USER CODE BEGIN 2 */

  /* PA2, PA3 -> alternate function mode (MODER = 10) */
  GPIOA->MODER |= (1 << 5);
  GPIOA->MODER &= ~(1 << 4);   // PA2
  GPIOA->MODER |= (1 << 7);
  GPIOA->MODER &= ~(1 << 6);   // PA3

  /* PA2, PA3 -> AF7 (USART2 TX/RX) */
  GPIOA->AFR[0] |= (7 << 8);    // PA2, bits [11:8]
  GPIOA->AFR[0] |= (7 << 12);   // PA3, bits [15:12]

  /* PA2, PA3 -> high speed output */
  GPIOA->OSPEEDR |= (3 << 4);   // PA2
  GPIOA->OSPEEDR |= (3 << 6);   // PA3

  /* BRR: 115200 baud @ 16 MHz APB1 (mantissa=8, fraction=11) */
  USART2->BRR = 139;   // 0x8B

  /* Enable USART2, transmitter, receiver */
  USART2->CR1 |= USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t received = uart_read_byte();
    uart_write_byte(received);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
