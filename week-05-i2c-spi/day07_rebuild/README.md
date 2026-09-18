# Week 5, Day 7: Full System Integration

## Overview
This represents the culmination of Week 5's bare-metal communications drivers. The STM32F405 acts as a master device managing two distinct protocols and three peripherals simultaneously.

## Features
*   **Shared I2C Bus:** The MPU6050 (0x68) and PCF8574 LCD Backpack (0x27) share the `I2C1` bus on PB6/PB7.
*   **Data Acquisition:** A 14-byte I2C burst read continuously fetches acceleration, rotation, and temperature without data tearing.
*   **HD44780 LCD Control:** Custom 4-bit nibble formatting translates raw bytes into enable-pulse commands via the GPIO expander.
*   **Standalone SPI:** The hardware `SPI1` peripheral remains initialized and ready on PA5-PA7, demonstrating safe multi-protocol pin usage.
