#!/bin/env python3

import sys
import matplotlib.pyplot as plt
from ctypes import cdll
se = cdll.LoadLibrary('./node_pytest.so')

print("init: ", se.se_init())

plot_f1 = []
plot_f2 = []
plot_state = []

filenum = 1

plotskip = 0

while True:
    filename = sys.argv[1] + "/log_%d" % filenum

    filenum += 1

    print("open file: ", filename)

    try:
        f = open(filename, 'rb')
    except FileNotFoundError:
        print("Not found:", filename)
        break

    for line in f:

        try:
            val = int(line, 10)
        except ValueError:
            continue

        # discard garbage
        if val > 0x0fff or val < 0:
            continue

        res = se.se_pushval(val)
        if res != 0:
            print("state event: ", res)

        if se.se_is_frame() != 0:
            plotskip += 1

            if plotskip > 50:
                plotskip = 0
                plot_f1 = plot_f1 + [se.se_current()]
                plot_f2 = plot_f2 + [se.se_avg()]
                plot_state = plot_state + [se.se_currentstate() * 500]


    f.close()

plt.plot(plot_f1, color='#FFE0E0')
plt.plot(plot_f2)
plt.plot(plot_state)
plt.show()
