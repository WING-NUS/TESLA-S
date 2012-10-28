#!/usr/bin/env python
import sys

lines = []


def print_lines():
    global lines
    if lines:
        print ' '.join(lines)
        lines = []
    else:
        print


for line in sys.stdin:
    line = line.strip()
    if line == '':
        pass
    elif line.startswith('TOPICSEPARATOR'):
        print_lines()
    else:
        lines.append(line)

assert len(lines) == 0
