#include "main.h"
#include <stdio.h>

/* ---------------------------------------------------------------------- */
/* Globals                                                                */
/* ---------------------------------------------------------------------- */

uint16_t buffer_a[99];
uint16_t buffer_b[99];
volatile uint8_t buffer_a_ready = 0;
volatile uint8_t buffer_b_ready = 0;

char csv_buf[600];
volatile uint8_t tx_done = 1;
uint8_t message[] = "Hello this is UART-DMA!";

/* ms_ticks is incremented in SysTick_Handler (stm32f4xx_it.c), alongside
 * HAL_IncTick() — declare it there as: extern volatile uint32_t ms_ticks; */
volatile uint32_t ms_ticks = 0;
volatile uint8_t adc_running = 1;
volatile uint8_t last_adc_running = 1;

/* ---------------------------------------------------------------------- */
/* Prototypes                                                             */
/* ---------------------------------------------------------------------- */

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
void LED_Init(void);
void DMA2_ADC1_Init(void);
void USART2_Init(void);
void USART2_DMA1_Init(void);
void EXTI0_Init(void);
void SysTick_Init(void);
void Error_Handler(void);

/* ---------------------------------------------------------------------- */
/* main                                                                   */
/* ---------------------------------------------------------------------- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    LED_Init();
    DMA2_ADC1_Init();
    USART2_Init();
    USART2_DMA1_Init();   /* sends the startup message */
    EXTI0_Init();
    SysTick_Init();

    /* One-time boot delay: lets the startup message finish clocking out
     * before the ADC pipeline starts reusing DMA1 Stream6 for CSV data. */
    HAL_Delay(2000);

    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (1)
    {
        /* Button-driven start/stop of the ADC stream */
        if (adc_running != last_adc_running)
        {
            last_adc_running = adc_running;

            if (!adc_running)
            {
                DMA2_Stream0->CR &= ~DMA_SxCR_EN;
                while (DMA2_Stream0->CR & DMA_SxCR_EN);   /* EN clears only after in-flight transfer ends */
            }
            else
            {
                DMA2_Stream0->NDTR = 99;
                DMA2_Stream0->CR |= DMA_SxCR_EN;
                ADC1->CR2 |= ADC_CR2_SWSTART;
            }
        }

        if (adc_running)
        {
            if (buffer_a_ready)
            {
                buffer_a_ready = 0;

                int pos = 0;
                for (int i = 0; i < 99; i++)
                {
                    pos += sprintf(&csv_buf[pos], "%u,", buffer_a[i]);
                }
                csv_buf[pos - 1] = '\n';

                if (tx_done)
                {
                    tx_done = 0;
                    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
                    while (DMA1_Stream6->CR & DMA_SxCR_EN);

                    DMA1_Stream6->M0AR = (uint32_t)csv_buf;
                    DMA1_Stream6->NDTR = pos;
                    DMA1_Stream6->CR |= DMA_SxCR_EN;
                }
            }

            if (buffer_b_ready)
            {
                buffer_b_ready = 0;

                int pos = 0;
                for (int i = 0; i < 99; i++)
                {
                    pos += sprintf(&csv_buf[pos], "%u,", buffer_b[i]);
                }
                csv_buf[pos - 1] = '\n';

                if (tx_done)
                {
                    tx_done = 0;
                    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
                    while (DMA1_Stream6->CR & DMA_SxCR_EN);

                    DMA1_Stream6->M0AR = (uint32_t)csv_buf;
                    DMA1_Stream6->NDTR = pos;
                    DMA1_Stream6->CR |= DMA_SxCR_EN;
                }
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
/* Error LED — PB2, driven directly from DMA error ISRs                   */
/* (deliberately independent of UART/DMA so it can report their failure)  */
/* ---------------------------------------------------------------------- */

void LED_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    GPIOB->MODER &= ~(3 << 4);
    GPIOB->MODER |=  (1 << 4);   /* PB2 -> output */
}

/* ---------------------------------------------------------------------- */
/* ADC1 multi-channel scan (ch0, ch1, ch4) + DMA2 Stream0, double buffer  */
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

    ADC1->CR1   |= (1 << 8);              /* SCAN */
    ADC1->SQR1  |= (2 << 20);             /* 3 conversions */
    ADC1->SQR3   = 0;
    ADC1->SQR3  |= (1 << 5) | (4 << 10);  /* ch0, ch1, ch4 */

    ADC1->CR2   |= ADC_CR2_ADON;

    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

    DMA2_Stream0->CR &= ~(3 << 6);   /* DIR = peripheral-to-memory */
    DMA2_Stream0->CR &= ~(1 << 9);   /* PINC = 0 */
    DMA2_Stream0->CR |=  (1 << 10);  /* MINC = 1 */
    DMA2_Stream0->CR |=  (1 << 11) | (1 << 13); /* halfword */
    DMA2_Stream0->CR |= DMA_SxCR_TCIE;
    DMA2_Stream0->CR |= DMA_SxCR_TEIE;   /* transfer-error interrupt */
    DMA2_Stream0->CR |= DMA_SxCR_CIRC;   /* DBM requires CIRC */
    DMA2_Stream0->CR |= DMA_SxCR_DBM;

    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->PAR  = (uint32_t)&(ADC1->DR);
    DMA2_Stream0->NDTR = 99;
    DMA2_Stream0->M0AR = (uint32_t)buffer_a;
    DMA2_Stream0->M1AR = (uint32_t)buffer_b;

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

/* ---------------------------------------------------------------------- */
/* USART2 + DMA1 Stream6 Channel4 (TX)                                    */
/* ---------------------------------------------------------------------- */

void USART2_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    GPIOA->MODER |= (2 << 4) | (2 << 6);
    GPIOA->AFR[0] |= (7 << 8) | (7 << 12);

    USART2->BRR = 365;
    USART2->CR1 |= USART_CR1_UE | USART_CR1_RE | USART_CR1_TE;
    USART2->CR3 |= USART_CR3_DMAT;
}

void USART2_DMA1_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;

    DMA1_Stream6->NDTR = sizeof(message) - 1;
    DMA1_Stream6->PAR  = (uint32_t)&(USART2->DR);
    DMA1_Stream6->M0AR = (uint32_t)message;

    DMA1_Stream6->CR &= ~(3 << 6);
    DMA1_Stream6->CR |=  (1 << 6);   /* DIR = memory-to-peripheral */
    DMA1_Stream6->CR |=  (1 << 10);  /* MINC */
    DMA1_Stream6->CR &= ~(1 << 9);   /* PINC = 0 */
    DMA1_Stream6->CR &= ~((3 << 11) | (3 << 13)); /* byte */
    DMA1_Stream6->CR |=  (4 << 25);  /* CHSEL = 4 (USART2_TX) */
    DMA1_Stream6->CR |= DMA_SxCR_TCIE;
    DMA1_Stream6->CR |= DMA_SxCR_TEIE;

    NVIC_EnableIRQ(DMA1_Stream6_IRQn);

    tx_done = 0;   /* startup message is in flight */
    DMA1_Stream6->CR |= DMA_SxCR_EN;
}

