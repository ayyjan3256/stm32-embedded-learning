# Day 4 — Joystick: Dual-Axis ADC + Digital Button

## Overview
Reads a KY-023 joystick module's two analog axes (X and Y) alongside its
digital pushbutton, and classifies the stick's position into a direction
using a deadzone around center. Demonstrates combining analog and
digital input handling in the same peripheral configuration.

## File
- `day04_joystick.c` — button GPIO configuration, dual-axis ADC read,
  and the direction classification function.

## Hardware
- STM32F405RGT6 (Blackpill)
- KY-023 joystick module: VRx to PA0, VRy to PA1, SW to PB0, VCC to
  3.3V, GND to GND
- USART2 (PA2/PA3) at 115200 baud for serial output

## Functionality
1. PB0 is configured as a digital input with an internal pull-up
   resistor. The joystick's button pin is active-low, so the pin reads
   high when unpressed and low when pressed.
2. VRx and VRy are read using the parameterized `ADC_Read()` function
   from Day 3, on channels 0 and 1 respectively.
3. `joystick_direction()` compares both axes against a measured center
   value with a deadzone tolerance, returning `"CENTER"` when within
   that window, or `"LEFT"`, `"RIGHT"`, `"UP"`, or `"DOWN"` based on
   whichever axis has moved furthest from center.
4. The button state, raw axis values, and classified direction are
   printed once every 100 ms.

## Setup / Usage
1. Wire the joystick as described above.
2. Flash the board with `day04_joystick.c` integrated into a generated
   `main.c` (see note below).
3. Open a serial terminal at 115200 baud.
4. Move the stick to each extreme and observe the printed direction
   change; press the button and confirm `BTN` drops to 0 while held.
5. If `CENTER` reports too easily during small movements, or real
   movements fail to register, adjust `JOY_DEADZONE` in the source.

## Calibration Note
`JOY_CENTER` is set to 2048, the theoretical midpoint of a 12-bit ADC
range. Actual resting values vary by unit due to potentiometer
tolerance. Measure your joystick's actual resting X/Y values and update
`JOY_CENTER` accordingly for more accurate centering.

## Note on File Contents
This file contains only the code written for this session and omits
CubeIDE/CubeMX-generated boilerplate (`HAL_Init`, `SystemClock_Config`,
`Error_Handler`, `USER CODE` markers, etc.). The listed functions and
configuration lines should be placed into their corresponding sections
of a generated `main.c` to build.
