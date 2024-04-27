#!/usr/bin/env python3

import sys
import glob
import math
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

plucks = []

for line in sys.stdin:
    plucks.append([int(x) for x in line.strip().split(" ")])

if True:
    ncols = 6
    nrows = nrows = math.ceil(len(plucks) / ncols)
    fig, _ = plt.subplots(constrained_layout=True,
                          sharex=True,
                          sharey=True,
                          figsize=(2*ncols, 2*nrows),
                          nrows=nrows,
                          ncols=ncols)

    avgs = [0 for _ in plucks[0]]
    for pin_plucks in plucks[:-2]:
        for i, v in enumerate(pin_plucks):
            avgs[i] += v
    avgs = [v / len(plucks[:-2]) for v in avgs]        
    
    for ax, ys in zip(fig.axes, plucks):
        xs = list(range(len(ys)))
        ax.plot(xs, ys, label="val")
        #ax.plot(xs, avgs, label="avg")
        #ax.plot(xs, [y-a for (y,a) in zip(ys,avgs)], label="delta")
        #ax.set_ylim([-15, 15])
        #ax.set_xlim([0, 200])
        #ax.legend()
        
    fig.savefig("stdin-plucks.png", dpi=180)
    plt.clf()
