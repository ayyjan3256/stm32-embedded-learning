/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — Week 5 Day 4: I2C1 + MPU6050 + PCF8574/HD44780 LCD
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
#include <stdio.h>
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
static void MX_GPIO_Init(void);

void I2C1_Init(void);
void I2C1_WriteReg(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t data);
void I2C1_WriteByte(uint8_t dev_Addr, uint8_t data); // for register-less devices like the PCF8574
uint8_t I2C1_ReadReg(uint8_t dev_Addr, uint8_t reg_Addr);
void I2C1_ReadRegs(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t *buffer, uint8_t len);

void USART2_Init(void);
void uart_write_byte(char c);
int _write(int file, char *ptr, int len);
void print_fixed2(const char *label, float val);

void LCD_PulseEnable(uint8_t byte);
void LCD_SendNibble(uint8_t nibble, uint8_t rs);
void LCD_SendByte(uint8_t byte, uint8_t rs);
void LCD_Init(void);
void LCD_Print(char *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_PrintLine(uint8_t row, const char *str);

/* USER CODE BEGIN PFP */

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
  USART2_Init();
  I2C1_Init();

  printf("I2C1 + LCD init done\r\n");

  LCD_Init();
  LCD_SetCursor(0, 0);
  LCD_Print("Hello");
  HAL_Delay(1000);

  // Wake the MPU6050 (PWR_MGMT_1 = 0x00, clears the default sleep-mode bit)
  I2C1_WriteReg(0x68, 0x6B, 0x00);
  printf("Sensor Woken Up!!\r\n");

  uint8_t raw_data[14];
  char line0[17];
  char line1[17];
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    I2C1_ReadRegs(0x68, 0x3B, raw_data, 14);

    int16_t accel_x = (raw_data[0] << 8) | raw_data[1];
    int16_t accel_y = (raw_data[2] << 8) | raw_data[3];
    int16_t accel_z = (raw_data[4] << 8) | raw_data[5];
    int16_t temp    = (raw_data[6] << 8) | raw_data[7];
    int16_t gyro_x  = (raw_data[8] << 8) | raw_data[9];
    int16_t gyro_y  = (raw_data[10] << 8) | raw_data[11];
    int16_t gyro_z  = (raw_data[12] << 8) | raw_data[13];
    (void)accel_z; (void)gyro_z; (void)temp;

    float ax_g = accel_x / 16384.0f;
    float ay_g = accel_y / 16384.0f;
    float gx_dps = gyro_x / 131.0f;
    float gy_dps = gyro_y / 131.0f;

    // Build fixed-point strings for the LCD (16 char width, HD44780 has no printf)
    int ax_sign = (ax_g < 0) ? 1 : 0;
    int ax_scaled = (int)((ax_sign ? -ax_g : ax_g) * 100.0f + 0.5f);
    int ay_sign = (ay_g < 0) ? 1 : 0;
    int ay_scaled = (int)((ay_sign ? -ay_g : ay_g) * 100.0f + 0.5f);

    int gx_sign = (gx_dps < 0) ? 1 : 0;
    int gx_scaled = (int)((gx_sign ? -gx_dps : gx_dps) * 100.0f + 0.5f);
    int gy_sign = (gy_dps < 0) ? 1 : 0;
    int gy_scaled = (int)((gy_sign ? -gy_dps : gy_dps) * 100.0f + 0.5f);

    snprintf(line0, sizeof(line0), "AX%s%d.%02d AY%s%d.%02d",
             ax_sign ? "-" : "", ax_scaled / 100, ax_scaled % 100,
             ay_sign ? "-" : "", ay_scaled / 100, ay_scaled % 100);

    snprintf(line1, sizeof(line1), "GX%s%d GY%s%d",
             gx_sign ? "-" : "", gx_scaled / 100,
             gy_sign ? "-" : "", gy_scaled / 100);

    LCD_PrintLine(0, line0);
    LCD_PrintLine(1, line1);

    // Also mirror to UART for logging/debugging alongside the LCD
    print_fixed2("Accel X", ax_g); printf(" ");
    print_fixed2("Accel Y", ay_g); printf("\r\n");

    HAL_Delay(300); // a few updates per second
    /* USER CODE END 3 */
  }
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
  * in the RCC_OscInitTypeDef structure. Board HSE confirmed at 8MHz.
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

/* -------------------- I2C1 (register-level) -------------------- */

void I2C1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER   |= (2 << 12) | (2 << 14);   // PB6/PB7 -> AF mode
    GPIOB->AFR[0]  |= (4 << 24) | (4 << 28);   // AF4 = I2C1
    GPIOB->OTYPER  |= (1 << 6) | (1 << 7);     // open-drain
    GPIOB->OSPEEDR |= (3 << 12) | (3 << 14);   // very high speed
    GPIOB->PUPDR   |= (1 << 12) | (1 << 14);   // pull-up

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    I2C1->CR2 &= ~(0x3F);
    I2C1->CR2 |= 42;              // APB1 = 42MHz

    I2C1->CCR &= ~(0xFFF);
    I2C1->CCR |= 210;             // 100kHz standard mode

    I2C1->TRISE &= ~(0x3F);
    I2C1->TRISE |= 43;

    I2C1->CR1 |= I2C_CR1_PE;
}

// For devices WITH internal registers (e.g. MPU6050): addr, reg, data
void I2C1_WriteReg(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t data)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void)temp;

    I2C1->DR = reg_Addr;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
}

