from matplotlib import pyplot as plt
import numpy as np
import csv

def get_data_from_csv(csv_path, data, type):
    i = 0
    results = []
    with open(csv_path, "r") as csv_file:
        csv_reader = csv.reader(csv_file)
        next(csv_reader)

        for row in csv_reader:
            if row[4] == type:
                if data == "recall":
                    results.append(float(row[0]))
                elif data == "precision":
                    results.append(float(row[1]))
                elif data == "pd":
                    results.append(float(row[2]))
                elif data == "far":
                    results.append(float(row[3]))
                elif data == "width":
                    results.append(float(row[5]))
                elif data == "height":
                    results.append(float(row[6]))

    return results


def draw_plot():
    # Define the data for the two things we want to compare
    bit_16_data = [0.2, 0.5, 0.8, 0.3]
    bit_8_data = [0.4, 0.6, 0.1, 0.9]

    # Define the x-axis labels for the comparison methods
    comparison_methods = ['Precision', 'Recall', 'PD', 'FAR']

    # Set up the figure and axis for the bar chart
    fig, ax = plt.subplots()

    # Set the width of each bar
    bar_width = 0.35

    # Define an array of x positions for each pair of bars for each comparison method
    x_pos = np.arange(len(comparison_methods))

    # Plot the data for 16 bit as a blue chart
    ax.bar(x_pos - bar_width/2, bit_16_data, color='blue', width=bar_width, alpha=0.5, label='16 Bit')

    # Plot the data for 8 bit as a blue chart
    ax.bar(x_pos + bar_width/2, bit_8_data, color='red', width=bar_width, alpha=0.5, label='8 Bit')

    # Set the x-axis tick labels to the comparison method names
    ax.set_xticks(x_pos)
    ax.set_xticklabels(comparison_methods)

    # Set the y-axis limits to 0 and 1
    ax.set_ylim([0, 1])

    # Add a legend to the chart
    ax.legend()

    # Shot bar chart
    plt.show()


if __name__ == "__main__":
    draw_plot()


