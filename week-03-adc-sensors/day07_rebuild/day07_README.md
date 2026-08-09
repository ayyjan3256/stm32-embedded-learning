# Day 7 — Rebuild From Memory, Anomaly Dataset, Statistical Comparison

## Overview
Closes out Week 3 by rebuilding the week's core ADC setup from memory,
collecting a second labeled dataset under deliberate sensor disturbance,
and comparing it statistically against Day 5's baseline dataset.

## Files
- `day07_rebuild.c` — final, corrected version of the week's core
  three-channel ADC + UART setup, rebuilt from memory and fixed against
  the working Day 2-6 code.
- `analyze_datasets.py` — loads both CSV datasets and computes mean and
  standard deviation per channel for comparison.

## Rebuild Process
The initialization and read code was written without reference to
earlier working files, then compared against the known-working Day 2-6
code. Gaps found and corrected during review are documented in the
header comment of `day07_rebuild.c`, and include a missing USART2
enable sequence that caused the UART peripheral to never activate,
leaving `printf` calls blocked indefinitely on the first byte.

## Dataset Collection
Using `day05_live_plot/live_plot.py` (see Day 5), two labeled datasets
were collected:
- `normal_baseline.csv` — sensors left undisturbed for two minutes.
- `anomaly_data.csv` — photoresistor and thermistor actively and
  repeatedly disturbed for two minutes.

The internal temperature sensor (channel 16) was not used as a target
for disturbance, since it measures die temperature through the chip
package and responds negligibly to external touch over this timescale.
It instead served as a stable reference channel.

## Statistical Comparison
`analyze_datasets.py` computes the mean and standard deviation of each
channel across both datasets. Standard deviation, rather than mean
alone, was the primary indicator of disturbance: channels that were
actively disturbed show substantially higher standard deviation in the
anomaly dataset, while the internal temperature channel remains
comparable between both datasets.

## Setup / Usage
1. Flash the board with `day07_rebuild.c` integrated into a generated
   `main.c`.
2. Collect both datasets using `live_plot.py` as described in the Day 5
   folder, saving each to its own file.
3. Install numpy if not already installed:
   ```
   pip install numpy
   ```
4. Update the file paths at the top of `analyze_datasets.py` if your
   CSV files are stored in a different location.
5. Run:
   ```
   python analyze_datasets.py
   ```

## Note on File Contents
`day07_rebuild.c` contains only the code written for this session and
omits CubeIDE/CubeMX-generated boilerplate (`HAL_Init`,
`SystemClock_Config`, `Error_Handler`, `USER CODE` markers, etc.). The
listed functions and configuration lines should be placed into their
corresponding sections of a generated `main.c` to build.
