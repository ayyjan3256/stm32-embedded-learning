#include "main.h"
#include <stdio.h>

/* ---------------------------------------------------------------------- */
/* Globals                                                                */
/* ---------------------------------------------------------------------- */

/* NDTR and buffer length are both 96: divisible by 3 (clean scan boundary
 * per full buffer) AND by 2 (clean half-transfer split), so HTIF/TCIF both
 * land on complete channel-scan boundaries rather than mid-scan. */
uint16_t buffer[96];
volatile uint8_t half_ready = 0;
volatile uint8_t full_ready = 0;

/* ---------------------------------------------------------------------- */
/* Prototypes                                                             */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
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

    DMA2_ADC1_Init();
    USART2_Init();

    ADC1->CR2 |= ADC_CR2_SWSTART;   /* single trigger — CIRC + CONT + DDS run forever */

    while (1)
    {
        if (half_ready)
        {
            half_ready = 0;
            /* buffer[0..47] guaranteed stable — DMA is writing 48..95 */
            for (int i = 0; i < 48; i++)
            {
                printf("Value: %u\r\n", buffer[i]);
            }
        }

        if (full_ready)
        {
            full_ready = 0;
            /* buffer[48..95] guaranteed stable — DMA has wrapped to 0..47 */
            for (int i = 48; i < 96; i++)
            {
                printf("Value: %u\r\n", buffer[i]);
            }
        }
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
/* ADC1 multi-channel scan (ch0, ch1, ch4) + DMA2 Stream0, circular mode  */
/* ---------------------------------------------------------------------- */

void DMA2_ADC1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    GPIOA->MODER |= (3 << 0);            /* PA0 -> analog (channel 0) */
    GPIOA->MODER |= (3 << 2) | (3 << 8); /* PA1, PA4 -> analog (channels 1, 4) */

    ADC1->CR2   |= ADC_CR2_CONT;
    ADC1->CR2   |= ADC_CR2_DMA;
    ADC1->CR2   |= ADC_CR2_DDS;
    ADC1->SMPR2 |= (7 << 0);   /* slowest sample time */

    ADC1->CR1   |= (1 << 8);        /* SCAN = 1 */
    ADC1->SQR1  |= (2 << 20);       /* L = 2  -> 3 conversions in sequence */
    ADC1->SQR3   = 0;
    ADC1->SQR3  |= (1 << 5) | (4 << 10); /* sequence: ch0, ch1, ch4 */

    ADC1->CR2   |= ADC_CR2_ADON;

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    DMA2_Stream0->CR &= ~(3 << 6);   /* DIR = peripheral-to-memory */
    DMA2_Stream0->CR &= ~(1 << 9);   /* PINC = 0 */
    DMA2_Stream0->CR |=  (1 << 10);  /* MINC = 1 */
    DMA2_Stream0->CR |=  (1 << 11) | (1 << 13); /* PSIZE = MSIZE = halfword */
    DMA2_Stream0->CR |= DMA_SxCR_HTIE;  /* half-transfer interrupt */
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;  /* transfer-complete interrupt */
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;  /* circular mode */

    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
    DMA2_Stream0->NDTR = 96;
    DMA2_Stream0->M0AR = (uint32_t)buffer;

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

/* ---------------------------------------------------------------------- */
/* DMA2 Stream0 ISR — half/full transfer flags, minimal work only         */
/* ---------------------------------------------------------------------- */

void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_HTIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CHTIF0;
        half_ready = 1;
    }
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;
        full_ready = 1;
    }
}

/* ---------------------------------------------------------------------- */
/* USART2 — polling TX, retargeted for printf via _write()                */
/* ---------------------------------------------------------------------- */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER |= (2 << 4) | (2 << 6);
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
