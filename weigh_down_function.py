#!/usr/bin/env python
import sys
from getopt import getopt

# content words:
# NN, VB, JJ, RB and their derivatives
# function words:
# everything else. Weight x0.5.

opts, args = getopt(sys.argv[1:], 'd:p:')
assert len(args) == 0

discount = 0.5
pos_position = 1
content_pos_en = "NN VB JJ RB".split()
content_pos_de = "AD NN NE PT VV VA VM".split()
content_pos_fr = "AB AD NA NO VE".split()
content_pos_es = "AQ AO RG RN NC NP VM VA VS".split()
content_pos_cz = "N V X A D".split()
content_pos_cn = "VA VC VE VV NR NT NN AD".split()
content_pos = content_pos_en + \
    content_pos_de + \
    content_pos_fr + \
    content_pos_es + \
    content_pos_cz + \
    content_pos_cn

for opt, val in opts:
    if opt == '-d':
        discount = float(val)
        print >> sys.stderr, 'Weight discount:', discount
    elif opt == '-p':
        pos_position = int(val)
        print >> sys.stderr, 'POS is found as field:', pos_position
    else:
        assert False, (opt, val)

while True:
    line = sys.stdin.readline()
    if line == '':
        break

    line = line.strip()
    if line == '':
        print line
        continue

    ngrams = line.split(' ||| ')

    for i, ngram in enumerate(ngrams):
        toks = ngram.split()
        words = toks[:-1]
        weight = float(toks[-1])

        for word in words:
            pos = word.split('_')[pos_position]
            if pos[:2] not in content_pos:
                weight *= discount

        words.append(str(weight))
        ngrams[i] = ' '.join(words)

    print ' ||| '.join(ngrams)
