import numpy as np

def load_csv(filename):
    data = np.loadtxt(filename, delimiter=',')
    return data

normal = load_csv('day05_live_plot/data/normal_baseline.csv')
anomaly = load_csv('day05_live_plot/data/anomaly_data.csv')

print("=== Normal Baseline ===")
print(f"Samples: {len(normal)}")
print(f"Channel 0 (Photoresistor) - Mean: {normal[:,0].mean():.2f}  Std Dev: {normal[:,0].std():.2f}")
print(f"Channel 1 (Thermistor)    - Mean: {normal[:,1].mean():.2f}  Std Dev: {normal[:,1].std():.2f}")
print(f"Channel 16 (Internal Temp)- Mean: {normal[:,2].mean():.2f}  Std Dev: {normal[:,2].std():.2f}")

print("\n=== Anomaly Data ===")
print(f"Samples: {len(anomaly)}")
print(f"Channel 0 (Photoresistor) - Mean: {anomaly[:,0].mean():.2f}  Std Dev: {anomaly[:,0].std():.2f}")
print(f"Channel 1 (Thermistor)    - Mean: {anomaly[:,1].mean():.2f}  Std Dev: {anomaly[:,1].std():.2f}")
print(f"Channel 16 (Internal Temp)- Mean: {anomaly[:,2].mean():.2f}  Std Dev: {anomaly[:,2].std():.2f}")
