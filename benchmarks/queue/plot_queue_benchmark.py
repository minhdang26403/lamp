import pandas as pd
import matplotlib.pyplot as plt
import re

# Load the data from the CSV file
df = pd.read_csv("queue_benchmark.csv")

# Extract queue type, operations count, and thread count from the name column
df["queue_type"] = df["name"].apply(
    lambda x: re.search(r"<([A-Za-z]+)(?:Wrapper)?<", x).group(1)
)
df["operations"] = df["name"].apply(lambda x: int(re.search(r"/(\d+)/", x).group(1)))
df["threads"] = df["name"].apply(
    lambda x: int(re.search(r"/(\d+)/real_time", x).group(1))
)

# Map queue types to the desired legend names
queue_type_mapping = {
    "BoundedQueue": "BoundedQueue",
    "LockFreeQueueRecycle": "LockFreeQueueRecycle",
    "LockFreeQueue": "LockFreeQueue",
    "UnboundedQueue": "UnboundedQueue",
}

# Fix BoundedQueueWrapper to be just BoundedQueue
df["queue_type"] = df["queue_type"].replace("BoundedQueueWrapper", "BoundedQueue")

# Convert items_per_second to numeric
df["items_per_second"] = pd.to_numeric(df["items_per_second"], errors="coerce")

# Operations counts to plot
operations_counts = [10000, 100000, 1000000]

# Set up the figure with subplots
fig, axes = plt.subplots(1, 3, figsize=(18, 6))
fig.suptitle("Queue Implementation Performance Comparison", fontsize=16)

# Colors for each queue type
colors = {
    "BoundedQueue": "blue",
    "LockFreeQueueRecycle": "green",
    "LockFreeQueue": "red",
    "UnboundedQueue": "purple",
}

# Plot for each operations count
for i, ops_count in enumerate(operations_counts):
    ax = axes[i]

    # Filter data for this operations count
    data_for_plot = df[df["operations"] == ops_count]

    # Group by queue_type and threads, then get the mean items_per_second
    grouped = (
        data_for_plot.groupby(["queue_type", "threads"])["items_per_second"]
        .mean()
        .reset_index()
    )

    # Plot each queue type
    for queue_type in queue_type_mapping.values():
        queue_data = grouped[grouped["queue_type"] == queue_type]
        if not queue_data.empty:
            ax.plot(
                queue_data["threads"],
                queue_data["items_per_second"],
                marker="o",
                label=queue_type,
                color=colors[queue_type],
            )

    # Set x-axis to use actual thread counts
    thread_counts = sorted(grouped["threads"].unique())
    ax.set_xticks(thread_counts)
    ax.set_xticklabels(thread_counts)

    # Set labels and title
    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Operations Per Second")
    ax.set_title(f"Operations Count: {ops_count}")
    ax.grid(True, linestyle="--", alpha=0.7)

    # Use scientific notation for y-axis
    ax.ticklabel_format(axis="y", style="sci", scilimits=(0, 0))

    # Add legend
    ax.legend()

# Adjust layout
plt.tight_layout(rect=[0, 0, 1, 0.95])  # Make room for the suptitle
plt.savefig("queue_performance_comparison.png", dpi=300, bbox_inches="tight")
plt.show()
