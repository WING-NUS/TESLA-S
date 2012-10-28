#!/usr/bin/env python
import sys
import re
from util import *


## 
## reformat POS tagged output from different taggers
## output format: word1/POS1 word2/POS2 ... one sentence per line
##


if len(sys.argv) != 2:
    print "usage: pos_reformat.py [opennlp|treetagger|freeling|tnt|morce] < input > output"
    sys.exit(-1)

if sys.argv[1] in "opennlp treetagger freeling tnt morce".split():
    tagger = sys.argv[1]
else:
    print "unknown tagger: ", sys.argv[1]
    sys.exit(-1)

# POS separator
sep = "_"


if tagger == 'opennlp':
    while True:
        line = sys.stdin.readline()
        if line == '':
            break
        line = line.decode("utf8", "replace")
        for token in line.split():
            sep_index = token.rfind("/")
            if sep_index >=0:
                token = token[:sep_index]+sep+token[sep_index+1:]
                print token.encode("utf8", "replace"),
            else:
                print "Warning: no word/pos separator found: %s" % token.encode("utf8", "replace")
        print ""
elif tagger == 'morce':
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
            except:
                print "Warning: no word or pos found: %s" % line.encode("utf8", "replace")
            print "%s%s%s" % (word.encode("utf8"), sep, pos.encode("utf8")),
        print ""
else:
    for paragraph in paragraphs(sys.stdin, separator=lambda x:x.strip()=='', includeEmpty=False):
        for line in paragraph.split("\n"):
            line = line.decode("utf8", "replace")
            tokens = line.split()
            if tokens == []:
                continue
            try:
                if tagger == 'treetagger':
                    word, pos  = tokens[:2]
                elif tagger == "freeling":
                    word = tokens[0]
                    pos = tokens[2]
                elif tagger == "tnt":
                    word, pos  = tokens[:2]
                print "%s%s%s" % (word.encode("utf8"), sep, pos.encode("utf8")),
            except:
                msg = "Error: cannot format pos. line was %s" % line.encode("utf8", "replace")
                raise Exception(msg)
        print""
