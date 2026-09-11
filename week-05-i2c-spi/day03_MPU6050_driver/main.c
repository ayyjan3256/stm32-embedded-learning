/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Week 5, Day 3 - Register Read/Write Functions & Burst Read
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
void USART2_Init(void);
void I2C1_WriteReg(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t data);
uint8_t I2C1_ReadReg(uint8_t dev_Addr, uint8_t reg_Addr);
void I2C1_ReadRegs(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t *buffer, uint8_t len);
void print_fixed2(const char *label, float val);
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
  USART2_Init();
  I2C1_Init();

  uint8_t raw_data[14];

  // 1. Wake up the sensor
  I2C1_WriteReg(0x68, 0x6B, 0x00);
  printf("Sensor Woken Up!!\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  while (1)
  {
      // 2. Perform 14-byte Burst Read
      I2C1_ReadRegs(0x68, 0x3B, raw_data, 14);

      // 3. Reconstruct 16-bit Big-Endian values
      int16_t accel_x = (raw_data[0] << 8)  | raw_data[1];
      int16_t accel_y = (raw_data[2] << 8)  | raw_data[3];
      int16_t accel_z = (raw_data[4] << 8)  | raw_data[5];
      int16_t temp    = (raw_data[6] << 8)  | raw_data[7];
      int16_t gyro_x  = (raw_data[8] << 8)  | raw_data[9];
      int16_t gyro_y  = (raw_data[10] << 8) | raw_data[11];
      int16_t gyro_z  = (raw_data[12] << 8) | raw_data[13];

      // 4. Convert to Physical Units
      float ax_g = accel_x / 16384.0f;
      float ay_g = accel_y / 16384.0f;
      float az_g = accel_z / 16384.0f;

      float gx_dps = gyro_x / 131.0f;
      float gy_dps = gyro_y / 131.0f;
      float gz_dps = gyro_z / 131.0f;

      // 5. Print Results
      print_fixed2("Accel X", ax_g); printf(" ");
      print_fixed2("Accel Y", ay_g); printf(" ");
      print_fixed2("Accel Z", az_g); printf(" ");
      print_fixed2("Gyro X", gx_dps); printf(" ");
      print_fixed2("Gyro Y", gy_dps); printf(" ");
      print_fixed2("Gyro Z", gz_dps); printf(" ");
      printf("Temp: %d\r\n", temp);

      for(volatile int delay = 0; delay < 500000; delay++);
  }
}

void I2C1_WriteReg(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t data){
    I2C1->CR1 |= (1<<8);
    while(!(I2C1->SR1 & (1<<0)));

    I2C1->DR = dev_Addr<<1 | 0;
    while(!(I2C1->SR1 & (1<<1)));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void) temp;

    I2C1->DR = reg_Addr;
    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = data;
    while(!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= (1<<9);
}

uint8_t I2C1_ReadReg(uint8_t dev_Addr, uint8_t reg_Addr){
    uint8_t data = 0;

    I2C1->CR1 |= (1<<8);
    while(!(I2C1->SR1 & (1<<0)));

    I2C1->DR = dev_Addr<<1 | 0;
    while(!(I2C1->SR1 & (1<<1)));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void) temp;

    I2C1->DR = reg_Addr;
    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->CR1 |= (1<<8); // Repeated Start
    while(!(I2C1->SR1 & (1<<0)));

    I2C1->DR = dev_Addr<<1 | 1; // Read Mode
    while(!(I2C1->SR1 & (1<<1)));

    I2C1->CR1 &= ~I2C_CR1_ACK;
    temp = I2C1->SR1; temp = I2C1->SR2; (void) temp;
    I2C1->CR1 |= (1<<9);

    while(!(I2C1->SR1 & I2C_SR1_RXNE));
    data = I2C1->DR;

    return data;
}

void I2C1_ReadRegs(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t *buffer, uint8_t len){
    I2C1->CR1 |= (1<<8);
    while(!(I2C1->SR1 & (1<<0)));

    I2C1->DR = dev_Addr<<1 | 0;
    while(!(I2C1->SR1 & (1<<1)));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void) temp;

    I2C1->DR = reg_Addr;
    while(!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->CR1 |= (1<<8);
    while(!(I2C1->SR1 & (1<<0)));

    I2C1->DR = dev_Addr<<1 | 1;
    while(!(I2C1->SR1 & (1<<1)));

    I2C1->CR1 |= I2C_CR1_ACK;
    temp = I2C1->SR1; temp = I2C1->SR2; (void) temp;

    while(len > 0){
        if (len == 1){
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= (1<<9);
        }
        while(!(I2C1->SR1 & I2C_SR1_RXNE));
        *buffer = I2C1->DR;
        buffer++;
        len--;
    }
}

void I2C1_Init(void){
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER |= (2<<12) | (2<<14);
    GPIOB->AFR[0] |= (4<<24) | (4<<28);
    GPIOB->OTYPER |= (1<<6) | (1<<7);
    GPIOB->OSPEEDR |= (3<<12) | (3<<14);
    GPIOB->PUPDR |= (1<<12) | (1<<14);

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

void USART2_Init(void){
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER |= (2<<4) | (2<<6);
    GPIOA->AFR[0] |= (7<<8) | (7<<12);

    USART2->BRR = 365;
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
}

void print_fixed2(const char *label, float val) {
    int sign = (val < 0) ? 1 : 0;
    float aval = sign ? -val : val;
    int scaled = (int)(aval * 100.0f + 0.5f);
    printf("%s=%s%d.%02d\r\n", label, sign ? "-" : "", scaled / 100, scaled % 100);
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
