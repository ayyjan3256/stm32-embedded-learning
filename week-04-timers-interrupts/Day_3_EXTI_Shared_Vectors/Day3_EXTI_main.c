#include "main.h"

/* Private variables ---------------------------------------------------------*/
volatile uint32_t last_trigger_pa5 = 0;
volatile uint32_t last_trigger_pa7 = 0;
const uint32_t DEBOUNCE_DELAY = 200; // 200ms debounce

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void EXTI_Interrupt_Init(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  
  // Initialize GPIO and Interrupts
  EXTI_Interrupt_Init();

  while (1)
  {
      // The main loop is completely free! 
      // The CPU can do other tasks here while waiting for interrupts.
  }
}

void EXTI_Interrupt_Init(void)
{
    // 1. Enable Clocks for GPIOA (Inputs), GPIOB (LED), and SYSCFG
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN;
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    // 2. Configure PB2 as Output (LED)
    GPIOB->MODER |= (1 << 4);

    // 3. Configure PA5 and PA7 as Inputs (MODER = 00 by default)
    // Optional: Add Pull-up resistors if your buttons connect to ground
    GPIOA->PUPDR |= (1 << 10); // PA5 Pull-up
    GPIOA->PUPDR |= (1 << 14); // PA7 Pull-up

    // 4. SYSCFG: Connect EXTI Line 5 to PA5 and Line 7 to PA7
    SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI5_PA; // EXTI5 -> PA5
    SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI7_PA; // EXTI7 -> PA7

    // 5. EXTI: Enable Falling Edge Triggers (Button press pulls to GND)
    EXTI->FTSR |= (1 << 5); 
    EXTI->FTSR |= (1 << 7); 

    // 6. EXTI: Unmask (Enable) Interrupt Lines 5 and 7
    EXTI->IMR |= (1 << 5); 
    EXTI->IMR |= (1 << 7); 

    // 7. NVIC: Enable the EXTI9_5 vector and set priority
    NVIC_EnableIRQ(EXTI9_5_IRQn);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);
}

// 8. The Shared Interrupt Handler
void EXTI9_5_IRQHandler(void)
{
    uint32_t current_time = HAL_GetTick();

    // Check PA5 Flag
    if (EXTI->PR & (1 << 5)) {
        if ((current_time - last_trigger_pa5) > DEBOUNCE_DELAY) {
            GPIOB->ODR ^= (1 << 2); // Toggle LED
            last_trigger_pa5 = current_time;
        }
        EXTI->PR = (1 << 5); // Clear PA5 flag
    }
    
    // Check PA7 Flag
    if (EXTI->PR & (1 << 7)) {
        if ((current_time - last_trigger_pa7) > DEBOUNCE_DELAY) {
            // Do something else for PA7
            last_trigger_pa7 = current_time;
        }
        EXTI->PR = (1 << 7); // Clear PA7 flag
    }
}

void SystemClock_Config(void)
{
    // Implementation omitted for brevity (Use your standard 168MHz HSE config here)
}
