#!/bin/env python3

import os
import sys

"""
Stage1 filter
Rohdaten -> Frames
"""

# next file number
filenum = 1
# ctime der ersten datei (für timestamps)
firstfiletime = 0


# Zum filtern der rohdaten
# filterval = filterval * (1 - filterweight) + current * filterweigth
filterweight = 0.02
filterval = 0

# der aktuelle mittelwert
# 2105 ist ein initialwert, der recht genau dem zielwert für die testdaten entspricht
mid = 2105

# Anzahl der Werte pro Frame
framesize = 100
framecounter = 0


# Nur für den test (wurde um 12:30 gestartet -> 45000 sec offset)
time_offset = 41280

outfile = open('pp', 'w')

while True:
    filename = sys.argv[1] + "/log_%d" % filenum
    filenum += 1

    print("open file: ", filename)

    try:
        f = open(filename, 'rb')
    except FileNotFoundError:
        print("Not found:", filename)
        break

    mtime = os.path.getmtime(filename)
    if firstfiletime == 0:
        firstfiletime = mtime

    mtime -= firstfiletime
    mtime += time_offset

    outfile.write("#%u\n" % mtime)

    for line in f:

        try:
            val = int(line, 10)
        except ValueError:
            continue

        # discard garbage
        if val > 0x0fff or val < 0:
            continue

        if val > mid:
            mid += 0.0001
        else:
            mid -= 0.0001

        # now use mid to get abs values
        absval = abs(val - mid)

        filterval = filterval * (1.0 - filterweight) + absval * filterweight

        framecounter += 1

        if framecounter >= framesize:
            outfile.write("%f\n" % (filterval))
            framecounter = 0


    f.close()
    print("mid is now:", mid)
