#!/bin/bash

#
# do language specific lemmatization
#
 
L=$1
IN=$2
OUT=$3

DIR=`dirname $0`
MORPH=${DIR}/morphword/

${MORPH}/morphword ${MORPH} ${IN} | $DIR/lemma_format.py morph > $OUT
