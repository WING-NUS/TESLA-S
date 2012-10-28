#!/usr/bin/env python
import sys
import re
from util import *


## 
## reformat lemmatizer output from different lemmatizers
## output format: lemma1 lemma2 ... one sentence per line
##


if len(sys.argv) != 2:
    print "usage: lemma_reformat.py [treetagger|freeling|morce|morph]  < input > output"
    sys.exit(-1)

if sys.argv[1] in  "treetagger freeling morce morph".split():
    tagger = sys.argv[1]
else:
    print "Error: unknown lemmatizer: ", sys.argv[1]
    sys.exit(-1)

# POS/lemma separator
sep = "_"


if tagger == 'morce':
    for paragraph in paragraphs(sys.stdin, separator=lambda x:x.strip()=="<s>", includeEmpty=False):
        if paragraph.startswith("<csts>"):
            continue
        for line in paragraph.split("\n"):
            line = line.decode("utf8", "replace")
            if not line.startswith("<f") or line.startswith("<d"):
                continue
            try:
                re_word = re.search(">.*?<", line)
                word = re_word.group(0)[1:-1]
                re_pos = re.search("MDt.*?>.", line)
                pos = re_pos.group(0)[-1]
                re_lem = re.search("(<MDl.*?>)([^<]*)(<)", line)
                lem = re_lem.group(2)
                if lem.find("-") > 0:
                    lem = lem[:lem.find("-")]
                if lem.find("_") > 0:
                    lem = lem[:lem.find("_")]
            except:
                print "Warning: no lemma found: %s" % line.encode("utf8", "replace")
            # lowercase lemma
            lem = lem.lower()
            print "%s%s%s%s%s" % (word.encode("utf8", "replace"), sep, pos.encode("utf8", "replace"), sep, lem.encode("utf8")),
        print""
elif tagger == 'morph':
    while True:
        line = sys.stdin.readline()
        if line == '':
            break

        line = line.decode("utf8", "replace")
        tokens = line.split()
        if tokens == []:
            continue
        for token in tokens:
            sep1 = token.find('_')
            sep2 = token.find('_', sep1+1)
            if sep2 < 0:
                lem = token[:sep1].lower()
                print "%s%s%s" % (token.encode("utf8", "replace"), sep, lem.encode("utf8", "replace")),
            else:
                word_pos= token[:sep2]
                lem = token[sep2+1:].lower()
                print "%s%s%s" % (word_pos.encode("utf8", "replace"), sep, lem.encode("utf8", "replace")),
        print ""
        sys.stdout.flush()
else:
    for paragraph in paragraphs(sys.stdin, includeEmpty=False):
        for line in paragraph.split("\n"):
            line = line.decode("utf8", "replace")
            tokens = line.split()
            if len(tokens) < 3:   # skip 'other' stuff like xml markup
                continue
            if tagger == 'treetagger':
                word, pos, lem  = tokens
                if lem == "<unknown>":
                    lem = word
            else:   # tagger == "freeling"
                word, lem, pos = tokens[:3]
            # lowercase lemma
            lem = lem.lower()
            print "%s%s%s%s%s" % (word.encode("utf8", "replace"), sep, pos.encode("utf8", "replace"), sep, lem.encode("utf8")),
        print""
