import matplotlib.pyplot as plt
import numpy as np
import sys

# Load the latencies from the file
file_path = sys.argv[1]
with open(file_path, "r") as file:
    latencies = [float(line.strip()) for line in file.readlines()]

# Sort latencies to calculate CDF
latencies = np.sort(latencies)
cdf = np.arange(1, len(latencies) + 1) / len(latencies)

# Plot the CDF
plt.figure(figsize=(8, 6))
plt.plot(latencies, cdf, marker='.', linestyle='-', label="CDF of Latencies")
plt.title("CDF of Latencies")
plt.xlabel("Latency (seconds)")
plt.ylabel("CDF")
plt.grid(True)
plt.legend()
plt.show()
