# Week 5, Day 7: Full System Integration

## Overview
This represents the culmination of Week 5's bare-metal communications drivers. The STM32F405 acts as a master device managing two distinct protocols and three peripherals simultaneously.

## Features
*   **Shared I2C Bus:** The MPU6050 (0x68) and PCF8574 LCD Backpack (0x27) share the `I2C1` bus on PB6/PB7.
*   **Data Acquisition:** A 14-byte I2C burst read continuously fetches acceleration, rotation, and temperature without data tearing.
*   **HD44780 LCD Control:** Custom 4-bit nibble formatting translates raw bytes into enable-pulse commands via the GPIO expander.
*   **Standalone SPI:** The hardware `SPI1` peripheral remains initialized and ready on PA5-PA7, demonstrating safe multi-protocol pin usage.

## Combined Project

MPU6050 + LCD share I2C1 (PB6/PB7); SPI1 hardware loopback runs independently on separate pins (PA5/6/7), proving the drivers coexist in one build without interfering — confirmed by construction, since each uses entirely distinct GPIO pins and peripheral clock-enable bits.
