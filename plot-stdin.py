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
    ncols = 1
    nrows = nrows = math.ceil(len(plucks) / ncols)
    fig, _ = plt.subplots(constrained_layout=True,
                          sharex=True,
                          sharey=True,
                          figsize=(12*ncols, 2*nrows),
                          nrows=nrows,
                          ncols=ncols)

    for ax, ys in zip(fig.axes, plucks):
        ys = ys[-2048:]
        xs = list(range(len(ys)))
        ax.plot(xs, ys, label="val")
        
    fig.savefig("stdin-plucks.png", dpi=300)
    plt.clf()
