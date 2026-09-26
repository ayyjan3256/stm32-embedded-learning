#include "main.h"
#include <stdio.h>

/* ---------------------------------------------------------------------- */
/* Globals                                                                */
/* ---------------------------------------------------------------------- */

uint16_t buffer[96];               /* ADC1 multi-channel scan destination */
volatile uint8_t half_ready = 0;
volatile uint8_t full_ready = 0;

char csv_buf[600];                 /* shared CSV staging buffer, file scope */
volatile uint8_t tx_done = 1;      /* starts "done" so the first send isn't blocked */

/* ---------------------------------------------------------------------- */
/* Prototypes                                                             */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void USART2_Init(void);
void USART2_DMA1_Init(void);
void DMA2_ADC1_Init(void);
void uart_dma_send(int start_index, int count);
void Error_Handler(void);

/* ---------------------------------------------------------------------- */
/* main                                                                   */
/* ---------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    USART2_Init();
    USART2_DMA1_Init();
    DMA2_ADC1_Init();

    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (1)
    {
        if (half_ready)
        {
            half_ready = 0;
            uart_dma_send(0, 48);    /* buffer[0..47] guaranteed stable */
        }

        if (full_ready)
        {
            full_ready = 0;
            uart_dma_send(48, 96);   /* buffer[48..95] guaranteed stable */
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Format one completed half into csv_buf and hand it to USART2+DMA1      */
/* ---------------------------------------------------------------------- */

void uart_dma_send(int start_index, int end_index)
{
    /* sprintf only runs once the previous transfer has actually finished —
     * otherwise a new batch could overwrite csv_buf while DMA is still
     * reading the old one out of it, corrupting the transmission. */
    if (!tx_done)
    {
        return;   /* previous transfer still in flight — this batch is dropped */
    }

    tx_done = 0;

    int pos = 0;
    for (int i = start_index; i < end_index; i++)
    {
        pos += sprintf(&csv_buf[pos], "%u,", buffer[i]);
    }
    csv_buf[pos - 1] = '\n';

    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    while (DMA1_Stream6->CR & DMA_SxCR_EN);   /* wait for hardware to actually clear EN */

    DMA1_Stream6->M0AR = (uint32_t)csv_buf;
    DMA1_Stream6->NDTR = pos;
    DMA1_Stream6->CR |= DMA_SxCR_EN;
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
    DMA2_Stream0->CR |= DMA_SxCR_HTIE;
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;

    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
    DMA2_Stream0->NDTR = 96;
    DMA2_Stream0->M0AR = (uint32_t)buffer;

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

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
/* USART2 — polling init, DMA-driven TX                                   */
/* ---------------------------------------------------------------------- */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER |= (2 << 4) | (2 << 6);   /* PA2/PA3 -> alternate function */
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12); /* AF7 = USART2                  */

    USART2->BRR = 365;
    USART2->CR3 |= USART_CR3_DMAT;   /* enable DMA transmit requests */
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
}

/* ---------------------------------------------------------------------- */
/* USART2_TX + DMA1 Stream6 Channel4                                      */
/* ---------------------------------------------------------------------- */

void USART2_DMA1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    DMA1_Stream6->PAR  = (uint32_t)&(USART2->DR);
    DMA1_Stream6->M0AR = (uint32_t)csv_buf;
    DMA1_Stream6->NDTR = 0;   /* set per-transfer in uart_dma_send() */

    DMA1_Stream6->CR &= ~(3 << 6);
    DMA1_Stream6->CR |=  (1 << 6);   /* DIR = memory-to-peripheral */
    DMA1_Stream6->CR |=  (1 << 10);  /* MINC = 1 */
    DMA1_Stream6->CR &= ~(1 << 9);   /* PINC = 0 */
    DMA1_Stream6->CR &= ~((3 << 11) | (3 << 13)); /* PSIZE = MSIZE = byte */
    DMA1_Stream6->CR |=  (4 << 25);  /* CHSEL = 4 (USART2_TX) */
    DMA1_Stream6->CR |= DMA_SxCR_TCIE;

    NVIC_EnableIRQ(DMA1_Stream6_IRQn);
}

void DMA1_Stream6_IRQHandler(void)
{
    if (DMA1->HISR & DMA_HISR_TCIF6)
    {
        DMA1->HIFCR |= DMA_HIFCR_CTCIF6;
        tx_done = 1;
    }
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
