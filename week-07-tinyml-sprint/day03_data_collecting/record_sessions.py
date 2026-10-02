"""Record labeled photoresistor sessions from the STM32 UART stream to CSV.

Each session is saved as data/<name>.csv with the header "timestamp,photo".
The timestamp (ms) is derived from the sample index and the declared RATE,
not from the PC clock, because PC timing over serial is jittery.

Usage:  python record_sessions.py
Needs:  pip install pyserial numpy
"""
import os
import time

import numpy as np
import serial

# ---------------- SETTINGS ----------------
PORT = "COM5"      # change to your COM port (Device Manager)
BAUD = 115200
RATE = 200         # Hz, must match the firmware (TIM2 PSC=8399, ARR=49)
DURATION = 60      # seconds per session
FOLDER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
# ------------------------------------------

os.makedirs(FOLDER, exist_ok=True)
expected = RATE * DURATION

ser = serial.Serial(PORT, BAUD, timeout=1)
print(f"Opened {PORT} at {BAUD} baud")


def record():
    ser.reset_input_buffer()
    ser.readline()  # discard a possibly partial first line
    vals = []
    start = time.time()
    last_t, last_n, last_data = start, 0, start
    while time.time() - start < DURATION:
        line = ser.readline().decode(errors="ignore").strip()
        now = time.time()
        if line.isdigit():
            vals.append(int(line))
            last_data = now
        elif now - last_data > 5:
            print("No data for 5 s. Is the firmware running and the port right?")
            break
        if now - last_t >= 5:
            print(f"  {(len(vals) - last_n) / (now - last_t):.1f} samples/s")
            last_t, last_n = now, len(vals)
    return np.array(vals)


try:
    while True:
        name = input("\nSession name (e.g. normal_01_day), or Enter to quit: ").strip()
        if not name:
            break
        input(f"Get ready for '{name}'. Press Enter to start {DURATION} s recording...")
        x = record()
        if len(x) == 0:
            print("Nothing recorded, file not saved.")
            continue

        path = os.path.join(FOLDER, name + ".csv")
        with open(path, "w") as f:
            f.write("timestamp,photo\n")
            for i, v in enumerate(x):
                f.write(f"{i * 1000 // RATE},{v}\n")

        print(f"Saved {path}")
        print(f"  samples {len(x)} (expected about {expected}), "
              f"mean {x.mean():.1f}, std {x.std():.1f}, min {x.min()}, max {x.max()}")
        if abs(len(x) - expected) > 0.05 * expected:
            print("  WARNING: sample count is off, so the real rate is not 200 Hz or data was dropped.")
        if x.min() < 50 or x.max() > 4045:
            print("  WARNING: reading hit the ADC limits. Check the sensor for a spike or a disturbance.")
finally:
    ser.close()
    print("Port closed.")
