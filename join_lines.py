#!/usr/bin/env python
import sys

lines = []
for line in sys.stdin:
    line = line.strip()
    if line != '':
        lines.append(line)

print ' '.join(lines)
