#!/usr/bin/env python
# -*- coding: utf-8 -*-
import sys
from pprint import pprint
from nltk.corpus import wordnet as wn
from itertools import product
from cvxopt import matrix, spmatrix, solvers
from getopt import gnu_getopt as getopt

solvers.options['show_progress'] = False

optlist, args = getopt(sys.argv, '', [
        'verbose', 'streaming', 'refcount=', 'lineids=', 'simmethod=', 'separator=', 'format=', 'dict=', 'cilin='])

_, reference, paraphrase, outputfile = args
verbose = False
simmethod = 'default'
separator = '/'
format = 'word_pos_stem'
synonym_dictionary = None

# the following options are only used in streaming
streaming = False
refcount = 1            # number of references per sentence
lineidsf = None         # where do we read the line IDs? They come in 
                        # a different file from the paraphrases, as the
                        # paraphrases need to be preprocessed

for (opt, val) in optlist:
    if opt == '--verbose':
        verbose = True
    elif opt == '--streaming':
        streaming = True
    elif opt == '--refcount':
        refcount = int(val)
    elif opt == '--lineids':
        lineidsf = open(val)
        print >> sys.stderr, "Reading line IDs from", val
    elif opt == '--simmethod':
        simmethod = val             
        # values:
        # maxsim: l1 == l2, or syn(l1) intersect syn(l2) != {} and pos1 == pos2
        # surface: l1 == l2
        # surface_pos: l1 == l2 return 1; pos1 == pos2 return 0.5
        # surface_syn: l1 == l2 return 1; syn(l1) intersect syn(l2) != {} return 0.5
    elif opt == '--separator':
        assert len(val) == 1
        separator = val
    elif opt == '--format':
        assert val in ['word_pos_stem', 'word', 'pos_stem', 'stem_pos']
        format = val
    elif opt == '--dict':
        dict_file = open(val)
        synonym_dictionary={}
        for line in dict_file:
            line = line.lower()
            tokens = line.split()
            if len(tokens) < 1:
                continue
            synonym_dictionary[tokens[0]] = tokens[1:]
        dict_file.close()
    elif opt == '--cilin':
        dict_file = open(val)
        synonym_dictionary={}
        for line in dict_file:
            # each line is of the form
            # Di15A04= 名位 名分 排名分
            # the dictionary is thus,
            #   '名位' => ['Di15A04'],
            #   '名分' => ['Di15A04']
            # etc. Later, the synonyms_of would return all
            # tags that the word falls under. Then the sets
            # are compared using disjoint(), the effect is
            # two words are judged synonyms if they ever
            # appear on the same line.
            tokens = line.split()
            for w in tokens[1:]:
                try:
                    synonym_dictionary[w].append(tokens[0])
                except KeyError:
                    synonym_dictionary[w] = [tokens[0],]
    else:
        raise AssertionError(opt, val)

outputfile = open(outputfile, 'w')

# Returns e.g.
# [
#   [('a', 1), ('b', 4), ('c', 2)],             # monograms
#   [('a b', 1), ('b c', 2), ('c a', 1)],       # bigrams
#   [('a b c', 1), ('b c d', 1)],               # trigram
#   [],                                         # four-grams
# ]
def parse_ngrams_line(line):
    output = [[] for i in range(4)]
    line = line.strip()

    if line == '':
        return output

    for n_gram_and_count in line.split(' ||| '):
        tokens = n_gram_and_count.split()
        output[len(tokens)-2].append((tokens[:-1], float(tokens[-1])))
    return output


def ngram_similarity(ngram1, ngram2):
    assert len(ngram1) == len(ngram2)
    ss = []
    for lemma_pos1, lemma_pos2 in zip(ngram1, ngram2):
        sss = lemma_pos_similarity(lemma_pos1, lemma_pos2)
        # if any gram doesn't match at all, the ngrams don't match
        if sss < 0.001:
            return 0.0
        ss.append(sss)
    sim = sum(ss) / len(ss)
    return sim


