## Goal
Get I2C1 initialized on the WeAct STM32F405 Blackpill and confirm a device (MPU6050) responds on the bus, before writing any real read/write functions.

## Wiring
| STM32F405 Pin | Function | Connects To |
| :--- | :--- | :--- |
| PB6 | I2C1_SCL (AF4) | MPU6050 SCL |
| PB7 | I2C1_SDA (AF4) | MPU6050 SDA |
| 3V3 | Power | MPU6050 VCC |
| GND | Ground | MPU6050 GND |

*Note: GY-521 breakout boards carry their own onboard 4.7kΩ pull-ups to VCC — the STM32's internal PUPDR pull-ups are redundant on top of these but not harmful.*

## GPIO Configuration (PB6/PB7)
*   **MODER = 10** (Alternate Function mode) — bit field is 2 bits per pin, so pin 6 → bits [13:12], pin 7 → bits [15:14].
*   **AFR[0] = AF4** for both pins — I2C1 lives on Alternate Function 4 on the F405. AFRL covers pins 0–7, 4 bits per pin.
*   **OTYPER = 1** (open-drain) — mandatory for I2C; the bus is wired-AND, push-pull would fight other devices trying to pull the line low.
*   **OSPEEDR = 11** (very high speed) — not strictly required at 100kHz, but harmless.
*   **PUPDR = 01** (pull-up) — internal pull-up as backup to the module's onboard resistors.

## I2C Timing Register Math (100kHz Standard Mode, APB1 = 42MHz)
*   **CR2[FREQ] = 42** — tells the peripheral its own APB1 clock frequency in MHz (does not set the clock, just informs the timing logic).
*   **CCR = 210** — APB1clk / (2 × target) = 42,000,000 / (2 × 100,000) = 210.
*   **TRISE = 43** — (1000ns / APB1_period) + 1 = (1000ns × 42MHz) + 1 = 43.

## Init Sequence (order matters)
1.  Enable GPIOB clock, configure MODER/AFR/OTYPER/OSPEEDR/PUPDR — before touching I2C1 registers.
2.  Enable I2C1 clock (APB1ENR).
3.  Software reset: set SWRST, then clear it. Resets the peripheral to a known state.
4.  Configure CR2, CCR, TRISE — must happen while PE (peripheral enable) is still 0.
5.  Set PE last, to actually enable the peripheral.

## Probe Transaction
Simplest possible bus test — no register read, just confirm a device ACKs its address:
`START → write address byte (0xD0 = 0x68<<1, write bit) → read SR1/SR2 to clear ADDR → STOP`

*Reading SR1 then SR2 is the documented way to clear the ADDR flag — this is not optional, ADDR blocks clock stretching until cleared.*

## Pitfalls/Quirks
*   **Clock config mismatch** is the #1 suspect for "nothing happens on the bus." If `SystemClock_Config()` doesn't match the board's actual HSE crystal, execution parks in `Error_Handler()` and initialization never runs. 
*   Confirmed on this board: HSE = 8MHz, so PLLM=4, PLLN=168, PLLP=2 → 168MHz SYSCLK → 42MHz APB1 is correct as-is.
*   **Logic Analyzer Capture:** Capturing a one-shot transaction is timing-painful. Wrapping the transaction in a `while` loop or `for` loop with a delay makes it easy to capture on PulseView using a falling-edge trigger on SDA.
