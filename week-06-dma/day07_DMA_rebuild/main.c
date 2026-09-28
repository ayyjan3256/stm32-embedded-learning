/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Week 6 - ADC1 (3-ch scan) -> DMA2 double buffer -> CSV -> USART2 via DMA1
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
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define N_CH          3                    /* PA0, PA1, PA4                                   */
#define N_SETS        33                   /* conversion sets per buffer                      */
#define BUF_LEN       (N_CH * N_SETS)      /* 99 items: multiple of 3, so every buffer        */
                                           /* starts on channel 0                             */
#define OUTPUT_STATS  0                    /* 0 = send raw samples (one CSV line per buffer)  */
                                           /* 1 = send mean/std per channel instead           */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Shared with ISRs: volatile, declared before the ISRs that use them */
volatile uint16_t buffer_a[BUF_LEN];
volatile uint16_t buffer_b[BUF_LEN];
volatile uint8_t  buffer_a_ready = 0;
volatile uint8_t  buffer_b_ready = 0;
volatile uint8_t  tx_done        = 1;
volatile uint8_t  adc_running    = 1;      /* toggled by the button ISR */
volatile uint8_t  dma_error      = 0;      /* set by either TEIF handler */
volatile uint32_t ms_ticks       = 0;      /* unused; kept in case another file references it.
                                              Debounce uses HAL_GetTick() */

