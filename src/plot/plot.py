# Description: Plotting functions for the src/output/.txt files

import matplotlib.pyplot as plt
import numpy as np
import os
import sys
import argparse



def plot_encoding_time(filename, save_folder, fps=24):
    data = np.loadtxt(filename, delimiter=',')

    encoding_time = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        encoding_time[i] = (data[i,3])

    x_axis = np.arange(0, encoding_time.shape[0], fps) / fps

    encoding_time = np.cumsum(encoding_time)
    encoding_time = encoding_time[::fps] / fps
    for i in range(encoding_time.shape[0]-1, 0, -1):
        encoding_time[i] = encoding_time[i] - encoding_time[i-1]

    plt.plot(x_axis, encoding_time, 'c-')
    plt.title("Encoding Latency")
    plt.xlabel("Streaming Time (s)")
    plt.ylabel("Latency (ms)")
    # Save figure
    save_path = os.path.join(save_folder, 'encoding_time.png')
    plt.savefig(save_path)

    plt.clf()


def plot_decoding_time(filename, save_folder, fps=24):
    data = np.loadtxt(filename, delimiter=',')
    # Calculate decoding latency in ms; the x-axis is time (sec), the y-axis is average decoding latency (ms) within each second
    decoding_time = np.zeros(data.shape[0])
    for i in range(data.shape[0]):
        # calculate decoding latency in ms
        decoding_time[i] = (data[i,3])
    # Re-scale the x_axis to time (sec); it should be n_frame / fps
    x_axis = np.arange(0, decoding_time.shape[0], fps) / fps
    # Accumulate the decoding latency within each second (i.e., 24 frames), and then reset the counter in the next second
    # The x-axis is time (sec), the y-axis is average decoding latency (ms) within each second
    decoding_time = np.cumsum(decoding_time)
    decoding_time = decoding_time[::fps] / fps
    for i in range(decoding_time.shape[0]-1, 0, -1):
        decoding_time[i] = decoding_time[i] - decoding_time[i-1]
    # Plot decoding latency
    plt.plot(x_axis, decoding_time, 'c-')
    # Set title and labels
    plt.title("Decoding Latency")
    plt.xlabel("Streaming Time (s)")
    plt.ylabel("Latency (ms)")
    # Save figure
    save_path = os.path.join(save_folder, 'decoding_time.png')
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
    plt.ylabel("Network Throughput (Mbps)")
    # Set the legend
    plt.legend(['Baseline'])
    # Save figure
    save_path = os.path.join(save_folder, 'throughput.png')
    plt.savefig(save_path)
    # Clear figure
    plt.clf()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-e', '--encoder', required=True, type=str, help='encoder file path')
    parser.add_argument('-d', '--decoder', required=True, type=str, help='decoder file path')
    parser.add_argument('--fps', required=False, default=24, type=int, help='FPS')
    parser.add_argument('-o', '--output', required=True, help='output folder')
    args = parser.parse_args()

    plot_throughput(args.decoder, args.output, fps=args.fps) 
    plot_encoding_time(args.encoder, args.output, fps=args.fps)
    plot_decoding_time(args.decoder, args.output, fps=args.fps)

if __name__ == '__main__':
    main()