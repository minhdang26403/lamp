import pandas as pd
import matplotlib.pyplot as plt

# Read the CSV file
df = pd.read_csv("list_benchmark_result.csv")  # Replace with your actual filename


# Function to extract thread count and list size from benchmark name
def parse_benchmark_name(name):
    parts = name.split("/")
    threads = int(parts[1])
    size = int(parts[2])
    list_type = parts[0].split("<")[1].split(">")[0]
    workload = parts[0].split("_")[1].split("<")[0]
    return workload, list_type, threads, size


# Parse the benchmark names and create new columns
df["workload"], df["list_type"], df["threads"], df["size"] = zip(
    *df["name"].apply(parse_benchmark_name)
)

workload_info = {
    "ReadHeavyWorkload": "80% contains, 15% add, 5% remove",
    "WriteHeavyWorkload": "20% contains, 40% add, 40% remove",
    "BalancedWorkload": "33% contains, 33% add, 33% remove",
}


# Function to create plot for a specific workload
def create_workload_plot(workload_type, size_to_plot, ax):
    workload_data = df[(df["workload"] == workload_type) & (df["size"] == size_to_plot)]

    list_types = [
        "CoarseList",
        "FineList",
        "OptimisticList",
        "LazyList",
        "LockFreeList",
    ]
    colors = ["blue", "red", "green", "purple", "orange"]

    for list_type, color in zip(list_types, colors):
        list_data = workload_data[workload_data["list_type"] == list_type]
        # Sort by threads to ensure proper line plotting
        list_data = list_data.sort_values("threads")
        ax.plot(
            list_data["threads"],
            list_data["ops"],
            label=list_type,
            color=color,
            marker="o",
        )

    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Operations per Second")
    ax.set_title(
        f"{workload_type} - {workload_info[workload_type]} (Size={size_to_plot})"
    )
    ax.legend()
    ax.grid(True)


sizes = [100, 1000, 10000]

for size in sizes:
    # Create figure with three subplots
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(8, 12))

    # Create plots for each workload
    create_workload_plot("ReadHeavyWorkload", size, ax1)
    create_workload_plot("WriteHeavyWorkload", size, ax2)
    create_workload_plot("BalancedWorkload", size, ax3)

    # Adjust layout to prevent overlap
    plt.tight_layout()

    # Display the plots
    plt.show()
