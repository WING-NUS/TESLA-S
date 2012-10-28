#!/usr/bin/env python
from sys import stdin

s = 0.0
n = 0

for line in stdin:
    s += float(line)
    n += 1

print s / n
