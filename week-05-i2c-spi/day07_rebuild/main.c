#include "main.h"
#include <stdio.h>

// Core Prototypes
void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// I2C & LCD Prototypes
void I2C1_Init(void);
void I2C1_WriteReg(uint8_t addr, uint8_t reg, uint8_t data);
uint8_t I2C1_ReadReg(uint8_t addr, uint8_t reg);
void I2C1_ReadRegs(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t len);
void I2C1_WriteByte(uint8_t addr, uint8_t data);
void LCDPulseEnable(uint8_t byte);
void LCD_SendNibble(uint8_t nibble, uint8_t rs);
void LCD_SendByte(uint8_t byte, uint8_t rs);
void LCD_Print(uint8_t *str);
void LCD_SetCursor(uint8_t row, uint8_t col);
void LCD_Init(void);
void print_fixed(const char *label, float val);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // 1. Initialize Peripherals
    I2C1_Init();
    LCD_Init();

    // 2. Hardware SPI1 Init (Demonstrating multi-peripheral coexistence)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    GPIOA->MODER |= (2<<10) | (2<<12) | (2<<14);
    GPIOA->AFR[0] |= (5<<20) |(5<<24) | (5<<28);
    GPIOA->OTYPER &= ~((1<<5)| (1<<6) | (1<<7));
    GPIOA->OSPEEDR |= (3<<10) | (3<<12) | (3<<14);
    SPI1->CR1 = SPI_CR1_MSTR | (3<<3) | SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE;

    // 3. Wake MPU6050
    I2C1_WriteReg(0x68, 0x6B, 0x00);
    
    uint8_t data_buffer[14];

    while (1)
    {
        // 4. Burst Read MPU6050
        I2C1_ReadRegs(0x68, 0x3B, data_buffer, 14);

        int16_t accel_x = (data_buffer[0] << 8) | data_buffer[1];
        int16_t accel_y = (data_buffer[2] << 8) | data_buffer[3];
        int16_t accel_z = (data_buffer[4] << 8) | data_buffer[5];
        
        float ax_g = accel_x / 16384.0f;
        float ay_g = accel_y / 16384.0f;
        float az_g = accel_z / 16384.0f;

        // Print physical values to UART
        print_fixed("Accel X", ax_g); printf(" ");
        print_fixed("Accel Y", ay_g); printf(" ");
        print_fixed("Accel Z", az_g); printf("\r\n");

        // 5. Update LCD
        LCD_SetCursor(0, 0);
        LCD_Print((uint8_t *)"MPU6050 Active");

        HAL_Delay(500);
    }
}

// --- I2C DRIVER IMPLEMENTATION ---
void I2C1_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER |= (2<<12) | (2<<14);
    GPIOB->AFR[0] |= (4<<24) | (4<<28);
    GPIOB->OTYPER |= (1<<6) | (1<<7);
    GPIOB->PUPDR |= (1<<12) | (1<<14);
    GPIOB->OSPEEDR |= (3<<12) | (3<<14);

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    I2C1->CR1 |= I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;
    I2C1->CR2 = 42;
    I2C1->CCR = 210;
    I2C1->TRISE = 43;
    I2C1->CR1 |= I2C_CR1_PE;
}

void I2C1_WriteByte(uint8_t addr, uint8_t data) {
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
    I2C1->DR = addr<<1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1; temp = I2C1->SR2; (void)temp;
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));
    I2C1->CR1 |= I2C_CR1_STOP;
}

void I2C1_WriteReg(uint8_t addr, uint8_t reg, uint8_t data) {
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
    I2C1->DR = addr<<1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1; temp = I2C1->SR2; (void)temp;
    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));
    I2C1->CR1 |= I2C_CR1_STOP;
}

void I2C1_ReadRegs(uint8_t addr, uint8_t reg, uint8_t *buffer, uint8_t len) {
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
    I2C1->DR = addr<<1 | 0;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    volatile uint32_t temp = I2C1->SR1; temp = I2C1->SR2; (void)temp;
    
    I2C1->DR = reg;
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    
    I2C1->CR1 |= I2C_CR1_START; // Repeated Start
    while (!(I2C1->SR1 & I2C_SR1_SB));
    I2C1->DR = addr<<1 | 1;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    I2C1->CR1 |= I2C_CR1_ACK;
    temp = I2C1->SR1; temp = I2C1->SR2; (void)temp;

    while (len > 0) {
        if (len == 1) {
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }
        while (!(I2C1->SR1 & I2C_SR1_RXNE));
        *buffer = I2C1->DR;
        buffer++;
        len--;
    }
}

// --- LCD DRIVER IMPLEMENTATION ---
void LCDPulseEnable(uint8_t byte) {
    I2C1_WriteByte(0x27, byte | 0x04);
    for(volatile int d=0; d<5000; d++); // Micro-delay
    I2C1_WriteByte(0x27, byte & ~0x04);
    for(volatile int d=0; d<5000; d++);
}

void LCD_SendNibble(uint8_t nibble, uint8_t rs) {
    uint8_t byte = (nibble<<4) | 0x08; // 0x08 keeps backlight on
    if (rs) byte |= 0x01;
    LCDPulseEnable(byte);
}

void LCD_SendByte(uint8_t byte, uint8_t rs) {
    LCD_SendNibble((byte>>4) & 0x0F, rs);
    LCD_SendNibble(byte & 0x0F, rs);
}

void LCD_Print(uint8_t *str) {
    while (*str) {
        LCD_SendByte((uint8_t)*str, 1);
        str++;
    }
}

void LCD_SetCursor(uint8_t row, uint8_t col) {
    uint8_t row_offsets[] = {0x00, 0x40};
    uint8_t address = row_offsets[row] + col;
    LCD_SendByte(0x80 | address, 0);
}

void LCD_Init(void) {
    HAL_Delay(15);
    LCD_SendNibble(0x03, 0); HAL_Delay(5);
    LCD_SendNibble(0x03, 0); for (volatile int d = 0; d < 3000; d++);
    LCD_SendNibble(0x03, 0); for (volatile int d = 0; d < 3000; d++);
    LCD_SendNibble(0x02, 0); for (volatile int d = 0; d < 3000; d++);

    LCD_SendByte(0x28, 0); for (volatile int d = 0; d < 1000; d++);
    LCD_SendByte(0x0C, 0); for (volatile int d = 0; d < 1000; d++);
    LCD_SendByte(0x06, 0); for (volatile int d = 0; d < 1000; d++);
    LCD_SendByte(0x01, 0); HAL_Delay(2);
}

// --- UTILITIES & BOILERPLATE ---
void print_fixed(const char *label, float val) {
    int sign = (val < 0) ? 1 : 0;
    float aval = sign ? -val : val;
    int scaled = (int)(aval * 100.0f + 0.5f);
    printf("%s=%s%d.%02d ", label, sign ? "-" : "", scaled / 100, scaled % 100);
}

void SystemClock_Config(void) {
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
void Error_Handler(void) { __disable_irq(); while (1) {} }