/* Main-loop only */
uint8_t  last_adc_running = 1;
char     csv_buf[600];                     /* worst case 99 x "4095," = 495 chars */
uint8_t  message[] = "Hello this is UART-DMA!\n";
uint32_t ch_mean[N_CH];
uint32_t ch_std[N_CH];
uint32_t dropped_tx    = 0;                /* buffers skipped because UART was still busy */
uint32_t overrun_count = 0;                /* both buffers ready at once = fell behind    */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
void DMA2_ADC1_Init(void);
void USART2_Init(void);
void USART2_DMA1_Init(void);
void EXTI0_Init(void);
uint8_t USART2_Send(const void *data, uint16_t len);
static void adc_start(void);
static void adc_stop(void);
static void process_buffer(volatile uint16_t *buf);
static uint32_t isqrt(uint32_t x);
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
  DMA2_ADC1_Init();
  USART2_Init();
  USART2_DMA1_Init();
  EXTI0_Init();

  USART2_Send(message, sizeof(message) - 1);   /* hello line, one-shot DMA TX */
  while (!tx_done);

  adc_start();                                 /* stream EN, ADON, SWSTART last */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t run = adc_running;                 /* one snapshot per pass */

    /* Button toggled the pipeline: stop or cleanly restart the ADC stream */
    if (run != last_adc_running) {
        last_adc_running = run;
        if (run) adc_start();
        else     adc_stop();
    }

    if (run) {
        if (buffer_a_ready && buffer_b_ready) {
            overrun_count++;                   /* processing fell behind the fill rate */
        }
        if (buffer_a_ready) {
            buffer_a_ready = 0;
            process_buffer(buffer_a);
        }
        if (buffer_b_ready) {
            buffer_b_ready = 0;
            process_buffer(buffer_b);
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

/* ------------------------------------------------------------------------- */
/*  ADC1 (scan, 3 channels) -> DMA2 Stream0 Channel0, double-buffer mode      */
/*  Configures everything but leaves the stream disabled; adc_start() enables */
/* ------------------------------------------------------------------------- */
void DMA2_ADC1_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;         /* AHB1 */
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;          /* AHB1 */
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;          /* APB2 */

	GPIOA->MODER |= (3<<0) | (3<<2) | (3<<8);    /* PA0, PA1, PA4 analog */

	/* ADC clock: PCLK2 = 84 MHz, default /2 = 42 MHz exceeds the 36 MHz limit -> /4 = 21 MHz */
	ADC->CCR &= ~ADC_CCR_ADCPRE;
	ADC->CCR |= ADC_CCR_ADCPRE_0;

	ADC1->CR1  |= ADC_CR1_SCAN;
	ADC1->SQR1 &= ~(0xF<<20);
	ADC1->SQR1 |= (2<<20);                       /* L = 2 -> 3 conversions */
	ADC1->SQR3  = (0<<0) | (1<<5) | (4<<10);     /* ch0, ch1, ch4 */
	ADC1->SMPR2 &= ~((7<<0) | (7<<3) | (7<<12));
	ADC1->SMPR2 |=  (7<<0) | (7<<3) | (7<<12);   /* 480 cycles on all three */

	ADC1->CR2 |= ADC_CR2_CONT;                   /* convert continuously */
	ADC1->CR2 |= ADC_CR2_DMA;                    /* ADC issues DMA requests */
	ADC1->CR2 |= ADC_CR2_DDS;                    /* ...and keeps issuing them */

	/* DMA2 Stream0 Channel0 = ADC1 */
	DMA2_Stream0->CR &= ~DMA_SxCR_EN;
	while (DMA2_Stream0->CR & DMA_SxCR_EN);

	DMA2_Stream0->CR &= ~(7<<25);                /* CHSEL = 0, explicit */
	DMA2_Stream0->CR &= ~(3<<6);                 /* DIR = periph -> mem */
	DMA2_Stream0->CR &= ~(1<<9);                 /* PINC = 0 */
	DMA2_Stream0->CR |=  (1<<10);                /* MINC = 1 */
	DMA2_Stream0->CR &= ~((3<<11) | (3<<13));
	DMA2_Stream0->CR |=  (1<<11) | (1<<13);      /* PSIZE = MSIZE = halfword */
	DMA2_Stream0->CR |= DMA_SxCR_CIRC;
	DMA2_Stream0->CR |= DMA_SxCR_DBM;
	DMA2_Stream0->CR |= DMA_SxCR_TCIE | DMA_SxCR_TEIE;

	DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
	DMA2_Stream0->M0AR = (uint32_t)buffer_a;
	DMA2_Stream0->M1AR = (uint32_t)buffer_b;
	DMA2_Stream0->NDTR = BUF_LEN;

	NVIC_EnableIRQ(DMA2_Stream0_IRQn);
}

/* Start (or restart) the ADC stream. Stream must be disabled (EN == 0). */
static void adc_start(void){
	DMA2->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0
	            | DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;      /* clear stale flags */
	DMA2_Stream0->CR &= ~DMA_SxCR_CT;            /* first target = M0AR (writable only while EN = 0) */
	DMA2_Stream0->NDTR = BUF_LEN;                /* NDTR holds a partial count after a manual stop */
	buffer_a_ready = 0;
	buffer_b_ready = 0;

	ADC1->SR = 0;                                /* clear OVR / EOC / STRT */
	ADC1->CR2 &= ~ADC_CR2_DMA;                   /* toggle DMA so requests restart after an overrun */
	ADC1->CR2 |=  ADC_CR2_DMA;

	DMA2_Stream0->CR |= DMA_SxCR_EN;             /* stream first ... */
	ADC1->CR2 |= ADC_CR2_ADON;                   /* ... then power the ADC */
	for (volatile uint32_t i = 0; i < 2000; i++);/* tSTAB */
	ADC1->CR2 |= ADC_CR2_SWSTART;                /* trigger last */
}

static void adc_stop(void){
	ADC1->CR2 &= ~ADC_CR2_ADON;
	DMA2_Stream0->CR &= ~DMA_SxCR_EN;
	while (DMA2_Stream0->CR & DMA_SxCR_EN);      /* hardware clears EN when in-flight transfer ends */
}

/* ------------------------------------------------------------------------- */
/*  USART2 (PA2 TX / PA3 RX, AF7) on APB1, 115200 @ PCLK1 = 42 MHz             */
/* ------------------------------------------------------------------------- */
void USART2_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;         /* AHB1 */
	RCC->APB1ENR |= RCC_APB1ENR_USART2EN;        /* APB1 */

	GPIOA->MODER &= ~((3<<4) | (3<<6));
	GPIOA->MODER |=  (2<<4) | (2<<6);            /* alternate function */
	GPIOA->AFR[0] &= ~((0xF<<8) | (0xF<<12));
	GPIOA->AFR[0] |=  (7<<8) | (7<<12);          /* AF7 = USART2 */

	USART2->BRR = 365;                           /* 42 MHz / (16 x 115200) = 22.79 -> 0x16D */
	USART2->CR3 |= USART_CR3_DMAT;               /* USART issues DMA TX requests */
	USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
}

