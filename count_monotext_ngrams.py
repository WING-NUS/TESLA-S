#!/usr/bin/env python
from itertools import product
from sys import stdin, stderr, exit
from optparse import OptionParser

parser = OptionParser()
parser.add_option("--ngrams", dest="ngrams", choices=['1', '2', '3', '4', '123', '1234', 'su3', 'su4', 'su5'],
                  help="What ngrams to extract.")

options, args = parser.parse_args()

lengths = []
skip_bigram_window = None

if options.ngrams is None:
    lengths = [1,2,3,4] # default
elif options.ngrams == '1':
    lengths = [1,]
elif options.ngrams == '2':
    lengths = [2,]
elif options.ngrams == '3':
    lengths = [3,]
elif options.ngrams == '4':
    lengths = [4,]
elif options.ngrams == '123':
    lengths = [1,2,3,]
elif options.ngrams == '1234':
    lengths = [1,2,3,4,]
elif options.ngrams == 'su3':
    lengths = [1,]
    skip_bigram_window = 4
elif options.ngrams == 'su4':
    lengths = [1,]
    skip_bigram_window = 4
elif options.ngrams == 'su5':
    lengths = [1,]
    skip_bigram_window = 5
else:
    assert False


while True:
    line = stdin.readline()
    if line == '':
        break

    if line.strip() == '':
        print 
        continue

    counts = {}

    # normal n-grams
    words = line.split()
    for start, length in product(range(len(words)), lengths):
        end = start + length
        if end > len(words):
            continue
        ngram = ' '.join(words[start:end])

        try:
            counts[ngram] += 1.0
        except KeyError:
            counts[ngram] = 1.0

    # skip bi-grams
    if skip_bigram_window is not None:
        # min offset is 1, e.g. 1 2
        # max offset is skip_bigram_window-1, e.g. when start is 1 and skip_bigram_window is 4,
        # the max skip bigram is 1 4, where the offset is 4 - 1.
        for start, offset in product(range(len(words)), range(1, skip_bigram_window)):
            end = start + offset
            if end >= len(words):
                continue
            ngram = words[start] + ' ' + words[end]

            try:
                counts[ngram] += 1.0
            except KeyError:
                counts[ngram] = 1.0
            
    # the output
    output = []
    for ngram in counts:
        output.append('%s %f' % (ngram, counts[ngram]))

    print ' ||| '.join(output)
