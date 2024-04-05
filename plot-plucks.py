#!/usr/bin/env python3

import glob
import math
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

plucks = {
    False: [],
    True: [],
}

def import_data(fname, is_up):
    with open(fname) as inf:
        for line in inf:
            plucks[is_up].append([int(x) for x in line.strip().split(" ")])

for fname in glob.glob("plucks/up*.txt"):
    import_data(fname, True)

for fname in glob.glob("plucks/down*.txt"):
    import_data(fname, False)

def classify(ys):
    #return peak_only(ys)
    return broad_if_possible_else_peak(ys)

def broad_if_possible_else_peak(ys):
    broad_len = 0
    broad_dir = 0

    max_broad_len = 0
    max_broad_dir = 0
    
    for v in ys:
        if v > 16:
            if broad_dir > 0:
                broad_len += 1
            else:
                broad_len = 0
                broad_dir = 1

        elif v < -16:
            if broad_dir < 0:
                broad_len += 1
            else:
                broad_len = 0
                broad_dir = -1

        if broad_len > max_broad_len:
            max_broad_len = broad_len
            max_broad_dir = broad_dir

    if max_broad_len > 400:
        return "up" if max_broad_dir > 0 else "down"

    return peak_only(ys)

def peak_only(ys):
    """
    Classify by the direction of the peak.  Works prett well, but makes
    mistakes when downstrokes have a sharp rebound peak and upstrokes have a
    small sharp peak.
    """
    
    if max(ys) > -min(ys) and max(ys) > 16:
        return "down"
    elif max(ys) < -min(ys) and -min(ys) > 16:
        return "up"
    return "ND"
    
for is_up in [True, False]:
    data = plucks[is_up]
    ncols = 6
    nrows = nrows = math.ceil(len(data) / ncols)
    fig, _ = plt.subplots(constrained_layout=True,
                          sharex=True,
                          sharey=True,
                          figsize=(1*ncols, 1*nrows),
                          nrows=nrows,
                          ncols=ncols)
    scored_data = [
        #(sum(y*y for y in ys), ys)
        (max(max(ys), -min(ys)), ys)
        for ys in data]
    scored_data.sort()
    
    for ax, (score, ys) in zip(fig.axes, scored_data):
        label = classify(ys)
        xs = list(range(len(ys)))
        ax.plot(xs, ys)
        ax.set_title(label)

    fig.suptitle("%sstrokes" % (
        "Up" if is_up else "Down"))

    fig.savefig("%s-plucks.png" % ("up" if is_up else "down"), dpi=180)
    plt.clf()

        
