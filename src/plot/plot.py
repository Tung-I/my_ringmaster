# Description: Plotting functions for the src/output/.txt files

import matplotlib.pyplot as plt
import numpy as np
import os
import sys
import argparse


def plot_encoding(filename, save_folder, fps=24):
    # Read the data from the txt file (frame_id,target_bitrate,frame_size_in_byte,frame_generatation_timestamp_in_us,frame_encoder_timestamp_in_us,EWMA RTT (ms))
    data = np.loadtxt(filename, delimiter=',')

    # Calculate encoding latency in ms; the x-axis is time (sec), the y-axis is average encoding latency (ms) within each second
    encoding_latency = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        # calculate encoding latency in ms
        encoding_latency[i] = (data[i,4] - data[i,3]) / 1000
    # Re-scale the x_axis to time (sec); it should be n_frame / fps
    x_axis = np.arange(0, encoding_latency.shape[0], fps) / fps
    # Accumulate the encoding latency within each second (i.e., 24 frames), and then reset the counter in the next second
    # The x-axis is time (sec), the y-axis is average encoding latency (ms) within each second
    encoding_latency = np.cumsum(encoding_latency)
    encoding_latency = encoding_latency[::fps] / fps
    for i in range(encoding_latency.shape[0]-1, 0, -1):
        encoding_latency[i] = encoding_latency[i] - encoding_latency[i-1]

    # Plot encoding latency
    plt.plot(x_axis, encoding_latency, 'c-')
    # Set title and labels
    plt.title("Encoding Latency")
    plt.xlabel("Time (s)")
    plt.ylabel("Encoding Latency (ms)")
    # Save figure
    save_path = os.path.join(save_folder, 'encoding_latency.png')
    plt.savefig(save_path)
    # Clear figure
    plt.clf()

    # Calculate EWMA RTT in ms; the x-axis is time (sec), the y-axis is average EWMA RTT (ms) within each second
    ewma_rtt = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        # calculate EWMA RTT in ms
        ewma_rtt[i] = data[i,5]
    # Re-scale the x_axis to time (sec); it should be n_frame / fps
    x_axis = np.arange(0, ewma_rtt.shape[0], fps) / fps
    # Accumulate the EWMA RTT within each second (i.e., 24 frames), and then reset the counter in the next second
    # The x-axis is time (sec), the y-axis is average EWMA RTT (ms) within each second
    ewma_rtt = np.cumsum(ewma_rtt)
    ewma_rtt = ewma_rtt[::fps] / fps
    for i in range(ewma_rtt.shape[0]-1, 0, -1):
        ewma_rtt[i] = ewma_rtt[i] - ewma_rtt[i-1]
    # Plot EWMA RTT
    plt.plot(x_axis, ewma_rtt, 'c-')
    # Set title and labels
    plt.title("EWMA RTT")
    plt.xlabel("Time (s)")
    plt.ylabel("EWMA RTT (ms)")
    # Save figure
    save_path = os.path.join(save_folder, 'ewma_rtt.png')
    plt.savefig(save_path)
    # Clear figure
    plt.clf()


def plot_throughput(filename, save_folder, fps=24):
    data = np.loadtxt(filename, delimiter=',')  # [frame_id,frame_size_in_byte,frame_decodable_timestamp_in_us]
    # Calculate throughput (bitrate) per frame in Mbps
    bitrate = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        bitrate[i] = (data[i,1] * 8 / 1000000) # bytes -> Mbits
    # Accumulate the bitrate within each second (i.e., every 24 frames)
    # so that the x-axis is time (sec) and the y-axis is average bitrate (kbps) within each second
    bitrate = np.cumsum(bitrate)
    bitrate = bitrate[::fps]
    for i in range(bitrate.shape[0]-1, 0, -1):
        bitrate[i] = bitrate[i] - bitrate[i-1]
    # Remove the first 2*fps frames (i.e., 2 second) because the throughput is not stable
    bitrate = bitrate[2:]
    # Preprar the x-axis
    x_axis = np.arange(0, bitrate.shape[0])
    # Plot bitrate
    plt.plot(x_axis, bitrate, 'c-')
    # Set title and labels
    plt.xlabel("Streaming Time (s)")
    plt.ylabel("Throughput (Mbps)")
    # Set the legend
    plt.legend(['Baseline'])
    # Save figure
    save_path = os.path.join(save_folder, 'throughput.png')
    plt.savefig(save_path)
    # Clear figure
    plt.clf()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--throughput', required=False, help='file path')
    parser.add_argument('-r', '--rtt', required=False, help='file path')
    parser.add_argument('--fps', required=False, default=24, type=int, help='FPS')
    parser.add_argument('-o', '--output', required=True, help='output folder')
    args = parser.parse_args()

    if args.throughput is not None:
        plot_throughput(args.throughput, args.output, fps=args.fps) 

if __name__ == '__main__':
    main()