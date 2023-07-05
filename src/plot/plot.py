# Description: Plotting functions for the src/output/.txt files

import matplotlib.pyplot as plt
import numpy as np
import os
import sys
import argparse


def plot_encoder(filename, save_folder, fps=24):
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


def plot_decoder(filename, save_folder, fps=24):
    # Read the data from the txt file (frame_id,frame_size_in_byte,frame_decodable_timestamp_in_us)
    data = np.loadtxt(filename, delimiter=',')

    # Calculate throughput (bitrate); the x-axis is time (sec), the y-axis is bitrate (kbps)
    actual_bitrate = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        # calculate actual bitrate in kbps
        actual_bitrate[i] = (data[i,1] * 8 / 1000)
    # Re-scale the x_axis to time (sec); it should be n_frame / fps
    x_axis = np.arange(0, actual_bitrate.shape[0], fps) / fps
    # Accumulate the bitrate within each second (i.e., 24 frames), and then reset the counter in the next second
    # The x-axis is time (sec), the y-axis is average bitrate (kbpss) within each second
    actual_bitrate = np.cumsum(actual_bitrate)
    actual_bitrate = actual_bitrate[::fps]
    for i in range(actual_bitrate.shape[0]-1, 0, -1):
        actual_bitrate[i] = actual_bitrate[i] - actual_bitrate[i-1]
    # Plot actual bitrate
    plt.plot(x_axis, actual_bitrate, 'c-')
    # Set title and labels
    plt.title("Throughput (Bitrate)")
    plt.xlabel("Time (s)")
    plt.ylabel("Bitrate (kbps)")
    # Save figure
    save_path = os.path.join(save_folder, 'receiver_bitrate.png')
    plt.savefig(save_path)
    # Clear figure
    plt.clf()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--log1', required=True, help='encoder log path')
    parser.add_argument('--log2', required=True, help='decoder log path')
    parser.add_argument('--output', required=True, help='output figure path')
    args = parser.parse_args()

    # Call plot_encoder
    plot_encoder(args.log1, args.output)
    plot_decoder(args.log2, args.output)

if __name__ == '__main__':
    main()