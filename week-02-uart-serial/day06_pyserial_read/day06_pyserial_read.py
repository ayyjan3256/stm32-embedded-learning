"""
Week 2, Day 6 — pyserial on the PC side

Reads "COUNT:N" lines sent by the MCU (Day 5's printf loop) over the
ST-Link virtual COM port, parses out the integer, and prints it.

Requires: pip install pyserial
"""

import serial

PORT = 'COM4'
BAUD = 115200

# timeout is generous relative to the MCU's ~1s send interval, to avoid
# racing a readline() timeout against an in-progress line
ser = serial.Serial(PORT, BAUD, timeout=2)

while True:
    line = ser.readline()

    if not line:
        # nothing arrived within the timeout window — just loop again
        continue

    text = line.decode('ascii').strip()   # bytes -> str, drop trailing \n
    parts = text.split(':')               # "COUNT:0" -> ["COUNT", "0"]

    if len(parts) != 2:
        print("Unexpected format:", text)
        continue

    count = int(parts[1])                 # str -> actual integer
    print(f"Received count: {count}")