/* DMA1 Stream6 Channel4 = USART2_TX. One-shot: CIRC off, NDTR reloaded per send. */
void USART2_DMA1_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

	DMA1_Stream6->CR &= ~DMA_SxCR_EN;
	while (DMA1_Stream6->CR & DMA_SxCR_EN);

	DMA1_Stream6->CR &= ~(3<<6);
	DMA1_Stream6->CR |=  (1<<6);                 /* DIR = mem -> periph */
	DMA1_Stream6->CR &= ~(1<<8);                 /* CIRC = 0 */
	DMA1_Stream6->CR &= ~(1<<9);                 /* PINC = 0 */
	DMA1_Stream6->CR |=  (1<<10);                /* MINC = 1 */
	DMA1_Stream6->CR &= ~((3<<11) | (3<<13));    /* PSIZE = MSIZE = byte */
	DMA1_Stream6->CR &= ~(7<<25);
	DMA1_Stream6->CR |=  (4<<25);                /* CHSEL = 4 */
	DMA1_Stream6->CR |= DMA_SxCR_TCIE | DMA_SxCR_TEIE;

	DMA1_Stream6->PAR = (uint32_t)(&USART2->DR);

	NVIC_EnableIRQ(DMA1_Stream6_IRQn);
	/* No EN here: USART2_Send() sets M0AR/NDTR and enables the stream. */
}

/* Starts a DMA transmission. Returns 1 if started, 0 if the previous one is still running. */
uint8_t USART2_Send(const void *data, uint16_t len){
	if (!tx_done || len == 0) return 0;
	while (DMA1_Stream6->CR & DMA_SxCR_EN);      /* EN already 0 once tx_done is set */
	tx_done = 0;                                 /* before EN so the ISR can't be overwritten */
	DMA1->HIFCR = DMA_HIFCR_CFEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CTEIF6
	            | DMA_HIFCR_CHTIF6 | DMA_HIFCR_CTCIF6;
	DMA1_Stream6->M0AR = (uint32_t)data;
	DMA1_Stream6->NDTR = len;                    /* reloaded on every send */
	DMA1_Stream6->CR  |= DMA_SxCR_EN;            /* EN last */
	return 1;
}

/* ------------------------------------------------------------------------- */
/*  Button (PB0, EXTI0, falling edge, pull-up) and error LED (PB2)             */
/* ------------------------------------------------------------------------- */
void EXTI0_Init(void){
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	GPIOB->MODER &= ~(3<<0);                     /* PB0 input */
	GPIOB->PUPDR &= ~(3<<0);
	GPIOB->PUPDR |=  (1<<0);                     /* pull-up */

	GPIOB->MODER &= ~(3<<4);
	GPIOB->MODER |=  (1<<4);                     /* PB2 output (error LED) */

	SYSCFG->EXTICR[0] &= ~(0xF<<0);
	SYSCFG->EXTICR[0] |=  (0x1<<0);              /* EXTI0 <- port B */

	EXTI->FTSR |= (1<<0);
	EXTI->PR    = (1<<0);                        /* clear any pending flag first */
	EXTI->IMR  |= (1<<0);

	NVIC_SetPriority(EXTI0_IRQn, 1);
	NVIC_EnableIRQ(EXTI0_IRQn);
}