lemma_pos_similarity_buffer = {}
def lemma_pos_similarity(lemma_pos1, lemma_pos2):
    global lemma_pos_similarity_buffer

    # a test shows about 2m entries in the dictionary uses about 1GB of memory.
    # And 2m entries ought to be enough for anybody :)
    if len(lemma_pos_similarity_buffer) > 2000000:
        lemma_pos_similarity_buffer = {}

    if lemma_pos1 == 'NULL' and lemma_pos2 == 'NULL':
        return 1.0
    elif lemma_pos1 == 'NULL' or lemma_pos2 == 'NULL':
        return 0.0

    if lemma_pos1 < lemma_pos2:
        lemma_pos1, lemma_pos2 = lemma_pos2, lemma_pos1
    try:
        return lemma_pos_similarity_buffer[(lemma_pos1, lemma_pos2)]
    except KeyError:
        sim = do_lemma_pos_similarity(lemma_pos1, lemma_pos2)
        if verbose:
            print 'lemma_pos_similarity:', \
                    '[lemma_pos1]', lemma_pos1, \
                    '[lemma_pos2]', lemma_pos2, \
                    '[similarity]', sim

        lemma_pos_similarity_buffer[(lemma_pos1, lemma_pos2)] = sim
        return sim


def do_split(lemma_pos):
    if format == 'word_pos':
        lemma, pos = lemma_pos.rsplit(separator,1)
        return lemma, pos

    elif format == 'pos_stem':
        parts = lemma_pos.split(separator,1)
        if len(parts) == 2:
            # pos, lemma.
            return parts[1], parts[0], parts[1]
        elif len(parts) == 1:
            # lemma only. POS failed.
            return parts[0], 'UNKNOWN', parts[0]

    elif format == 'stem_pos':
        lemma, pos = lemma_pos.split(separator,1)
        return lemma, pos, lemma

    elif format == 'word_pos_stem':
        s = lemma_pos.rsplit(separator,2)
        if len(s) == 2:
            # stem missing; word is the stem too
            return s[0], s[1], s[0]
        return s

    else:   
        raise AssertionError, format


def synonyms_of(word):
    ret = set()
    if synonym_dictionary:
        # use dictionary file
        try:
            ret = set(synonym_dictionary[word])
        except KeyError:
            pass
    else:
        # use wordnet
        ret = set()
        synsets = wn.synsets(word)
        for synset in synsets:
            for lemma in synset.lemmas:
                ret.add(lemma.name)
    return ret


def do_lemma_pos_similarity(lemma_pos1, lemma_pos2):
    if lemma_pos1 == lemma_pos2:
        return 1.0

    try:
        if format == 'word_pos':
            lemma1, pos1 = do_split(lemma_pos1)
            lemma2, pos2 = do_split(lemma_pos2)
        elif format == 'word_pos_stem' or format == 'pos_stem' or format == 'stem_pos':
            lemma1, pos1, stem1 = do_split(lemma_pos1)
            lemma2, pos2, stem2 = do_split(lemma_pos2)
        elif format == 'word':
            lemma1 = lemma_pos1
            lemma2 = lemma_pos2
        else:
            raise AssertionError(format)

    except ValueError:
        raise ValueError(lemma_pos1, lemma_pos2)

    if simmethod == 'maxsim':
        if lemma1 == lemma2:
            return 1.0
        elif format == 'word_pos_stem' and stem1 == stem2:
            return 1.0
        else:
            synonyms1 = synonyms_of(stem1)
            synonyms2 = synonyms_of(stem2)

            sim1 = 1.0 if pos1 == pos2 else 0.0
            sim2 = 0.0 if synonyms1.isdisjoint(synonyms2) else 1.0
            sim = (sim1 + sim2) / 2
            return sim

    elif simmethod == 'synonym':
        if lemma1 == lemma2:
            return 1.0
        elif format == 'word_pos_stem' and stem1 == stem2:
            return 1.0

        synonyms1 = synonyms_of(stem1)
        synonyms2 = synonyms_of(stem2)

        if synonyms1.isdisjoint(synonyms2):
            return 0.0
        else:
            return 1.0

    elif simmethod == 'partial_surface':
        if lemma1 == lemma2:
            return 1.0
        else:
            return 0.0

    elif simmethod == 'stem':
        if lemma1 == lemma2 or stem1 == stem2:
            return 1.0
        else:
            return 0.0

    elif simmethod == 'surface_pos':
        if lemma1 == lemma2:
            return 1.0
        elif pos1 == pos2:
            return 0.5
        else:
            return 0.0

    elif simmethod == 'surface_syn':
        if lemma1 == lemma2:
            return 1.0
        else:
            synonyms1 = synonyms_of(lemma1)   # use lemma instead of stem
            synonyms2 = synonyms_of(lemma2)
            sim = 0.0 if synonyms1.isdisjoint(synonyms2) else 0.5
            return sim

    elif simmethod == 'exact':
        assert format == 'word_pos_stem'
        return 1.0 if lemma_pos1.split()[0] == lemma_pos2.split()[0] else 0.0

    raise AssertionError(simmethod)


