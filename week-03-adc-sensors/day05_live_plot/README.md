# Day 5 — Streaming to Python: Live Plot and Baseline Dataset

## Overview
Streams three ADC channels from the MCU to a PC over UART in CSV
format, and uses a Python script to parse, plot live, and log the data
to disk. Produces the first labeled dataset used later in the Week 7
anomaly detection sprint.

## Files
- `day05_adc_stream.c` — MCU-side changes: reads photoresistor,
  thermistor, and internal temperature sensor, and prints all three as
  a single CSV line per sample.
- `live_plot.py` — PC-side script: reads the serial stream, plots all
  three channels live with a scrolling window, and logs every sample
  to a CSV file on disk.
- `data/normal_baseline.csv` — two minutes of undisturbed sensor data,
  collected with this script, intended as the "normal" reference
  dataset for later anomaly comparison.
- `notes/live_plot_screenshot.png` — screenshot of the live plot while
  actively responding to sensor changes.

## Hardware
- STM32F405RGT6 (Blackpill)
- Photoresistor voltage divider on PA0
- Thermistor voltage divider on PA1
- USART2 (PA2/PA3) at 115200 baud

## Functionality

### MCU side (`day05_adc_stream.c`)
Reads channel 0 (photoresistor), channel 1 (thermistor), and channel 16
(internal chip temperature sensor) once every 50 ms, and prints all
three as a single comma-separated line with no labels, so the PC side
can parse it directly:
```
3421,1802,1680
```

### PC side (`live_plot.py`)
1. Opens the board's serial port and reads one line per available
   sample.
2. Parses each line into three integers, appending each to its own
   fixed-length rolling buffer (100 samples), which produces the
   scrolling-window effect on the plot.
3. Updates a live matplotlib plot with all three channels every 50 ms.
4. Appends each valid parsed line to a CSV file on disk, flushing
   after every write so data already collected survives an unexpected
   script interruption.

## Setup / Usage
1. Flash the board with `day05_adc_stream.c` integrated into a
   generated `main.c` (see note below).
2. Install pyserial and matplotlib if not already installed:
   ```
   pip install pyserial matplotlib
   ```
3. Update `PORT` in `live_plot.py` to match the board's COM port
   (Device Manager → Ports).
4. Run:
   ```
   python live_plot.py
   ```
5. A window will open showing three live scrolling lines. Disturb each
   sensor individually to confirm each line responds independently.
6. To collect a baseline dataset, let the script run undisturbed for
   two minutes, then close the window. Move the generated
   `normal_baseline.csv` into `data/` within this folder.

## Note on File Contents
`day05_adc_stream.c` contains only the code written for this session
and omits CubeIDE/CubeMX-generated boilerplate (`HAL_Init`,
`SystemClock_Config`, `Error_Handler`, `USER CODE` markers, etc.). The
listed functions and configuration lines should be placed into their
corresponding sections of a generated `main.c` to build.
