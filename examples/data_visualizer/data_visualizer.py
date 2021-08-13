""" SPDX-License-Identifier: Apache-2.0 """
import numpy

"""
Copyright 2021 XIA LLC, All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

""" data_visualizer.py

"""

from argparse import ArgumentParser
from io import BytesIO
import logging
import math
import multiprocessing as mp
import sys

import dolosse.constants.data
from dolosse.hardware.xia.pixie16.list_mode_data_mask import ListModeDataMask
from dolosse.hardware.xia.pixie16.list_mode_data_decoder import decode_listmode_data
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

logging.basicConfig(stream=sys.stdout, datefmt="%Y-%m-%dT%H:%M:%S.000Z", level=logging.INFO,
                    format='%(asctime)s - %(levelname)s - %(message)s')


def process_data_buffer(args):
    buffer = args[0]
    mask = args[1]
    return decode_listmode_data(buffer, mask)[0]


def process_list_mode_data_file(args):
    data_mask = ListModeDataMask(args.freq, args.rev)
    data_buffer_list = list()
    logging.info("Started reading data buffers into memory.")
    with open(args.file, 'rb') as f:
        while True:
            chunk = f.read(dolosse.constants.data.WORD * 4)
            if chunk:
                data_buffer_list.append([BytesIO(chunk), data_mask])
            else:
                break
    logging.info(f"Read {len(data_buffer_list)} data buffers from file.")

    logging.info("Sending %s DATA buffers for decoding." % len(data_buffer_list))
    logging.info("Aggregating triggers into single list.")
    results = list()
    for result in mp.Pool().imap(process_data_buffer, data_buffer_list):
        results.append(result)
    return pd.DataFrame(results)


def plot_csv(plot_type, data, args):
    title = "Untitled"
    xlabel = "Sample"
    ylabel = "ADC (arb) / Sample"

    if plot_type == 'mca':
        xlabel = "Bin"
        ylabel = "Energy (arb) / Bin"
        title = "MCA Spectrum"
    if plot_type == 'trc':
        title = "ADC Traces"
    if plot_type == 'baseline':
        title = "Baselines"

    if args.chan is not None:
        data[f'Chan{args.chan}'].plot(title=title, xlabel=xlabel, ylabel=ylabel)
        plt.legend()
    else:

        data.plot(subplots=True, layout=(4, 4), title=title, xlabel=xlabel, ylabel=ylabel,
                  figsize=(11, 8.5), sharey=True)
        plt.tight_layout()

    if args.xlim:
        plt.xlim(args.xlim)
        if any(val > 1000 for val in args.xlim):
            plt.subplots_adjust(wspace=0.2)
    plt.show()


if __name__ == '__main__':
    PARSER = ArgumentParser(description='Optional app description')
    PARSER.add_argument('-b', '--baseline', action='store_true', help='Plots MCA spectra')
    PARSER.add_argument('-c', '--chan', type=int, dest='chan',
                        help="The channel that you'd like to plot.")
    PARSER.add_argument('-f', '--file', type=str, dest="file", required=True,
                        help="The file containing the data to read.")
    PARSER.add_argument('--freq', type=int, dest='freq',
                        help="The sampling frequency used to collect list-mode data. Ex. 250")
    PARSER.add_argument('-l', '--lmd', action='store_true',
                        help="Tells the program that the file is list-mode data.")
    PARSER.add_argument('-m', '--mca', action='store_true', help='Plots MCA spectra')
    PARSER.add_argument('-x', '--xlim', dest='xlim',
                        type=lambda r: [int(x) for x in r.split(",")],
                        help="Comma separated range for X-axis limits. Ex. 10,400")
    PARSER.add_argument('--rev', type=int, dest='rev',
                        help="The firmware used to collect list-mode data. Ex. 30474")
    PARSER.add_argument('-t', '--trace', action='store_true', help='Plots traces')
    ARGS = PARSER.parse_args()

    if ARGS.xlim:
        if len(ARGS.xlim) != 2:
            PARSER.error("The xlim flag expects two and ONLY two values.")
        if ARGS.xlim[1] < ARGS.xlim[0]:
            ARGS.xlim.sort()

    if ARGS.lmd:
        if not ARGS.freq or not ARGS.rev:
            PARSER.error("When requesting list-mode data you must provide us with the "
                         "hardware's sampling frequency and firmware revision.")

        df = process_list_mode_data_file(ARGS)
        channels = df.channel.unique()
        channels.sort()

        if len(channels) < 4:
            fig, axes = plt.subplots(nrows=len(channels), ncols=1, sharex='all',
                                     sharey='all')
        else:
            subplot_dim = int(math.sqrt(math.pow(math.ceil(math.sqrt(len(channels))), 2)))
            channels = np.pad(channels, (0, subplot_dim ** 2 - len(channels)), mode='constant',
                              constant_values=None)
            fig, axes = plt.subplots(nrows=subplot_dim, ncols=subplot_dim, sharex='col',
                                     sharey='row')

        for channel, ax in np.column_stack((channels, fig.axes)):
            if numpy.isnan(channel):
                continue
            df[df['channel'] == channel].hist(column='energy', ax=ax, grid=False)
            ax.title.set_text(f'Chan {int(channel)}')

        fig.supxlabel("Energy(arb)")
        fig.supylabel("Energy(arb) / bin")
        plt.xlim(ARGS.xlim)
        plt.suptitle("List-Mode Energy Histograms")
        plt.tight_layout()
        plt.show()
    else:
        df = pd.read_csv(ARGS.file, index_col='bin', keep_default_na=False)

    if ARGS.trace:
        plot_csv('trc', df, ARGS)
    if ARGS.mca:
        plot_csv('mca', df, ARGS)
    if ARGS.baseline:
        df.pop("timestamp")
        plot_csv('baseline', df, ARGS)