// For register-less devices (e.g. PCF8574 GPIO expander): addr, single data byte
void I2C1_WriteByte(uint8_t dev_Addr, uint8_t data)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void)temp;

    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));

    I2C1->CR1 |= I2C_CR1_STOP;
}

uint8_t I2C1_ReadReg(uint8_t dev_Addr, uint8_t reg_Addr)
{
    uint8_t data = 0;

    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void)temp;

    I2C1->DR = reg_Addr;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 1;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));

    I2C1->CR1 &= ~I2C_CR1_ACK;          // clear ACK BEFORE clearing ADDR
    temp = I2C1->SR1; temp = I2C1->SR2; // clears ADDR, releases SCL
    I2C1->CR1 |= I2C_CR1_STOP;          // STOP right after clearing ADDR

    while (!(I2C1->SR1 & I2C_SR1_RXNE));
    data = I2C1->DR;

    return data;
}

void I2C1_ReadRegs(uint8_t dev_Addr, uint8_t reg_Addr, uint8_t *buffer, uint8_t len)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1;
    temp = I2C1->SR2; (void)temp;

    I2C1->DR = reg_Addr;
    while (!(I2C1->SR1 & I2C_SR1_TXE));

    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));

    I2C1->DR = dev_Addr << 1 | 1;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));

    I2C1->CR1 |= I2C_CR1_ACK;
    temp = I2C1->SR1; temp = I2C1->SR2; (void)temp;

    while (len > 0)
    {
        if (len == 1)
        {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }
        while (!(I2C1->SR1 & I2C_SR1_RXNE));
        *buffer = I2C1->DR;
        buffer++;
        len--;
    }
}

/* -------------------- LCD (PCF8574 + HD44780, 4-bit) -------------------- */

void LCD_PulseEnable(uint8_t byte)
{
    I2C1_WriteByte(0x27, byte | 0x04);   // E = 1 (bit 2)
    HAL_Delay(1);
    I2C1_WriteByte(0x27, byte & ~0x04);  // E = 0 -> falling edge latches data
    HAL_Delay(1);
}

void LCD_SendNibble(uint8_t nibble, uint8_t rs)
{
    uint8_t data = (nibble << 4) | 0x08; // nibble -> D4-D7, bit3 = backlight always on
    if (rs)
    {
        data |= 0x01; // RS = 1 for data, 0 for command
    }
    LCD_PulseEnable(data);
}

void LCD_SendByte(uint8_t byte, uint8_t rs)
{
    LCD_SendNibble((byte >> 4) & 0x0F, rs); // high nibble first, per HD44780 spec
    LCD_SendNibble(byte & 0x0F, rs);        // low nibble second
}

void LCD_Init(void)
{
    HAL_Delay(15); // power-on settle

    LCD_SendNibble(0x03, 0);
    HAL_Delay(5);

    LCD_SendNibble(0x03, 0);
    for (volatile int d = 0; d < 3000; d++);

    LCD_SendNibble(0x03, 0);
    for (volatile int d = 0; d < 3000; d++);

    LCD_SendNibble(0x02, 0); // commit to 4-bit mode
    for (volatile int d = 0; d < 3000; d++);

    LCD_SendByte(0x28, 0); // function set: 4-bit, 2-line, 5x8
    for (volatile int d = 0; d < 1000; d++);

    LCD_SendByte(0x0C, 0); // display on, cursor off, blink off
    for (volatile int d = 0; d < 1000; d++);

    LCD_SendByte(0x06, 0); // entry mode: increment, no shift
    for (volatile int d = 0; d < 1000; d++);

    LCD_SendByte(0x01, 0); // clear display — needs the longer delay
    HAL_Delay(2);
}

void LCD_Print(char *str)
{
    while (*str)
    {
        LCD_SendByte((uint8_t)*str, 1);
        str++;
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t row_offsets[] = {0x00, 0x40};
    uint8_t address = row_offsets[row] + col;
    LCD_SendByte(0x80 | address, 0); // RS=0 — this is a command, not data
}

// Prints a string padded/truncated to 16 chars so no stale characters
// remain on screen from a previous, longer value at the same position.
void LCD_PrintLine(uint8_t row, const char *str)
{
    char buf[17];
    int i = 0;
    for (; i < 16 && str[i] != '\0'; i++)
    {
        buf[i] = str[i];
    }
    for (; i < 16; i++)
    {
        buf[i] = ' ';
    }
    buf[16] = '\0';

    LCD_SetCursor(row, 0);
    LCD_Print(buf);
}

/* -------------------- UART2 (retarget printf) -------------------- */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER  |= (2 << 4) | (2 << 6);
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);

    USART2->BRR = 365;
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
}

void uart_write_byte(char c)
{
    while (!(USART2->SR & USART_SR_TXE));
    USART2->DR = c;
}

int _write(int file, char *ptr, int len)
{
    for (int i = 0; i < len; i++)
    {
        uart_write_byte(ptr[i]);
    }
    return len;
}

void print_fixed2(const char *label, float val)
{
    int sign = (val < 0) ? 1 : 0;
    float aval = sign ? -val : val;
    int scaled = (int)(aval * 100.0f + 0.5f); // round, not truncate
    printf("%s=%s%d.%02d", label, sign ? "-" : "", scaled / 100, scaled % 100);
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
