#!/bin/env python3

import sys
import argparse
import matplotlib.pyplot as plt
import re

from ctypes import cdll

parser = argparse.ArgumentParser(description="Evaluate ADC log files with the state estimation.",
                                 epilog="The state estimation parameters are configured in se_pytester.c.")

parser.add_argument("-i", "--input", help="Input file, the ADC log file of the node.", required=True)
parser.add_argument("--channel", type=int, help="Channel to evaluate", required=True)

args = parser.parse_args()

se = cdll.LoadLibrary('./stateestimation.so')

print("init: ", se.se_init())

plot_f1 = []
plot_f2 = []
plot_state = []

plotskip = 0

# The format of a log line is:
# <NUM> <TYPE>  <CH>=<VAL>; <CH>=<VAL>;
# So we first use a regex to extract the channel array and then split it
# and use another regex to extract the data
line_re = re.compile("^\\d* A (( \\d*=\\d*;)*)$")
elem_re = re.compile("^(\\d*)=(\\d*);$")

cnt = 0

with open(args.input, 'r') as f:
    for line in f:
        l = re.search(line_re, line)

        cnt += 1

        if cnt % 1000 == 0:
            print(cnt)

        # Not ADC values
        if not l:
            continue

        # Split by the spaces seperating the channel data
        vals = l.group(1).split()

        val = None

        for v in vals:
            vm = re.search(elem_re, v)
            if not vm:
                continue

            if int(vm.group(1)) != args.channel:
                continue

            val = int(vm.group(2))
            break

        if val is None:
            import pdb;pdb.set_trace()
            print("No data for channel", args.channel)
            continue

        # discard garbage
        if val > 0x0fff or val < 0:
            print("ADC value out of range:", val)
            continue

        res = se.se_pushval(val)
        if res != 0:
            print("state event: ", res)

        if se.se_is_frame() != 0:
            plotskip += 1

            if plotskip > 5:
                plotskip = 0
                plot_f1 = plot_f1 + [se.se_current()]
                plot_f2 = plot_f2 + [se.se_avg()]
                plot_state = plot_state + [se.se_currentstate() * 500]


    f.close()

plt.plot(plot_f1, color='#FFE0E0')
plt.plot(plot_f2)
plt.plot(plot_state)
plt.show()
