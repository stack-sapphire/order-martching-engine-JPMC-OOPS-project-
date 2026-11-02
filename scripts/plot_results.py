import pandas as pd
import matplotlib.pyplot as plt
import os

# Read CSV
data = pd.read_csv("profits.csv")
time = data["time"]

# FIXED — create graphs folder if it doesn't exist
os.makedirs("graphs", exist_ok=True)

plt.figure(figsize=(10, 6))

for column in data.columns[1:]:
    is_fast = str(column) == "5"
    plt.plot(
        time, data[column],
        linewidth=2 if is_fast else 1.5,
        linestyle="--" if is_fast else "-",
        color="red" if is_fast else None,
        label="FastTrader" if is_fast else f"Trader {column}",
        zorder=5 if is_fast else 2,
    )

plt.xlabel("Time (seconds)")
plt.ylabel("Profit / Loss ($)")
plt.title("Trader Profit Over Time")
plt.legend()
plt.grid(True)
plt.savefig("graphs/profit_graph.png")
print("Graph saved successfully!")
plt.show()
