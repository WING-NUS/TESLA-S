TESLA-S
=======

TESLA-S: Evaluating Summary Content

Author: Liu Chang <liuchangjohn@gmail.com>

Usage: tesla-s submission reference-0 reference-1 .. > score

Outputs a single score for the given system. The submission and references are
in the same format:

line 1 for topic 1
line 2 for topic 1
..
TOPICSEPARATOR
line 1 for topic 2
..
TOPICSEPARATOR
line 1 for last topic
..
TOPICSEPARATOR

Notes:

- Do NOT join each entry into a single line; this messes up POS tagging.

- It is assumed that each file has the same number of topics. If a reference
  lacks some topics, simply put another TOPICSEPARATOR.

- Sometimes the input has no EOL at the end of file. For robustness, it's
  recommended to add a new line for every file.

A sample of the submission and reference file generation is as follows:

# submission
for TOPIC in `cat topics`; do
    cat submission-for-$TOPIC >> submission
    echo >> submission
    echo TOPICSEPARATOR >> submission
done

# a reference
for TOPIC in `cat topics`; do
    if [ -f reference-for-$TOPIC ]; then
        cat reference-for-$TOPIC >> reference
        echo >> reference
        echo TOPICSEPARATOR >> reference
    else
        echo TOPICSEPARATOR >> reference
    fi
done
