"""
Week 2, Day 7 — Bidirectional Command Console (PC side)

Sends "LED ON" / "LED OFF" over serial to the MCU, and prints back
whatever confirmation line the board sends in response.

Type 'exit' to close the connection and quit.
"""

import serial

PORT = 'COM4'
BAUD = 115200

ser = serial.Serial(PORT, BAUD, timeout=2)

while True:
    command = input("Enter command (LED ON / LED OFF, or 'exit'): ")

    if command.lower() == "exit":
        break

    ser.write((command + '\n').encode('ascii'))

    response = ser.readline()
    if response:
        print("MCU says:", response.decode('ascii').strip())
    else:
        print("No response (timed out) — check connection/board.")

ser.close()
print("Serial connection closed.")
