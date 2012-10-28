import operator
import random

def smart_open(fname, mode = 'r'):
    if fname.endswith('.gz'):
        import gzip
        # Using max compression (9) by default seems to be slow.
        # Let's try using the fastest.
        return gzip.open(fname, mode, 1)
    else:
        return open(fname, mode)


def randint(b, a=0):
    return random.randint(a,b)

def uniq(seq, idfun=None):
    # order preserving
    if idfun is None:
        def idfun(x): return x
    seen = {}
    result = []
    for item in seq:
        marker = idfun(item)
        # in old Python versions:
        # if seen.has_key(marker)
        # but in new ones:
        if marker in seen: continue
        seen[marker] = 1
        result.append(item)
    return result
        

def sort_dict(myDict, byValue=False, reverse=False):
    if byValue:
        items = myDict.items()
        items.sort(key = operator.itemgetter(1), reverse=reverse)
    else:
        items = sorted(myDict.items())
    return items


def paragraphs(file, separator=None, includeEmpty=False):
    if not callable(separator):
        def separator(line): return line == '\n'
    paragraph = []
    for line in file:
        if separator(line):
            if includeEmpty or paragraph:
                yield ''.join(paragraph)
                paragraph = []
        else:
            paragraph.append(line)
    if includeEmpty or paragraph:
        yield ''.join(paragraph)
                                                                            
