import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns

# Set style
sns.set(style="whitegrid")
plt.rcParams["figure.figsize"] = (14, 8)

# Read combined benchmark CSV
df = pd.read_csv("combined_benchmark_results.csv")

# Ensure consistent order for the tests
df['Test'] = pd.Categorical(df['Test'], categories=df['Test'].unique(), ordered=True)

### 1. Time Taken Plot (ms)
plt.figure()
sns.barplot(data=df, x="Test", y="Time_ms", hue="Allocator")
plt.title("Time Taken per Test")
plt.ylabel("Time (ms)")
plt.xlabel("Test")
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig("graph_time_taken.png")
plt.close()

### 2. Throughput Plot (KOps/sec)
plt.figure()
sns.barplot(data=df, x="Test", y="KOps_per_sec", hue="Allocator")
plt.title("Throughput per Test")
plt.ylabel("Throughput (KOps/sec)")
plt.xlabel("Test")
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig("graph_throughput.png")
plt.close()

### 3. Peak Memory Usage Plot (MB)
plt.figure()
sns.barplot(data=df, x="Test", y="Peak_Memory_MB", hue="Allocator")
plt.title("Peak Memory Usage per Test")
plt.ylabel("Memory (MB)")
plt.xlabel("Test")
plt.xticks(rotation=45)
plt.tight_layout()
plt.savefig("graph_peak_memory.png")
plt.close()

print("✅ Graphs saved: 'graph_time_taken.png', 'graph_throughput.png', 'graph_peak_memory.png'")
