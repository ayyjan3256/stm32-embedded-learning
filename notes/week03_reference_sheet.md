# Week 3 Reference Sheet — ADC and Analog Sensors

## 1. ADC1's Bus
ADC1 sits on **APB2**, not AHB1 — clock enable lives in
`RCC->APB2ENR` (`ADC1EN` bit), separate from GPIO's `AHB1ENR`. A macro
whose name references one bus (e.g. `RCC_APB2ENR_ADC1EN`) must be
written into the matching register — writing it into the wrong ENR
register silently leaves the peripheral's clock disabled.

## 2. The EOC Flag
End Of Conversion — a bit in `ADC1->SR`, set by hardware once a
conversion result is ready in `DR`. Poll it after triggering `SWSTART`:
```c
while (!(ADC1->SR & ADC_SR_EOC));
```
Reading `DR` automatically clears EOC as a side effect.

## 3. Raw-to-Voltage Formula
```
Voltage = (ADC_raw / 4095.0) × VREF   (VREF = 3.3V on this board)
```
Requires floating-point division (`4095.0f`) and `-u _printf_float`
added to the linker's Miscellaneous flags to print correctly.

## 4. Why MODER = 11 for an Analog Pin
`11` disconnects a pin's digital input/output circuitry and connects it
to the ADC's sampling circuit. `01` (output mode) is a different,
unrelated configuration — using it instead of `11` gives silently
meaningless digital-mode behavior rather than a real analog reading.

## 5. SQR3 — Overwrite, Not OR
SQR3's SQ1 field (bits 4:0) selects which channel converts next. Each
channel switch must be a full overwrite:
```c
ADC1->SQR3 = channel;   // not |=
```
Using `|=` here accumulates channel-select bits across calls instead of
cleanly switching, causing one channel's data to silently bleed into
another's readings.

## 6. Sample Time Tradeoff
`SMPR2`/`SMPR1` set how many ADC clock cycles are spent letting the
input settle before conversion begins (3 to 480 cycles, 3-bit field per
channel). Longer sample time suits high-impedance sources like
resistive dividers; shorter sample time trades accuracy for speed.
Measured directly with the Cortex-M4's DWT cycle counter: real speedup
from fastest to slowest setting was smaller than the theoretical cycle
ratio, due to fixed software polling overhead in the read function.

## 7. Averaging and Noise
Random noise sources — power supply ripple, thermal noise, digital
switching noise, input impedance effects — exist on every individual
reading regardless of averaging. Averaging multiple samples statistically
cancels out random above/below deviations, reducing the *visible*
effect of jitter on the final number without eliminating its physical
causes.

## 8. Internal vs. External Temperature Sensing
Internal sensor (ADC1 channel 16, enabled via
`ADC123_COMMON->CCR |= ADC_CCR_TSVREFE`) measures the chip's own die
temperature — reads warmer than ambient due to self-heating, and is
largely insensitive to brief external touch due to the chip's
packaging. External thermistor readings require converting the voltage
divider's output to resistance, then applying the Steinhart-Hart
equation — a different, non-linear formula, not interchangeable with
the internal sensor's linear one.

## 9. Standard Deviation as an Anomaly Signal
Standard deviation, not mean alone, is the more reliable indicator that
a sensor was disturbed: an actively disturbed channel shows much higher
variance than an undisturbed one, even when the mean stays similar.
This numeric distinction is the seed of the Week 7/8 anomaly detection
capstone.

## 10. RES Bits — A 2-Bit Field, Not a Flag
`ADC1->CR1` bits 25:24 select 12/10/8/6-bit resolution. Because this is
a 2-bit field rather than a single on/off bit, changing it safely
requires clear-then-set:
```c
ADC1->CR1 &= ~(3 << 24);
ADC1->CR1 |= (res << 24);
```
A plain OR can combine incorrectly with whatever value was previously
set, producing an unintended resolution.
