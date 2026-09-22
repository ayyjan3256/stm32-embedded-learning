#include "main.h"
#include <stdio.h>

/* ---------------------------------------------------------------------- */
/* Globals                                                                */
/* ---------------------------------------------------------------------- */

/* Destination for the ADC1+DMA2 continuous-mode pipeline.
 * File scope (not local to a function) so its address stays valid for the
 * life of the program, and so both DMA2_ADC1_Init() and main() can see it. */
uint16_t buffer[100];

/* ---------------------------------------------------------------------- */
/* Prototypes                                                             */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void DMA2_M2M_Test(void);
void DMA2_ADC1_Init(void);
void USART2_Init(void);
void uart_write_byte(char c);
int  _write(int file, char *ptr, int len);
void Error_Handler(void);

/* ---------------------------------------------------------------------- */
/* main                                                                   */
/* ---------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /* Hours 1-2: proves the DMA engine itself moves data correctly,
     * independent of any peripheral, before wiring it to the ADC.
     * Uncomment to re-run the memory-to-memory sanity check. */
    // DMA2_M2M_Test();

    /* Hours 3-5: continuous-mode ADC1 sampling into buffer[], driven
     * entirely by DMA2 Stream0 with zero per-sample CPU involvement. */
    DMA2_ADC1_Init();
    USART2_Init();

    ADC1->CR2 |= ADC_CR2_SWSTART;   /* single trigger — CONT + DDS keep it running */

    /* Wait for transfer-complete (TCIF0, bit 5) or transfer-error (TEIF0, bit 3)
     * on Stream0's status, both reported in DMA2->LISR (streams 0-3). */
    while (!(DMA2->LISR & (1 << 5)) && !(DMA2->LISR & (1 << 3)));

    for (int i = 0; i < 100; i++)
    {
        printf("Value: %u\r\n", buffer[i]);
    }

    while (1)
    {
    }
}

/* ---------------------------------------------------------------------- */
/* System clock — HSE 8MHz -> 168MHz SYSCLK via PLL                       */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM       = 4;
    RCC_OscInitStruct.PLL.PLLN       = 168;
    RCC_OscInitStruct.PLL.PLLP       = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ       = 4;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
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

/* ---------------------------------------------------------------------- */
/* DMA2 memory-to-memory sanity test (Day 2, Hours 1-2)                   */
/* ---------------------------------------------------------------------- */

void DMA2_M2M_Test(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    static uint32_t src[10]  = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    static uint32_t dest[10] = {0};

    DMA2_Stream0->CR |= (2 << 6);              /* DIR = memory-to-memory */
    DMA2_Stream0->CR |= (1 << 9) | (1 << 10);  /* PINC = 1, MINC = 1     */
    DMA2_Stream0->CR |= (2 << 11) | (2 << 13); /* PSIZE = MSIZE = word   */

    DMA2_Stream0->PAR  = (uint32_t)src;
    DMA2_Stream0->NDTR = 10;
    DMA2_Stream0->M0AR = (uint32_t)dest;

    DMA2_Stream0->CR |= DMA_SxCR_EN;

    while (!(DMA2->LISR & (1 << 5)) && !(DMA2->LISR & (1 << 3)));
}

/* ---------------------------------------------------------------------- */
/* ADC1 (PA0) + DMA2 Stream0 Channel0 — continuous, peripheral-to-memory  */
/* ---------------------------------------------------------------------- */

void DMA2_ADC1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    GPIOA->MODER |= (3 << 0);   /* PA0 -> analog mode */

    ADC1->CR2   |= ADC_CR2_CONT;
    ADC1->CR2   |= ADC_CR2_DMA;
    ADC1->CR2   |= ADC_CR2_DDS;     /* keep issuing DMA requests every conversion */
    ADC1->SMPR2 |= (7 << 0);        /* slowest sample time, channel 0 */
    ADC1->SQR3   = 0;               /* first conversion in sequence = channel 0 */
    ADC1->CR2   |= ADC_CR2_ADON;

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    DMA2_Stream0->CR &= ~(3 << 6);   /* DIR = peripheral-to-memory */
    DMA2_Stream0->CR &= ~(1 << 9);   /* PINC = 0 (DR address is fixed) */
    DMA2_Stream0->CR |=  (1 << 10);  /* MINC = 1 (buffer must advance) */
    DMA2_Stream0->CR |=  (1 << 11) | (1 << 13); /* PSIZE = MSIZE = halfword */

    DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
    DMA2_Stream0->NDTR = 100;
    DMA2_Stream0->M0AR = (uint32_t)buffer;

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

/* ---------------------------------------------------------------------- */
/* USART2 — polling TX, retargeted for printf via _write()                */
/* ---------------------------------------------------------------------- */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER |= (2 << 4) | (2 << 6);   /* PA2/PA3 -> alternate function */
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12); /* AF7 = USART2                  */

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

/* ---------------------------------------------------------------------- */
/* Error handler                                                          */
/* ---------------------------------------------------------------------- */

void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}
