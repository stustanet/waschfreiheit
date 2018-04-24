#!/bin/env python3

import os
from collections import deque
import matplotlib.pyplot as plt

def format_date(curtime):

    d = curtime / (60 * 60 * 24)
    h = (curtime % (60 * 60 * 24)) / (60 * 60)
    m = (curtime % (60 * 60)) / 60
    s = curtime % 60

    return "%d %d:%02d:%02d" % (d, h, m, s)


time_per_line = 0.2 # sec per line in pp file
line_skip = 5       # only use every n-th line/frame (faster processing)

# Puffer für die letzten frame werte (frameavg)
# Für den aktuellen level Wert wird der Durchschnitt aller avgwnd Werte genommen.
# Alle Werte, die ignoriert werden (avg_reject_*) gehen mit 0 in den Durchschnitt ein
avg_wndsize_off = int(30 / (time_per_line * line_skip))   # window size in off state
avg_wndsize = int(300 / (time_per_line * line_skip))       # window size, if NOT off
avgwnd = [0] * avg_wndsize


avg_reject_min = 13 # Werte kleiner als das sind irrelevant

# Erst wenn so viele Werte direkt nacheinander übder avg_reject_min liegen werden diese Werte verwendet
# Das funktioniert sehr gut um Rauschen / Störungen wegzufiltern,
# da insbesondere der Motor immer eine gewisse Zeit läuft. min 3 - 4 sec
avg_reject_conseq = int(3 / (time_per_line * line_skip)) # 3 sec


# State machine stuff

current_state = 0

# states: off(0), end(1), on1(2), on2(3)
# Zeilen:  States
# Spalten: Bedingungen für Zustandsübergänge
#          0   -> Übergang nicht möglich
#          >0  -> Übergang wenn val > Wert
#          <0  -> Übergang wenn val < -Wert
state_transition_values = [
        [  0,   0, +10,   0],
        [ -3,   0,   0, +20],
        [ -6,   0,   0, +20],
        [  0, -10,   0,   0]]

# Spezieller timeout für end-status (~ Kintterschutz nach normalem waschen)
state_end_timer = 0
state_max_end_time = 1900 # 1900s

linecounter = 0
lines_since_ts = 0
state_change_vt = 0

outfile = open('result', 'w')
eventfile = open('event', 'w')

f = open("pp", 'r')

mtime = 0

plotvals1 = []
plotvals2 = []
plotvals3 = []


avg = 0

for line in f:

    if line[0] == '#':
        mtime = int(line[1:])
        outfile.write("# %s\n" % (format_date(mtime)))
        linecounter += 1
        lines_since_ts = 0
        print(format_date(mtime))
        continue

    lines_since_ts += 1

    # frame skip
    if lines_since_ts % line_skip != 0:
        continue

    linecounter += 1

    frameavg = float(line)

    """
    if frameavg < avg_reject_min:
        frameavg = 0

    avg = avg * (0.99) + frameavg * 0.01
    """
    avgwnd = avgwnd + [frameavg]

    #avgsum = 0
    avg_cons_cnt = 0
    #avgsum_tmp = 0

    # Alle werte die verwendet werden
    # (also alle aus mind avg_reject_conseq langen blöcken mit allen werten > avg_reject_min)
    above_thd_vals = []

    # Werte des aktuellen blocks
    above_thd_vals_tmp = []

    for v in avgwnd:
        if v > avg_reject_min:
            # Größer min
            avg_cons_cnt += 1
            # -> in tmp einfügen
            above_thd_vals_tmp += [v];

            if avg_cons_cnt >= avg_reject_conseq:
                # wenn genug über min -> tmp an liste anhängen
                above_thd_vals += above_thd_vals_tmp
                # und tmp leeren
                above_thd_vals_tmp = []
        else:
            # kleiner als min
            # -> reset counter + tmp leeren
            above_thd_vals_tmp = []
            avg_cons_cnt = 0

    avg = 0
    median = 0

    #if (len(above_thd_vals) > 0):
        # Median der werte berechen
        # -> Sortieren
        #above_thd_vals = sorted(above_thd_vals)
        # -> Median ist der mittlere Wert
        #median = above_thd_vals[int(len(above_thd_vals) / 2)]

        # jetzt noch anhand der Anzahl der verwendeten Werte skalieren
        #avg = (median * len(above_thd_vals)) / len(avgwnd)

    avg = sum(above_thd_vals) / len(avgwnd)

    # Store frame + avg for plotting
    plotvals1.append(frameavg)
    plotvals2.append(avg)

    old_state = current_state

    # Aktive Zeile der Transition Matrix
    stv = state_transition_values[current_state]

    # Transitions prüfen
    for i in range(0, len(stv)):
        if stv[i] > 0 and avg > stv[i]:
            current_state = i
            break
        elif stv[i] < 0 and avg < -stv[i]:
            current_state = i
            break

    # End-State timer
    if current_state == 1:
        state_end_timer += 1
        if state_end_timer > state_max_end_time:
            current_state = 2 # avoid getting stuck
    else:
        state_end_timer = 0
    # Window size anpassen
    if current_state == 0 and len(avgwnd) > avg_wndsize_off:
        # reduce window size
        avgwnd = avgwnd[(len(avgwnd) - avg_wndsize_off) :]

    if current_state != 0 and len(avgwnd) > avg_wndsize:
        # reduce window size
        avgwnd = avgwnd[(len(avgwnd) - avg_wndsize) :]

    # on1 und on2 sind on
    is_on = (current_state >= 2)

    if is_on:
        ind = '\t+++++'
    else:
        ind = ''

    # plot current state
    plotvals3.append(current_state * 10)

    curtime = lines_since_ts * time_per_line + mtime

    if old_state != current_state:
        state_duration = curtime - state_change_vt

        mark = ' '
        if is_on:
            mark = 'X'

        eventfile.write("%s\t%d -> %d\t%s %d\t%d\n" % (format_date(curtime), old_state, current_state, mark, state_duration, linecounter ))
        state_change_vt = curtime

    outfile.write("%f\t%f\t%s %s\n" % (frameavg, avg, current_state, ind))


plt.plot(plotvals1, color='#FFE0E0')
plt.plot(plotvals2)
plt.plot(plotvals3)
plt.show()