def surface_match(ref_ngrams_with_weights, para_ngrams_with_weights):
    # optimization for match in the case of surfaces
    weight_ref = sum([rw[1] for rw in ref_ngrams_with_weights])
    weight_para = sum([rw[1] for rw in para_ngrams_with_weights])

    # only support word format for now, so it's a simple concat
    assert format == 'word'

    count = {}
    # count[ngram] = [times in ref, times in para]

    for ref_ngram, ref_weight in ref_ngrams_with_weights:
        ngram = ' '.join(ref_ngram)
        try:
            count[ngram][0] += ref_weight
        except KeyError:
            count[ngram] = [ref_weight, 0.0]

    for para_ngram, para_weight in para_ngrams_with_weights:
        ngram = ' '.join(para_ngram)
        try:
            count[ngram][1] += para_weight
        except KeyError:
            count[ngram] = [0.0, para_weight]

    match_weight = 0.0
    for ngram in count:
        times_ref, times_para = count[ngram]
        match_weight += min(times_para, times_ref)

    return match_weight, weight_ref, weight_para


def match(ref_ngrams_with_weights, para_ngrams_with_weights):
    weight_ref = sum([rw[1] for rw in ref_ngrams_with_weights])
    weight_para = sum([rw[1] for rw in para_ngrams_with_weights])

    # Ax <= b; minimize c.x
    # A is a sparse matrix, represented by three vectors a, I, J
    a, I, J, b, c = [], [], [], [], []

    num_links = 0
    links_for_ref = [set() for i in range(len(ref_ngrams_with_weights))]
    links_for_para = [set() for i in range(len(para_ngrams_with_weights))]
    links = []

    # build c
    for i, (ref_ngram, ref_weight) in enumerate(ref_ngrams_with_weights):
        for j, (para_ngram, para_weight) in enumerate(para_ngrams_with_weights):
            sim = ngram_similarity(ref_ngram, para_ngram)

            if sim > 0.01:
                # new link
                links_for_ref[i].add(num_links)
                links_for_para[j].add(num_links)
                num_links += 1
                links.append((ref_ngram, ref_weight, para_ngram, para_weight))

                # we'd like to maximize, but the solver minimizes, so invert c
                c.append(-1.0 * ngram_similarity(ref_ngram, para_ngram))

    if num_links == 0:
        # no links at all
        return 0.0, weight_ref, weight_para

    num_conditions = 0

    # condition: all link weights are non-negative
    for i in range(num_links):
        # x[i] >= 0
        # i.e. -1.0 * x[i] <= 0
        # i.e. A[num_conditions, i] = -1.0, b[num_conditions] = 0.0
        a.append(-1.0)
        I.append(num_conditions)
        J.append(i)
        b.append(0.0)
        num_conditions += 1

    # condition: sum of all links for a para_ngram is capped by its weight
    for i, (para_ngram, para_weight) in enumerate(para_ngrams_with_weights):
        # sum x[links with i] <= para_weight
        # i.e. A[num_conditions, link number with i] = 1.0, b[num_conditions] = para_weight
        if len(links_for_para[i]) != 0:
            for j in links_for_para[i]:
                a.append(1.0)
                I.append(num_conditions)
                J.append(j)
            b.append(para_weight)
            num_conditions += 1

    # condition: sum of all links for a ref_ngram is capped by its weight
    for i, (ref_ngram, ref_weight) in enumerate(ref_ngrams_with_weights):
        # sum x[links with i] <= ref_weight
        # i.e. A[num_conditions, link number with i] = 1.0, b[num_conditions] = ref_weight
        if len(links_for_ref[i]) != 0:
            for j in links_for_ref[i]:
                a.append(1.0)
                I.append(num_conditions)
                J.append(j)
            b.append(ref_weight)
            num_conditions += 1
        
    c = matrix(c)
    A = spmatrix(a, I, J)
    b = matrix(b)

    sol = solvers.lp(c, A, b)
    x = sol['x']

    if verbose:
        print c
        print A
        print b
        print x

        for i, (cc, xx) in enumerate(zip(c, x)):
            if xx > 1e-5:
                print links[i], cc, 'x', xx

    weight_match = sum([-cc * xx for cc, xx in zip(c, x)])

    if verbose:
        print '[match]', weight_match, '[ref]', weight_ref, '[para]', weight_para
        precision = 1.0 * weight_match / weight_para
        recall = 1.0 * weight_match / weight_ref
        f0_8 = precision * recall / (0.8 * precision + 0.2 * recall)
        print '[precision]', precision, '[recall]', recall, '[f0.8]', f0_8

    return weight_match, weight_ref, weight_para