/* ---------------------------------------------------------------------- */
/* EXTI0 button (PB0, pull-up, falling edge)                              */
/* ---------------------------------------------------------------------- */

void EXTI0_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    GPIOB->MODER &= ~(3 << 0);   /* PB0 -> input */
    GPIOB->PUPDR &= ~(3 << 0);
    GPIOB->PUPDR |=  (1 << 0);   /* pull-up */

    SYSCFG->EXTICR[0] &= ~(0xF << 0);
    SYSCFG->EXTICR[0] |=  (0x1 << 0);   /* port B -> EXTI0 */

    EXTI->FTSR |= (1 << 0);
    EXTI->IMR  |= (1 << 0);

    NVIC_SetPriority(EXTI0_IRQn, 0);
    NVIC_EnableIRQ(EXTI0_IRQn);
}

void SysTick_Init(void)
{
    SysTick->LOAD = 167999;   /* 1ms at 168MHz */
    SysTick->VAL  = 0;
    SysTick->CTRL |= (1 << 2) | (1 << 1) | (1 << 0);
}

/* ---------------------------------------------------------------------- */
/* Interrupt handlers                                                     */
/* ---------------------------------------------------------------------- */

void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CTCIF0;

        /* CT reports where DMA is headed, so the finished buffer is the other one */
        if (DMA2_Stream0->CR & DMA_SxCR_CT)
        {
            buffer_a_ready = 1;
        }
        else
        {
            buffer_b_ready = 1;
        }
    }
    if (DMA2->LISR & DMA_LISR_TEIF0)
    {
        DMA2->LIFCR |= DMA_LIFCR_CTEIF0;
        GPIOB->ODR ^= (1 << 2);
    }
}

void DMA1_Stream6_IRQHandler(void)
{
    if (DMA1->HISR & DMA_HISR_TCIF6)
    {
        DMA1->HIFCR |= DMA_HIFCR_CTCIF6;
        tx_done = 1;
    }
    if (DMA1->HISR & DMA_HISR_TEIF6)
    {
        DMA1->HIFCR |= DMA_HIFCR_CTEIF6;
        GPIOB->ODR ^= (1 << 2);
    }
}

void EXTI0_IRQHandler(void)
{
    if (EXTI->PR & (1 << 0))
    {
        EXTI->PR = (1 << 0);   /* write-1-to-clear */

        static uint32_t last_press_time = 0;
        if ((ms_ticks - last_press_time) > 50)
        {
            adc_running = !adc_running;
            last_press_time = ms_ticks;
        }
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
