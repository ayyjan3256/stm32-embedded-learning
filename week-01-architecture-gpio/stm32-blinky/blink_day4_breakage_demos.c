/**
 * blink_day4_breakage_demos.c
 *
 * STM32F405 bare-metal LED blink — PC13 — with documented deliberate
 * failure experiments. The active code below is a working "3 fast
 * blinks then a long pause" pattern built entirely from raw register
 * writes (RCC->AHB1ENR, GPIOC->MODER, GPIOC->BSRR) plus HAL_Delay for
 * timing. Each commented-out EXPERIMENT block below shows a single
 * line changed from the working version, along with what was actually
 * observed on hardware when that change was made — a controlled-failure
 * exercise for understanding STM32 GPIO/clock behavior, not just reading
 * about it.
 *
 * Board: STM32F405 (RGT6 / LQFP64), LED wired to PC13.
 * Toolchain: STM32CubeIDE. HAL_Init()/SystemClock_Config() (HSI-based)
 * retained from the generated project for clock setup and HAL_Delay's
 * SysTick backing; GPIO itself is fully raw-register.
 */

#include "stm32f4xx.h"

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Enable GPIOC clock (bit 2 = GPIOCEN) — see blink_raw.c / README.md
     * for the full address derivation. */
    RCC->AHB1ENR |= (1 << 2);

    /* Configure PC13 as general-purpose output (MODER bits 27:26 = 01). */
    GPIOC->MODER &= ~(0b11 << 26);
    GPIOC->MODER |=  (0b01 << 26);

    /* Working pattern: 3 fast blinks, then a long pause, repeating.
     * Built entirely from BSRR — no ODR read-modify-write anywhere. */
    while (1)
    {
        for (int i = 0; i < 3; i++)
        {
            GPIOC->BSRR = (1 << 13);        /* PC13 high */
            HAL_Delay(100);
            GPIOC->BSRR = (1 << (13 + 16)); /* PC13 low */
            HAL_Delay(100);
        }

        HAL_Delay(1000);                    /* long pause */
    }
}

/* =====================================================================
 * DOCUMENTED FAILURE EXPERIMENTS
 * =====================================================================
 * Each block below is a single deliberate change from the working code
 * above. None of these are active — they're left here as a record of
 * what was tested and what it proved. To reproduce, apply exactly one
 * change at a time to the working code above, reflash, observe, revert.
 * ---------------------------------------------------------------------
 *
 * EXPERIMENT 1 — Remove the clock enable entirely.
 *
 *     // RCC->AHB1ENR |= (1 << 2);   <- commented out, GPIOC clock
 *                                        never enabled
 *
 *   OBSERVED: LED does not blink. No crash, no fault, no error of any
 *   kind — the LED simply sits in whatever state it was already in.
 *
 *   WHY: GPIOC's address is valid and mapped (writes complete as normal
 *   bus transactions, no HardFault), but the peripheral's internal
 *   logic is clock-gated off, so it never sees or reacts to the write.
 *   This is the classic "silent no-op" failure mode of memory-mapped
 *   I/O — distinct from writing to a genuinely invalid address, which
 *   *would* trigger a HardFault.
 *
 * ---------------------------------------------------------------------
 *
 * EXPERIMENT 2 — Wrong MODER encoding (Alternate Function instead of
 * Output).
 *
 *     GPIOC->MODER &= ~(0b11 << 26);
 *     GPIOC->MODER |=  (0b10 << 26);   // 10 = Alternate Function,
 *                                         not Output (01)
 *
 *   OBSERVED: LED does not blink — stays off / in a passive state.
 *
 *   WHY: In Alternate Function mode, PC13 is disconnected from BSRR
 *   entirely; the pin is routed to whichever AF peripheral function is
 *   selected. Since no AF peripheral was actually configured, the pin
 *   is effectively inert as far as this code is concerned. BSRR writes
 *   still execute as instructions — they just have no effect on the
 *   physical pin while it's in this mode.
 *
 * ---------------------------------------------------------------------
 *
 * EXPERIMENT 3 — HAL_Delay coexisting with raw register writes.
 *
 *   Not a failure — a coexistence check. The working code above
 *   already uses HAL_Delay() alongside raw GPIOC->BSRR writes. This
 *   confirms HAL and bare-metal register access are not mutually
 *   exclusive: HAL_Delay is backed by SysTick (configured inside
 *   HAL_Init), which is a completely separate mechanism from the GPIO
 *   register writes. Swapping HAL_Delay(100) for a manual busy-loop
 *   for(volatile int i = 0; i < N; i++); still blinks, but the timing
 *   becomes compiler/optimization-level dependent rather than a real
 *   millisecond value — see blink_day6_systick_delay_raw.c for the
 *   from-scratch replacement that fixes this.
 *
 * ---------------------------------------------------------------------
 *
 * NOTE ON OPEN-DRAIN (not tested on this pin, documented for reference)
 *
 *   PC13 here uses push-pull (OTYPER = 0, the reset default) — the pin
 *   actively drives both high and low. Open-drain (OTYPER = 1) can
 *   only actively pull low and relies on a pull-up (internal or
 *   external) for the high level. This matters on shared multi-master
 *   buses like I2C: if every device could actively drive high AND low,
 *   two devices disagreeing at the same instant would short the line.
 *   Open-drain means every device can only pull low or release —
 *   whoever pulls low "wins" safely, no matter how many devices share
 *   the bus. Relevant again in Week 5 (I2C/SPI).
 * =====================================================================
 */
