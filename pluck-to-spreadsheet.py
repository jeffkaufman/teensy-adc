#!/usr/bin/env python3

import sys

fname, = sys.argv[1:]

header = ["sample"]
rows = []
with open(fname) as inf:
    for sample_no, line in enumerate(inf):
        header.append("s_%s" % sample_no)
        line = line.rstrip("\n")
        for i, val in enumerate(line.split()):
            if sample_no == 0:
                rows.append([i])
            rows[i].append(val)

print(*header, sep="\t")
for row in rows:
  print(*row, sep="\t")
    

