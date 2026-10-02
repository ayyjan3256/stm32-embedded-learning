/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Week 7 - fixed-rate photoresistor sampling for Edge Impulse
  *
  * Signal path:
  *   TIM2 update event (TRGO) -> ADC1 ch0 (PA0) single conversion
  *   -> DMA2 Stream0 (circular, 16-bit) -> buffer[BUFSIZE]
  *   -> half/full-transfer flags -> main loop -> USART1 TX, one value per line
  *
  * Rate: 84 MHz / ((PSC+1)(ARR+1)) = 84 MHz / (8400 * 50) = 200 Hz
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
#define BUFSIZE            100
#define HALF               (BUFSIZE / 2)
#define DEBUG_TOGGLE_PIN   1      /* 1 = toggle PB0 on every DMA half/full event (logic analyzer check) */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
uint16_t buffer[BUFSIZE];
volatile uint8_t half_ready = 0;
volatile uint8_t full_ready = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
void ADC1_TIM2_DMA2_Init(void);
void UART1_Init(void);
void uart_putc(char c);
void uart_send_sample(uint16_t v);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  UART1_Init();              /* UART first, so no sample is produced before it can be sent */
  ADC1_TIM2_DMA2_Init();     /* starts sampling at 200 Hz */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (half_ready)
    {
      half_ready = 0;
      for (uint16_t i = 0; i < HALF; i++)
      {
        uart_send_sample(buffer[i]);
      }
    }
    if (full_ready)
    {
      full_ready = 0;
      for (uint16_t i = HALF; i < BUFSIZE; i++)
      {
        uart_send_sample(buffer[i]);
      }
    }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * USART1 TX on PA9 (AF7), 115200 baud, 8N1.
  * USART1 is on APB2 = 84 MHz: BRR = 84e6 / 115200 = 729.17 -> 729.
  * If your USB-serial adapter is wired to a different USART, change this function.
  */
void UART1_Init(void)
{
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
  RCC->APB2ENR |= RCC_APB2ENR_USART1EN;

  GPIOA->MODER &= ~(3U << (9 * 2));
  GPIOA->MODER |=  (2U << (9 * 2));          /* PA9 alternate function */
  GPIOA->AFR[1] &= ~(0xFU << ((9 - 8) * 4));
  GPIOA->AFR[1] |=  (7U   << ((9 - 8) * 4)); /* AF7 = USART1 */

  USART1->BRR = 729;
  USART1->CR1 = USART_CR1_TE | USART_CR1_UE; /* TX only, 8 data bits, no parity, 1 stop */
}

void uart_putc(char c)
{
  while (!(USART1->SR & USART_SR_TXE))
  {
  }
  USART1->DR = (uint8_t)c;
}

/* Sends one 12-bit sample as decimal text followed by '\n' (up to 5 bytes). */
void uart_send_sample(uint16_t v)
{
  char tmp[5];
  int n = 0;

  if (v == 0)
  {
    uart_putc('0');
  }
  else
  {
    while (v > 0)
    {
      tmp[n++] = (char)('0' + (v % 10));
      v /= 10;
    }
    while (n > 0)
    {
      n--;
      uart_putc(tmp[n]);
    }
  }
  uart_putc('\n');
}

/**
  * Timer-triggered ADC1 channel 0 (PA0) with circular DMA.
  * Startup order matters: DMA -> ADC (ADON last) -> TIM2 (CEN last).
  */
void ADC1_TIM2_DMA2_Init(void)
{
  /* Clocks, read-modify-write */
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_DMA2EN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

  /* PA0 analog input, PB0 debug output */
  GPIOA->MODER |= (3U << (0 * 2));
  GPIOB->MODER &= ~(3U << (0 * 2));
  GPIOB->MODER |=  (1U << (0 * 2));

  /* ---- DMA2 Stream0, Channel 0 (ADC1) ---- */
  DMA2_Stream0->CR &= ~DMA_SxCR_EN;
  while (DMA2_Stream0->CR & DMA_SxCR_EN)
  {
  }
  DMA2->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0
              | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;

  DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;
  DMA2_Stream0->M0AR = (uint32_t)buffer;
  DMA2_Stream0->NDTR = BUFSIZE;

  DMA2_Stream0->CR &= ~(DMA_SxCR_CHSEL | DMA_SxCR_DIR | DMA_SxCR_PINC
                      | DMA_SxCR_PSIZE | DMA_SxCR_MSIZE);   /* ch0, periph->mem, fixed periph addr */
  DMA2_Stream0->CR |= DMA_SxCR_CIRC | DMA_SxCR_MINC
                    | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0    /* 16-bit both sides */
                    | DMA_SxCR_HTIE | DMA_SxCR_TCIE;

  NVIC_SetPriority(DMA2_Stream0_IRQn, 1);
  NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  DMA2_Stream0->CR |= DMA_SxCR_EN;

  /* ---- ADC1 ---- */
  ADC123_COMMON->CCR &= ~ADC_CCR_ADCPRE;
  ADC123_COMMON->CCR |= ADC_CCR_ADCPRE_0;     /* PCLK2 / 4 = 21 MHz (limit is 36 MHz) */

  ADC1->CR2  = 0;                             /* clear first: CONT = 0, no leftover trigger bits */
  ADC1->SQR1 = 0;                             /* L = 0: one conversion in the sequence */
  ADC1->SQR3 = 0;                             /* SQ1 = channel 0 (PA0) */
  ADC1->SMPR2 &= ~ADC_SMPR2_SMP0;
  ADC1->SMPR2 |=  ADC_SMPR2_SMP0;             /* 480 cycles, suits a high-impedance divider */

  ADC1->CR2 |= ADC_CR2_DMA | ADC_CR2_DDS;     /* DMA requests keep coming in circular mode */
  ADC1->CR2 |= (6U << ADC_CR2_EXTSEL_Pos);    /* EXTSEL = 0110: TIM2 TRGO */
  ADC1->CR2 |= (1U << ADC_CR2_EXTEN_Pos);     /* EXTEN  = 01: rising edge */
  ADC1->CR2 |= ADC_CR2_ADON;                  /* last; no SWSTART, the timer starts each conversion */

  /* ---- TIM2: 200 Hz update event -> TRGO ---- */
  TIM2->PSC = 8399;
  TIM2->ARR = 49;
  TIM2->CR2 = (TIM2->CR2 & ~TIM_CR2_MMS) | (2U << TIM_CR2_MMS_Pos);   /* MMS = 010: update -> TRGO */
  TIM2->EGR |= TIM_EGR_UG;                    /* load PSC/ARR now */
  TIM2->CNT = 0;
  TIM2->CR1 |= TIM_CR1_CEN;                   /* start sampling */
}

/* ISR: flags only (plus the optional debug toggle). All formatting/sending is in main. */
void DMA2_Stream0_IRQHandler(void)
{
  if (DMA2->LISR & DMA_LISR_HTIF0)
  {
    DMA2->LIFCR = DMA_LIFCR_CHTIF0;
    half_ready = 1;
#if DEBUG_TOGGLE_PIN
    GPIOB->ODR ^= (1U << 0);
#endif
  }
  if (DMA2->LISR & DMA_LISR_TCIF0)
  {
    DMA2->LIFCR = DMA_LIFCR_CTCIF0;
    full_ready = 1;
#if DEBUG_TOGGLE_PIN
    GPIOB->ODR ^= (1U << 0);
#endif
  }
}

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