/* ------------------------------------------------------------------------- */
/*  Buffer processing: per-channel mean/std, then CSV out over UART DMA        */
/* ------------------------------------------------------------------------- */
static uint32_t isqrt(uint32_t x){
	uint32_t r = 0, bit = 1u << 30;
	while (bit > x) bit >>= 2;
	while (bit) {
		if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
		else              { r >>= 1; }
		bit >>= 2;
	}
	return r;
}

static void process_buffer(volatile uint16_t *buf){
	uint32_t sum[N_CH] = {0}, sumsq[N_CH] = {0};

	for (int i = 0; i < BUF_LEN; i++) {          /* interleaved: ch0, ch1, ch4, ch0, ... */
		uint32_t v = buf[i];
		int c = i % N_CH;
		sum[c]   += v;
		sumsq[c] += v * v;
	}
	for (int c = 0; c < N_CH; c++) {
		uint32_t mean = sum[c] / N_SETS;
		uint32_t var  = sumsq[c] / N_SETS - mean * mean;
		ch_mean[c] = mean;
		ch_std[c]  = isqrt(var);
	}

	/* Check BEFORE formatting: csv_buf is the DMA source while a send is in flight. */
	if (!tx_done) {
		dropped_tx++;
		return;
	}

	int pos = 0;
#if OUTPUT_STATS
	pos = sprintf(csv_buf, "%lu,%lu,%lu,%lu,%lu,%lu\n",
	              (unsigned long)ch_mean[0], (unsigned long)ch_std[0],
	              (unsigned long)ch_mean[1], (unsigned long)ch_std[1],
	              (unsigned long)ch_mean[2], (unsigned long)ch_std[2]);
#else
	for (int i = 0; i < BUF_LEN; i++) {
		pos += sprintf(&csv_buf[pos], "%u,", (unsigned)buf[i]);
	}
	csv_buf[pos - 1] = '\n';                     /* last comma becomes newline */
#endif
	USART2_Send(csv_buf, (uint16_t)pos);
}

/* ------------------------------------------------------------------------- */
/*  Interrupt handlers                                                          */
/* ------------------------------------------------------------------------- */
void DMA2_Stream0_IRQHandler(void){
	if (DMA2->LISR & DMA_LISR_TCIF0) {
		DMA2->LIFCR = DMA_LIFCR_CTCIF0;
		if (DMA2_Stream0->CR & DMA_SxCR_CT) {    /* CT == 1: DMA now on buffer_b, buffer_a finished */
			buffer_a_ready = 1;
		} else {                                 /* CT == 0: DMA now on buffer_a, buffer_b finished */
			buffer_b_ready = 1;
		}
	}
	if (DMA2->LISR & DMA_LISR_TEIF0) {           /* hardware has cleared EN: the stream is stopped */
		DMA2->LIFCR = DMA_LIFCR_CTEIF0;
		dma_error = 1;
		GPIOB->BSRR = (1<<2);                    /* error LED on */
	}
}

void DMA1_Stream6_IRQHandler(void){
	if (DMA1->HISR & DMA_HISR_TCIF6) {
		DMA1->HIFCR = DMA_HIFCR_CTCIF6;
		tx_done = 1;
	}
	if (DMA1->HISR & DMA_HISR_TEIF6) {
		DMA1->HIFCR = DMA_HIFCR_CTEIF6;
		dma_error = 1;
		tx_done = 1;                             /* transfer aborted; let TX recover */
		GPIOB->BSRR = (1<<2);                    /* error LED on */
	}
}

void EXTI0_IRQHandler(void){
	if (EXTI->PR & (1<<0)) {
		EXTI->PR = (1<<0);

		static uint32_t last_press_time = 0;
		uint32_t now = HAL_GetTick();
		if ((now - last_press_time) > 50) {      /* 50 ms debounce */
			adc_running = !adc_running;
			last_press_time = now;
		}
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
