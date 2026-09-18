#include "main.h"

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

// Bit-Bang Prototypes
uint8_t SPIBitBang_TransferByte(uint8_t byte);
void SS_LOW(void); void SS_HIGH(void);
void MOSI_LOW(void); void MOSI_HIGH(void);
void SCK_LOW(void); void SCK_HIGH(void);
uint32_t MISO_READ(void);

// Hardware SPI Prototypes
void HW_SPI_Init(void);
uint8_t SPIHW_TransferByte(uint8_t byte);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    // 1. Bit-Bang GPIO Setup (PA0=SCK, PA1=MOSI, PA2=MISO, PA3=SS)
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    GPIOA->MODER |= (1<<0) | (1<<2) | (1<<6);      // Set PA0, PA1, PA3 as Output
    GPIOA->MODER &= ~(1<<1) & ~(1<<3) & ~(1<<7);
    GPIOA->MODER &= ~(3<<4);                       // Set PA2 as Input
    GPIOA->ODR |= (1<<3);                          // Initialize SS High (Idle)

    // 2. Hardware SPI1 Setup
    HW_SPI_Init();

    while (1)
    {
        // Test Bit-Bang Loopback
        uint8_t r1 = SPIBitBang_TransferByte(0xA5);
        uint8_t r2 = SPIBitBang_TransferByte(0x00);
        uint8_t r3 = SPIBitBang_TransferByte(0xFF);
        HAL_Delay(500);

        // Test Hardware SPI Loopback
        uint8_t hr1 = SPIHW_TransferByte(0xA5);
        uint8_t hr2 = SPIHW_TransferByte(0x00);
        uint8_t hr3 = SPIHW_TransferByte(0xFF);
        HAL_Delay(500);
    }
}

// --- BIT-BANG IMPLEMENTATION ---
uint8_t SPIBitBang_TransferByte(uint8_t byte) {
    uint8_t received = 0;
    SS_LOW();
    for (int i=7; i>=0; i--) {
        if (byte & (1<<i)) MOSI_HIGH(); else MOSI_LOW();
        SCK_HIGH();
        if (MISO_READ()) received |= (1<<i);
        SCK_LOW();
    }
    SS_HIGH();
    return received;
}

void SS_LOW(void)   { GPIOA->ODR &= ~(1<<3); }
void SS_HIGH(void)  { GPIOA->ODR |= (1<<3); }
void MOSI_HIGH(void){ GPIOA->ODR |= (1<<1); for(volatile int d=0; d<50; d++); }
void MOSI_LOW(void) { GPIOA->ODR &= ~(1<<1); for(volatile int d=0; d<50; d++); }
void SCK_LOW(void)  { GPIOA->ODR &= ~(1<<0); for(volatile int d=0; d<50; d++); }
void SCK_HIGH(void) { GPIOA->ODR |= (1<<0); for(volatile int d=0; d<50; d++); }
uint32_t MISO_READ(void) { return (GPIOA->IDR & (1<<2)); }

// --- HARDWARE SPI IMPLEMENTATION ---
void HW_SPI_Init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;
    
    // Configure PA5(SCK), PA6(MISO), PA7(MOSI) as AF5 Push-Pull
    GPIOA->MODER |= (2<<10) | (2<<12) | (2<<14);
    GPIOA->AFR[0] |= (5<<20) | (5<<24) | (5<<28);
    GPIOA->OTYPER &= ~((1<<5) | (1<<6) | (1<<7));
    GPIOA->OSPEEDR |= (3<<10) | (3<<12) | (3<<14);

    // Configure SPI1 Peripheral
    SPI1->CR1 = 0;
    SPI1->CR1 |= SPI_CR1_MSTR;               // Master Mode
    SPI1->CR1 |= (3<<3);                     // Baud Rate div 16
    SPI1->CR1 |= SPI_CR1_SSM | SPI_CR1_SSI;  // Software Slave Management
    SPI1->CR1 |= SPI_CR1_SPE;                // Enable SPI
}

uint8_t SPIHW_TransferByte(uint8_t byte) {
    while (!(SPI1->SR & SPI_SR_TXE));
    SPI1->DR = byte;
    while(!(SPI1->SR & SPI_SR_RXNE));
    return SPI1->DR;
}

// --- STANDARD BOILERPLATE ---
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