def read_n_lines(f, n):
    lines = []
    while True:
        line = f.readline()
        if line == '':
            break

        lines.append(line)
        if len(lines) >= n:
            yield lines
            lines = []

    if len(lines) != 0:
        assert(len(lines) == n)
        yield lines


def process_line(ref_ngramss_with_weightss, para_ngramss_with_weightss):
    output = []

    # 4 iterations: unigram, bigram, trigram, four gram
    for ref_ngrams_with_weights, para_ngrams_with_weights in \
            zip(ref_ngramss_with_weightss, para_ngramss_with_weightss):

        if simmethod == 'surface':
            # optimized trivial case
            weight_match, weight_ref, weight_para = \
                surface_match(ref_ngrams_with_weights, para_ngrams_with_weights)
        else:
            # slower LP-based version for the general case
            weight_match, weight_ref, weight_para = \
                match(ref_ngrams_with_weights, para_ngrams_with_weights)

        output += [weight_match, weight_ref, weight_para]

    print >> outputfile, ' '.join(map(str, output))
    outputfile.flush()


if streaming:
    ref_ngramssss_with_weightssss = []       # ref_ngramssss_with_weightssss[line_number][ref_id][0,1,2,3][ngram]

    # read everything in ref first
    for refs in read_n_lines(open(reference), refcount):
        ref_ngramssss_with_weightssss.append([parse_ngrams_line(ref) for ref in refs])

    print >> sys.stderr, "References loaded."

    # now stream the candidates
    para_file = open(paraphrase)
    while True:
        para_line = para_file.readline()
        lineid = lineidsf.readline()
        if para_line == '' or lineid == '':
            break

        lineid = int(lineid)
        para_ngramss_with_weightss = parse_ngrams_line(para_line)

        for ref_id in range(refcount):
            process_line(ref_ngramssss_with_weightssss[lineid][ref_id], para_ngramss_with_weightss);
else:
    for ref_line, para_line in zip(open(reference), open(paraphrase)):
        ref_ngramss_with_weightss = parse_ngrams_line(ref_line)
        para_ngramss_with_weightss = parse_ngrams_line(para_line)
        process_line(ref_ngramss_with_weightss, para_ngramss_with_weightss)
