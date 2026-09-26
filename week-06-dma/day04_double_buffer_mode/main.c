#include "main.h"
#include <stdio.h>
#include <math.h>

/* ---------------------------------------------------------------------- */
/* Globals                                                                */
/* ---------------------------------------------------------------------- */

/* NDTR = 99 = 3 channels x 33 scans. No half-transfer split exists in
 * double buffer mode (TCIF fires once per FULL buffer), so the only
 * constraint here is "divisible by channel count" — the /2 constraint
 * from Day 3 does not apply. */
uint16_t buffer_a[99];
uint16_t buffer_b[99];
volatile uint8_t buffer_a_ready = 0;
volatile uint8_t buffer_b_ready = 0;

/* ---------------------------------------------------------------------- */
/* Prototypes                                                             */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void DMA2_ADC1_Init(void);
void USART2_Init(void);
void uart_write_byte(char c);
int  _write(int file, char *ptr, int len);
void process_buffer(uint16_t *buf, int len);
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

    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (1)
    {
        if (buffer_a_ready)
        {
            buffer_a_ready = 0;
            /* buffer_a fully stable — DMA is writing buffer_b right now */
            process_buffer(buffer_a, 99);
        }

        if (buffer_b_ready)
        {
            buffer_b_ready = 0;
            /* buffer_b fully stable — DMA is writing buffer_a right now */
            process_buffer(buffer_b, 99);
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
/* ADC1 multi-channel scan + DMA2 Stream0, double buffer mode             */
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
    ADC1->SMPR2 |= (7 << 0);

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
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;  /* transfer-complete only — no HTIE */
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;  /* DBM requires CIRC underneath    */
    DMA2_Stream0->CR |= DMA_SxCR_DBM;   /* double buffer mode              */

    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
    DMA2_Stream0->NDTR = 99;
    DMA2_Stream0->M0AR = (uint32_t)buffer_a;
    DMA2_Stream0->M1AR = (uint32_t)buffer_b;

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

/* ---------------------------------------------------------------------- */
/* DMA2 Stream0 ISR — CT tells us which buffer DMA just switched TO,      */
/* so the buffer that just finished is the OTHER one.                     */
/* ---------------------------------------------------------------------- */

void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;

        if (DMA2_Stream0->CR & DMA_SxCR_CT)
        {
            /* CT==1: DMA just switched to M1AR (buffer_b) -> buffer_a just finished */
            buffer_a_ready = 1;
        }
        else
        {
            /* CT==0: DMA just switched to M0AR (buffer_a) -> buffer_b just finished */
            buffer_b_ready = 1;
        }
    }
}

/* ---------------------------------------------------------------------- */
/* Per-buffer processing — running mean/std, Week 3 anomaly-signal style  */
/* ---------------------------------------------------------------------- */

void process_buffer(uint16_t *buf, int len)
{
    uint32_t sum = 0;
    for (int i = 0; i < len; i++)
    {
        sum += buf[i];
    }
    float mean = sum / (float)len;

    float sq_diff_sum = 0.0f;
    for (int i = 0; i < len; i++)
    {
        float diff = buf[i] - mean;
        sq_diff_sum += diff * diff;
    }
    float std = sqrtf(sq_diff_sum / len);

    /* Deliberate Day 4 Hour 6 stress test: this delay outlasts DMA's fill
     * time for the other buffer, to observe stale/dropped-buffer behavior
     * when processing falls behind the incoming sample rate. Remove for
     * normal operation. */
    HAL_Delay(2000);

    printf("Mean: %d.%02d  Std: %d.%02d\r\n",
           (int)mean, (int)((mean - (int)mean) * 100),
           (int)std, (int)((std - (int)std) * 100));
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
