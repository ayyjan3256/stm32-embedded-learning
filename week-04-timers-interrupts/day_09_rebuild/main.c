/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body (Week 4 Final Rebuild & Boss Level)
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <math.h>

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile uint32_t vibration_count = 0;
volatile uint16_t adc_result = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
int _write(int file, char *ptr, int len);
void uart_write_byte(char c);

/* USER CODE BEGIN 0 */
/* USER CODE END 0 */

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();

  /* USER CODE BEGIN 2 */
  // 1. Enable Clocks
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_TIM4EN | RCC_APB1ENR_TIM2EN | RCC_APB1ENR_USART2EN;
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN | RCC_APB2ENR_ADC1EN;

  // 2. GPIO Configuration
  // PB2: Output (Onboard LED for EXTI / Watchdog testing)
  GPIOB->MODER |= (1<<4);
  GPIOB->MODER &= ~(1<<5);

  // PA0: Input with Pull-up (Push Button)
  GPIOA->MODER &= ~(3<<0);
  GPIOA->PUPDR |= (1<<0);
  GPIOA->PUPDR &= ~(1<<1);

  // PA1: Input with Pull-up (Vibration Sensor)
  GPIOA->MODER &= ~(3<<2);
  GPIOA->PUPDR |= (1<<2);
  GPIOA->PUPDR &= ~(1<<3);

  // PA4: Analog Mode (Photoresistor for ADC Channel 4)
  GPIOA->MODER |= (3<<8);

  // PA2 / PA3: Alternate Function 7 (USART2 TX/RX)
  GPIOA->MODER |= (2<<4) | (2<<6);
  GPIOA->AFR[0] |= (7<<8) | (7<<12);

  // PB6: Alternate Function 2 (TIM4 Channel 1 PWM output)
  GPIOB->MODER |= (2<<12);
  GPIOB->AFR[0] |= (2<<24);

  // 3. USART2 Configuration (115200 Baud)
  USART2->BRR = 365;
  USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;

  // 4. EXTI Configuration (PA0 and PA1)
  SYSCFG->EXTICR[0] &= ~(0xF<<0); // Map EXTI0 to PA0
  SYSCFG->EXTICR[0] &= ~(0xF<<4); // Map EXTI1 to PA1
  EXTI->IMR |= (1<<0) | (1<<1);   // Unmask EXTI0 and EXTI1
  EXTI->FTSR |= (1<<0) | (1<<1);  // Falling edge trigger
  
  NVIC_EnableIRQ(EXTI0_IRQn);
  NVIC_SetPriority(EXTI0_IRQn, 0);
  NVIC_EnableIRQ(EXTI1_IRQn);
  NVIC_SetPriority(EXTI1_IRQn, 1);

  // 5. TIM2 Configuration (1Hz Interrupt Metronome)
  TIM2->PSC = 8399;
  TIM2->ARR = 9999;
  TIM2->DIER |= TIM_DIER_UIE;
  TIM2->CR1 |= TIM_CR1_CEN;
  NVIC_EnableIRQ(TIM2_IRQn);
  NVIC_SetPriority(TIM2_IRQn, 2);

  // 6. TIM4 Configuration (1kHz PWM with Preload)
  TIM4->PSC = 83;
  TIM4->ARR = 999;
  TIM4->CCMR1 |= (6<<4);       // PWM Mode 1
  TIM4->CCER |= TIM_CCER_CC1E; // Enable Output
  TIM4->CR1 |= (1<<7);         // ARPE: Auto-reload preload enable
  TIM4->CCMR1 |= (1<<3);       // OC1PE: Output compare preload enable
  TIM4->CCR1 = 50;
  TIM4->CR1 |= TIM_CR1_CEN;

  // 7. TIM3 Configuration (100Hz Hardware Trigger for ADC)
  TIM3->PSC = 839;
  TIM3->ARR = 999;
  TIM3->CR2 |= (2<<4); // MMS = 010 (Update Event triggers TRGO)
  TIM3->CR1 |= TIM_CR1_CEN;

  // 8. ADC1 Configuration (Hardware Triggered + Analog Watchdog)
  ADC1->SQR3 = 4;        // Channel 4
  ADC1->CR1 |= (1<<5);   // EOCIE: End of Conversion Interrupt Enable
  ADC1->CR2 |= (1<<28) | (8<<24); // EXTEN = 01 (Rising Edge), EXTSEL = TIM3 TRGO
  
  // Analog Watchdog Setup
  ADC1->HTR = 3000;      // High Threshold
  ADC1->LTR = 1000;      // Low Threshold
  ADC1->CR1 |= (4<<0) | (1<<9) | (1<<23) | (1<<6); // AWDCH=4, AWDSGL=1, AWDEN=1, AWDIE=1
  
  ADC1->CR2 |= (1<<0);   // ADON: Turn ADC On
  
  NVIC_EnableIRQ(ADC_IRQn);
  NVIC_SetPriority(ADC_IRQn, 4);

  // 9. Generate Gamma Correction Table
  uint16_t gamma_table[101];
  for (int i = 0; i <= 100; i++){
      gamma_table[i] = (uint16_t)(pow((float)i / 100.0f, 2.2f) * 1000.0f);
  }
  
  // Signed ints for math safety
  int brightness = 0;
  int fade_dir = 1;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // PWM LED Breathing Animation
      TIM4->CCR1 = gamma_table[brightness];
      brightness += fade_dir;

      // Reverse fade direction at min/max bounds
      if(brightness >= 100 || brightness <= 0){
          fade_dir = -fade_dir;
      }

      // Stream Multi-Sensor Telemetry
      printf("%u,%lu\r\n", adc_result, vibration_count);
      vibration_count = 0; // Reset for next timeframe
      
      // 50 FPS Animation / Data Rate
      HAL_Delay(20);
  }
  /* USER CODE END 3 */
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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

static void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOH_CLK_ENABLE();
}

/* USER CODE BEGIN 4 */
void uart_write_byte(char c){
    while(!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len){
    for(int i = 0; i < len; i++){
        uart_write_byte(ptr[i]);
    }
    return len;
}

void EXTI0_IRQHandler(void){
    if(EXTI->PR & (1<<0)){
        static uint32_t last_pressed_time = 0;
        if(HAL_GetTick() - last_pressed_time > 50){
            GPIOB->ODR ^= (1<<2); // Toggle test LED
            last_pressed_time = HAL_GetTick();
        }
        EXTI->PR = (1<<0); // Clear Flag
    }
}

void EXTI1_IRQHandler(void){
    if(EXTI->PR & (1<<1)){
        vibration_count++;
        EXTI->PR = (1<<1); // Clear Flag
    }
}

void TIM2_IRQHandler(void){
    if(TIM2->SR & TIM_SR_UIF){
        TIM2->SR &= ~TIM_SR_UIF; // Clear Flag
    }
}

void ADC_IRQHandler(void){
    // Standard Conversion Complete
    if (ADC1->SR & ADC_SR_EOC){
        adc_result = ADC1->DR; // Read automatically clears EOC flag
    }

    // Hardware Analog Watchdog Alarm
    if(ADC1->SR & (1<<0)){ // AWD Flag
        GPIOB->ODR ^= (1<<2); // Flash warning LED
        printf("WATCHDOG ALARM: Light crossed threshold! Value: %u\r\n", adc_result);
        ADC1->SR &= ~(1<<0);  // Clear Watchdog Flag
    }
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}
