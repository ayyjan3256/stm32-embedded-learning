import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

PORT = 'COM4'   # change to match your board's COM port
BAUD = 115200
WINDOW = 100    # show last 100 samples

ser = serial.Serial(PORT, BAUD, timeout=1)
ch0_data = deque([0] * WINDOW, maxlen=WINDOW)
ch1_data = deque([0] * WINDOW, maxlen=WINDOW)
ch16_data = deque([0] * WINDOW, maxlen=WINDOW)

fig, ax = plt.subplots()
line0, = ax.plot([], [], label='Photoresistor')
line1, = ax.plot([], [], label='Thermistor')
line2, = ax.plot([], [], label='Internal Temp. Sensor')
ax.set_ylim(0, 4095)
ax.set_xlim(0, WINDOW)
ax.legend()

log_file = open('normal_baseline.csv', 'a')

def update(frame):
    if ser.in_waiting:
        raw = ser.readline().decode('ascii').strip()
        parts = raw.split(',')
        if len(parts) == 3:
            ch0_data.append(int(parts[0]))
            ch1_data.append(int(parts[1]))
            ch16_data.append(int(parts[2]))
            log_file.write(raw + '\n')
            log_file.flush()
    line0.set_data(range(WINDOW), ch0_data)
    line1.set_data(range(WINDOW), ch1_data)
    line2.set_data(range(WINDOW), ch16_data)
    return line0, line1, line2

ani = animation.FuncAnimation(fig, update, interval=50)
plt.show()
