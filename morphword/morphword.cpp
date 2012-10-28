
// A large portion of the code in this file is modified from the source code
// in the WordNet-2.1; I display verbatim their license below:

/*
WordNet Release 2.1

This software and database is being provided to you, the LICENSEE, by  
Princeton University under the following license.  By obtaining, using  
and/or copying this software and database, you agree that you have  
read, understood, and will comply with these terms and conditions.:  
  
Permission to use, copy, modify and distribute this software and  
database and its documentation for any purpose and without fee or  
royalty is hereby granted, provided that you agree to comply with  
the following copyright notice and statements, including the disclaimer,  
and that the same appear on ALL copies of the software, database and  
documentation, including modifications that you make for internal  
use or for distribution.  
  
WordNet 2.1 Copyright 2005 by Princeton University.  All rights reserved.  
  
THIS SOFTWARE AND DATABASE IS PROVIDED "AS IS" AND PRINCETON  
UNIVERSITY MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR  
IMPLIED.  BY WAY OF EXAMPLE, BUT NOT LIMITATION, PRINCETON  
UNIVERSITY MAKES NO REPRESENTATIONS OR WARRANTIES OF MERCHANT-  
ABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR THAT THE USE  
OF THE LICENSED SOFTWARE, DATABASE OR DOCUMENTATION WILL NOT  
INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR  
OTHER RIGHTS.  
  
The name of Princeton University or Princeton may not be used in  
advertising or publicity pertaining to distribution of the software  
and/or database.  Title to copyright in this software, database and  
any associated documentation shall at all times remain with  
Princeton University and LICENSEE agrees to preserve same.  
*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <vector>
#include <set>
#include <map>
#include <string>
#include <math.h>
#include <string.h>

using namespace std;

#define SENTENCE_BUFFER_SIZE 500000
#define MAX_TOKENS_PER_LINE  5000
#define TOKEN_SIZE           5000

#define MAX_FORMS            5
#define NUMPARTS             4
#define NOUN 1
#define VERB 2
#define ADJECTIVE 3
#define ADVERB 4

static char *sufx[] ={
    /* Noun suffixes */
    "s", "ses", "xes", "zes", "ches", "shes", "men", "ies",
    /* Verb suffixes */
    "s", "ies", "es", "es", "ed", "ed", "ing", "ing",
    /* Adjective suffixes */
    "er", "est", "er", "est"
};

static char *addr[] ={
    /* Noun endings */
    "", "s", "x", "z", "ch", "sh", "man", "y",
    /* Verb endings */
    "", "y", "e", "", "e", "", "e", "",
    /* Adjective endings */
    "", "", "e", "e"
};

#define NUMPREPS        15

static struct {
    char *str;
    int strlen;
} prepositions[NUMPREPS] = {
    "to", 2,
    "at", 2,
    "of", 2,
    "on", 2,
    "off", 3,
    "in", 2,
    "out", 3,
    "up", 2,
    "down", 4,
    "from", 4,
    "with", 4,
    "into", 4,
    "for", 3,
    "about", 5,
    "between", 7,
};

static int offsets[NUMPARTS] = { 0, 0, 8, 16 };
static int cnts[NUMPARTS] = { 0, 8, 8, 4 };

map<int, set<string> > lemmaSet;         // <POStype, .>
map<int, map<string, string> > excMap;   // <POStype, <..> >

FILE *indexFiles[MAX_FORMS];
FILE *excFiles[MAX_FORMS];

// =========================================
#define KEY_LEN         (1024)
#define LINE_LEN        (1024*25)

static char line[LINE_LEN]; 
long last_bin_search_offset = 0;

/* General purpose binary search function to search for key as first
   item on line in open file.  Item is delimited by space. */

#undef getc

char *read_index(long offset, FILE *fp) {
    char *linep;

    linep = line;
    line[0] = '0';

    fseek( fp, offset, SEEK_SET );
    fgets(linep, LINE_LEN, fp);
    return(line);
}

char *bin_search(char *searchkey, FILE *fp)
{
    int c;
    long top, mid, bot, diff;
    char *linep, key[KEY_LEN];
    int length;

    diff=666;
    linep = line;
    line[0] = '\0';

    fseek(fp, 0L, 2);
    top = 0;
    bot = ftell(fp);
    mid = (bot - top) / 2;

    do {
        fseek(fp, mid - 1, 0);
        if(mid != 1)
            while((c = getc(fp)) != '\n' && c != EOF);
        last_bin_search_offset = ftell( fp );
        fgets(linep, LINE_LEN, fp);
        length = (int)(strchr(linep, ' ') - linep);
        strncpy(key, linep, length);
        key[length] = '\0';
        if(strcmp(key, searchkey) < 0) {
            top = mid;
            diff = (bot - top) / 2;
            mid = top + diff;
        }
        if(strcmp(key, searchkey) > 0) {
            bot = mid;
            diff = (bot - top) / 2;
            mid = top + diff;
        }
    } while((strcmp(key, searchkey)) && (diff != 0));
    
    if(!strcmp(key, searchkey))
        return(line);
    else
        return(NULL);
}
// ==============================

int determinePOS(char *pos) {
   if( (strcmp(pos, "NN")==0) || (strcmp(pos, "NNS")==0) || (strcmp(pos, "NNP")==0) || (strcmp(pos, "NNPS")==0) ) {
     return NOUN;
   }
   else if( (strcmp(pos, "VB")==0) || (strcmp(pos, "VBD")==0) || (strcmp(pos, "VBG")==0) || (strcmp(pos, "VBN")==0) || (strcmp(pos, "VBP")==0) || (strcmp(pos, "VBZ")==0) || (strcmp(pos, "MD")==0) ) {
     return VERB;
   }
   else if( (strcmp(pos, "JJ")==0) || (strcmp(pos, "JJR")==0) || (strcmp(pos, "JJS")==0) ) {
     return ADJECTIVE;
   }
   else if( (strcmp(pos, "RB")==0) || (strcmp(pos, "RBR")==0) || (strcmp(pos, "RBS")==0) || (strcmp(pos, "WRB")==0) ) {
     return ADVERB;
   }
   else {
     return -1;
   }
}

void readWNLemma(char *myFilename, int POStype) {
  FILE *infile;
  char *sentence, *token;
  int length;
  set<string> mySet;

  if((sentence = (char *)malloc(SENTENCE_BUFFER_SIZE))==NULL) {
    fprintf(stderr, "Not enough memory for sentence.\n");
    exit(1);
  }

  infile = fopen(myFilename, "r");
  if(infile==NULL) {
    fprintf(stderr, "Cannot open %s.\n", myFilename);
    exit(1);
  }

  mySet.clear();
  while(fgets(sentence, SENTENCE_BUFFER_SIZE, infile)!=NULL) { // for each line in file
    // trim away the ending newline character
    length = strlen(sentence);
    if(sentence[length-1] == '\n') {
      sentence[length-1] = '\0';
    }

    token = strtok(sentence, " ");
    mySet.insert(string(token));
  }

  lemmaSet.insert( pair<int, set<string> >(POStype, mySet) );

  fclose(infile);
  free(sentence);
}

void readExcList(char *myFilename, int POStype) {
  FILE *infile;
  char *sentence, *token1, *token2;
  int length;
  map<string, string> myMap;

  if((sentence = (char *)malloc(SENTENCE_BUFFER_SIZE))==NULL) {
    fprintf(stderr, "Not enough memory for sentence.\n");
    exit(1);
  }

  infile = fopen(myFilename, "r");
  if(infile==NULL) {
    fprintf(stderr, "Cannot open %s.\n", myFilename);
    exit(1);
  }

  myMap.clear();
  while(fgets(sentence, SENTENCE_BUFFER_SIZE, infile)!=NULL) { // for each line in file
    // trim away the ending newline character
    length = strlen(sentence);
    if(sentence[length-1] == '\n') {
      sentence[length-1] = '\0';
    }

    token1 = strtok(sentence, " ");
    token2 = strtok(NULL, " ");
    myMap.insert( pair<string, string>(string(token1), string(token2)) );
  }

  excMap.insert( pair<int, map<string, string> >(POStype, myMap) );

  fclose(infile);
  free(sentence);
}

/*
int lemmaInWN(char *myLemmaString, int POStype) {
  //if(lemmaSet.find(POStype) != lemmaSet.end()) {
    set<string> mySet = lemmaSet.find(POStype)->second;
    if(mySet.find(string(myLemmaString)) != mySet.end()) {
      return 1;
    }
  //}
  return 0;
}
*/

int is_defined(char *myLemmaString, int POStype) {
  if(bin_search(myLemmaString, indexFiles[POStype])!=NULL) {
    return 1;
  }
  else {
    return 0;
  }
}

/*
string strtolower(string str) {
  char myToken[TOKEN_SIZE];
  int i;

  strcpy(myToken, str.c_str());
  for(i=0; i<strlen(myToken); i++) {
    myToken[i] = tolower(myToken[i]);
  }  
  return(string(myToken));
}
*/

char *strsubst(char *str, char from, char to)
{
    register char *p;

    for (p = str; *p != 0; ++p)
        if (*p == from)
            *p = to;
    return str;
}

// check if \param searchstr (or slight variations of it) are present in the WordNet index.* file
// \param dbase indicates which POS searchstr belongs to
// The different variations are:
// replace hyphens with underscores, replace underscores with hyphens, strip hyphens and underscores, strip periods.
int getIndex(char *searchstr, int POStype) {
  int i, j, k;
  char c;
  char strings[MAX_FORMS][TOKEN_SIZE]; 
  int offsets[MAX_FORMS];

  for(i=0; i<MAX_FORMS; i++) {
    strings[i][0] = '\0';
  }

  if(searchstr != NULL) {
    //string myString = strtolower(string(searchstr));
    for(i = 0; i < MAX_FORMS; i++) {
      //strcpy(strings[i], myString.c_str());
      strcpy(strings[i], searchstr);
      offsets[i] = 0;
    }

    strsubst(strings[1], '_', '-');
    strsubst(strings[2], '-', '_');

    // remove all spaces and hyphens from last search string, then all periods
    for(i = j = k = 0; (c = searchstr[i]) != '\0'; i++) {
      if(c != '_' && c != '-')
        strings[3][j++] = c;
      if(c != '.')
        strings[4][k++] = c;
    }
    strings[3][j] = '\0';
    strings[4][k] = '\0';

    if(strings[0][0] != '\0') {
      //offsets[0] = lemmaInWN(strings[0], POStype);
      //if(bin_search(strings[0], indexFiles[POStype])!=NULL) {
      if(is_defined(strings[0], POStype)) {
        return 1;
      }
      //if(lemmaInWN(strings[0], POStype)) {
      //  return 1;
      //}
    }

    for(i = 1; i < MAX_FORMS; i++) {
      if((strings[i][0]) != '\0' && (strcmp(strings[0], strings[i]))) {
        //offsets[i] = lemmaInWN(strings[i], POStype);
        //if(bin_search(strings[i], indexFiles[POStype])!=NULL) {
        if(is_defined(strings[i], POStype)) {
          return 1;
        }
        //if(lemmaInWN(strings[i], POStype)) {
        //  return 1;
        //}
      }
    }
  }

  //for(i=0; i<MAX_FORMS; i++) {
  //  if(offsets[i]) {
  //    return 1;
  //  }
  //}

  return 0;
}

/*
char * exc_lookup(char *searchstr, int POStype) {
  //if(excMap.find(POStype) != excMap.end()) {
    map<string, string> myMap = excMap.find(POStype)->second;
    if(myMap.find(string(searchstr)) != myMap.end()) {
      return (char *)(myMap.find(string(searchstr))->second).c_str();
    }
  //}
  return NULL;
}
*/

static char *exc_lookup(char *word, int pos)
{
  static char line[SENTENCE_BUFFER_SIZE], *beglp, *endlp;
  char *excline;
  int found = 0;

  if(excFiles[pos] == NULL)
    return(NULL);

  /* first time through load line from exception file */
  if(word != NULL){
    if((excline = bin_search(word, excFiles[pos])) != NULL) {
      strcpy(line, excline);
      endlp = strchr(line,' ');
    } 
    else {
      endlp = NULL;
    }
  }

  if(endlp && *(endlp + 1) != ' ') {
    beglp = endlp + 1;
    while(*beglp && *beglp == ' ') beglp++;
    endlp = beglp;
    while(*endlp && *endlp != ' ' && *endlp != '\n') endlp++;
    if(endlp != beglp) {
      *endlp='\0';
      return(beglp);
    }
  }
  beglp = NULL;
  endlp = NULL;
  return(NULL);
}

static int strend(char *str1, char *str2)
{
  char *pt1;

  if(strlen(str2) >= strlen(str1))
    return(0);
  else {
    pt1=str1;
    pt1=strchr(str1,0);
    pt1=pt1-strlen(str2);
    return(!strcmp(pt1,str2));
  }
}

// used by the morphword function
static char *wordbase(char *word, int ender)
{
    char *pt1;
    static char copy[TOKEN_SIZE];

    strcpy(copy, word);
    if(strend(copy,sufx[ender])) {
        pt1=strchr(copy,'\0');
        pt1 -= strlen(sufx[ender]);
        *pt1='\0';
        strcat(copy,addr[ender]);
    }
    return(copy);
}

// Try to find baseform (lemma) of individual word in POS
char *morphword(char *word, int pos)
{
  int offset, cnt;
  int i;
  static char retval[TOKEN_SIZE];
  char *tmp, tmpbuf[TOKEN_SIZE], *end;

  sprintf(retval,"");
  sprintf(tmpbuf, "");
  end = "";
    
  if(word == NULL) 
    return(NULL);

  // first look for word on exception list
  if((tmp = exc_lookup(word, pos)) != NULL)
    return(tmp);            // found it in exception list

  if(pos == ADVERB) {           // only use exception list for adverbs
    return(NULL);
  }
  if(pos == NOUN) {
    if(strend(word, "ful")) {    // does the noun end in "ful"?
      cnt = strrchr(word, 'f') - word;  // strrchr(.) returns a pointer to the last occurrence of 'f'
      strncat(tmpbuf, word, cnt);       // appends the first 'cnt' chars of 'word' to destination 'tmpbuf'
                                        //   essentially, get the part before the 'ful'
      end = "ful";
    } 
    else {
      // check for noun ending with 'ss' or short words
      if(strend(word, "ss") || (strlen(word) <= 2))
        return(NULL);
    }
  }

  // If not in exception list, try applying rules from tables
  if(tmpbuf[0] == '\0')
    strcpy(tmpbuf, word);

  offset = offsets[pos];
  cnt = cnts[pos];

  for(i = 0; i < cnt; i++){   // check thru each of the suffix; will return from this function using the first one that succeeds
    strcpy(retval, wordbase(tmpbuf, (i + offset)));  // check whether tmpbuf ends in certain suffix (declared in 'sufx')
                                                     //   if so, replace the suffix with the corresponding one in 'addr'
    //if(strcmp(retval, tmpbuf) && lemmaInWN(retval, pos)) {
    //if(strcmp(retval, tmpbuf) && (bin_search(retval, indexFiles[pos])!=NULL) ) {
    if(strcmp(retval, tmpbuf) && getIndex(retval, pos)) {
      strcat(retval, end);
      return(retval);
    }
  }

  return(NULL);
}

// Count the number of underscore or space separated words in a string.
int cntwords(char *s, char separator) {
  register int wdcnt = 0;

  while(*s) {
    if(*s == separator || *s == ' ' || *s == '_') {
      wdcnt++;
      while(*s && (*s == separator || *s == ' ' || *s == '_'))
        s++;
    } 
    else {
      s++;
    }
  }
  return(++wdcnt);
}

// Find a preposition in the verb string and return its corresponding word number.
static int hasprep(char *s, int wdcnt)
{
  int i, wdnum;

  for(wdnum = 2; wdnum <= wdcnt; wdnum++) {
    s = strchr(s, '_');
    for(s++, i = 0; i < NUMPREPS; i++)
      if(!strncmp(s, prepositions[i].str, prepositions[i].strlen) &&
         (s[prepositions[i].strlen] == '_' || s[prepositions[i].strlen] == '\0'))
        return(wdnum);
  }
  return(0);
}

static char *morphprep(char *s)
{
  char *rest, *exc_word, *lastwd = NULL, *last;
  int i, offset, cnt;
  char word[TOKEN_SIZE], end[TOKEN_SIZE];
  static char retval[TOKEN_SIZE];

  // Assume that the verb is the first word in the phrase. Strip it off, check for validity, 
  // then try various morphs with the rest of the phrase tacked on, trying to find a match.

  rest = strchr(s, '_');
  last = strrchr(s, '_');
  if(rest != last) {         // more than 2 words
    if(lastwd = morphword(last + 1, NOUN)) {   // assume it's made up of a verb+prep+noun
      strncpy(end, rest, last - rest + 1);    // the part in between first and last word
      end[last-rest+1] = '\0';
      strcat(end, lastwd);
    }
  }

  strncpy(word, s, rest - s);   // first word
  word[rest - s] = '\0';
  for(i = 0, cnt = strlen(word); i < cnt; i++)
    if(!isalnum((unsigned char)(word[i]))) return(NULL);  // verb must be every char alnum

  offset = offsets[VERB];
  cnt = cnts[VERB];

  // First try to find the verb in the exception list 

  if((exc_word = exc_lookup(word, VERB)) && strcmp(exc_word, word)) {
    sprintf(retval, "%s%s", exc_word, rest);  // exc_word is the lemma-form of the verb; then tag on the original rest
    //if(is_defined(retval, VERB))   // if after just doing morph on verb, then the phrase is found in WordNet..
    if(getIndex(retval, VERB))   // if after just doing morph on verb, then the phrase is found in WordNet..
      return(retval);
    else if (lastwd) {             // if I have done a morph on the noun just now
      sprintf(retval, "%s%s", exc_word, end);
      //if(is_defined(retval, VERB))
      if(getIndex(retval, VERB))
        return(retval);
    }
  }

  for(i = 0; i < cnt; i++) {        // cnt now denotes the number of different suffixes to try to change...
    if((exc_word = wordbase(word, (i + offset))) &&     // wordbase will replace the suffix
        strcmp(word, exc_word)) {                        // ending is different
      sprintf(retval, "%s%s", exc_word, rest);
      //if(is_defined(retval, VERB))
      if(getIndex(retval, VERB))
        return(retval);
      else if(lastwd) {
        sprintf(retval, "%s%s", exc_word, end);
        //if(is_defined(retval, VERB))
        if(getIndex(retval, VERB))
          return(retval);
      }
    }
  }

  sprintf(retval, "%s%s", word, rest);
  if(strcmp(s, retval))
    return(retval);
  if(lastwd) {
    sprintf(retval, "%s%s", word, end);
    if(strcmp(s, retval))
      return(retval);
  }
  return(NULL);
}

// invokes morphword above; to find lemmas for a word/collocation
char *morphstr(char *origstr, int pos)
{
  static char searchstr[TOKEN_SIZE], str[TOKEN_SIZE];
  static int svcnt, svprep;
  char word[TOKEN_SIZE], *tmp;
  int cnt, st_idx = 0, end_idx;
  int prep;
  char *end_idx1, *end_idx2;
  char *append;
    
  if(origstr != NULL) {
    // Assume string hasn't had spaces substitued with '_'
    strsubst(strcpy(str, origstr), ' ', '_');
    searchstr[0] = '\0';
    cnt = cntwords(str, '_');
    svprep = 0;

    // first try exception list
    if((tmp = exc_lookup(str, pos)) && strcmp(tmp, str)) {
      svcnt = 1;          // force next time to pass NULL
      return(tmp);
    }

    // Then try simply morph on original string

    if(pos != VERB && (tmp = morphword(str, pos)) && strcmp(tmp, str))
      return(tmp);

    if(pos == VERB && cnt > 1 && (prep = hasprep(str, cnt))) {
      // assume we have a verb followed by a preposition
      svprep = prep;
      return(morphprep(str));
    } 
    else {
      svcnt = cnt = cntwords(str, '-');
      while(origstr && --cnt) {
        end_idx1 = strchr(str + st_idx, '_');
        end_idx2 = strchr(str + st_idx, '-');
        if(end_idx1 && end_idx2) {
          if(end_idx1 < end_idx2) {
            end_idx = (int)(end_idx1 - str);
            append = "_";
          } 
          else {
            end_idx = (int)(end_idx2 - str);
            append = "-";
          }
        } 
        else {
          if(end_idx1) {
            end_idx = (int)(end_idx1 - str);
            append = "_";
          } 
          else {
            end_idx = (int)(end_idx2 - str);
            append = "-";
          }
        }
        if(end_idx < 0) return(NULL);          // shouldn't do this
        strncpy(word, str + st_idx, end_idx - st_idx);
        word[end_idx - st_idx] = '\0';
        if(tmp = morphword(word, pos))
          strcat(searchstr,tmp);
        else
          strcat(searchstr,word);
        strcat(searchstr, append);
        st_idx = end_idx + 1;
      }
            
      if(tmp = morphword(strcpy(word, str + st_idx), pos)) {
        strcat(searchstr,tmp);
      }
      else {
        strcat(searchstr,word);
      }

      //if(strcmp(searchstr, str) && is_defined(searchstr,pos))
      if(strcmp(searchstr, str) && getIndex(searchstr,pos))
        return(searchstr);
      else
        return(NULL);
    }
  } 
/*  
  else {                    // subsequent call on string
    if(svprep) {           // if verb has preposition, no more morphs
      svprep = 0;
      return(NULL);
    } 
    else if(svcnt == 1)
      return(exc_lookup(NULL, pos));
    else {
      svcnt = 1;
      if((tmp = exc_lookup(str, pos)) && strcmp(tmp, str))
        return(tmp);
      else
        return(NULL);
    }
  }
*/
}

void parseFile(char *filename) {
  char *sentence, *token, *sentenceTokens[1000], word[TOKEN_SIZE], pos[100], sentenceBuffer[SENTENCE_BUFFER_SIZE], tokenBuffer[TOKEN_SIZE*2];
  FILE *infile;
  int tokenNum, wordPos, length, count;
  char keyBuffer[TOKEN_SIZE], *valueBuffer; 
  char *baseForm, outputBuffer[SENTENCE_BUFFER_SIZE];
  bool process;

  // some initializations
  if((sentence = (char *)malloc(SENTENCE_BUFFER_SIZE)) == NULL) {
    fprintf(stderr, "Not enough memory to allocate buffer for variable sentence. Exiting.\n");
    exit(1);
  }
  for(int i=0; i<1000; i++) {
    sentenceTokens[i] = NULL;
  }

  infile = fopen(filename, "r");
  if(infile == NULL) {
    fprintf(stderr, "File %s could not be opened. Exiting.\n", filename);
    exit(1);
  }

  while(fgets(sentence, SENTENCE_BUFFER_SIZE, infile)!=NULL) {
    if(strcmp(sentence, "\n")!=0) {         // not merely a newline
      outputBuffer[0] = '\0';

      strcpy(sentenceBuffer, sentence);
      length = strlen(sentenceBuffer);
      sentenceBuffer[length-1] = '\0';

      tokenNum = 0;
      token = strtok(sentenceBuffer, " ");
      while(token!=NULL) {
        sentenceTokens[tokenNum++] = strdup(token);
        token = strtok(NULL, " ");
      }

      for(int i=0; i<tokenNum; i++) {
        process = true;    // to decide whether to find the moph root for this token
   
        if(strlen(outputBuffer) > 0) {
          strcpy(outputBuffer, strcat(outputBuffer, " "));
        }
        strcpy(outputBuffer, strcat(outputBuffer, sentenceTokens[i]));

        strcpy(tokenBuffer, sentenceTokens[i]);
        token = strtok(tokenBuffer, "_");   // word
        if(token==NULL) {
          fprintf(stderr, "1st token is NULL!\n");
          fprintf(stderr, "%s", sentence);
          process = false;
        }

        strcpy(word, token);

        token = strtok(NULL, "_");          // POS
        if(token==NULL) {
          fprintf(stderr, "2nd token is NULL!\n");
          fprintf(stderr, "%s", sentence);
          process = false;
        }

        if(process==true) {
          strcpy(pos, token);

          // now, to find the morphological root.
          // If cannot find any, means current word is in the morpho root already.
          wordPos = determinePOS(pos);    // if return -1, probably some punctuation

          if(wordPos!=-1) {
	    //lowercase word form
	    int i;
	    for(i=0; i<strlen(word); i++) {
	      word[i] = tolower(word[i]);
	    }  
            baseForm = morphstr(word, wordPos);
            //baseForm = morphword(word, wordPos);

            if(baseForm==NULL)  
              strcpy(keyBuffer, word);
            else 
              strcpy(keyBuffer, baseForm);

            strcpy(outputBuffer, strcat(outputBuffer, "_"));
            strcpy(outputBuffer, strcat(outputBuffer, keyBuffer));
          }
        }
      }
      printf("%s\n", outputBuffer);
      fflush(stdout);

      // free memory
      for(int i=0; i<tokenNum; i++) {
        free(sentenceTokens[i]);
        sentenceTokens[i] = NULL;
      }
    }
  }

  fclose(infile);
  free(sentence);
}



int main(int argc, char *argv[]) {
  char posTaggedFilename[500];
  int i;
  string resourceFilepath;

  //readWNLemma("index.adj", ADJECTIVE);
  //readWNLemma("index.noun", NOUN);
  //readWNLemma("index.verb", VERB);
  //readWNLemma("index.adv", ADVERB);

  //readExcList("noun.exc", NOUN);
  //readExcList("adj.exc", ADJECTIVE);
  //readExcList("verb.exc", VERB);
  //readExcList("adv.exc", ADVERB);


  for(i=0; i<MAX_FORMS; i++) {
    indexFiles[i] = NULL;
    excFiles[i] = NULL;
  }


  resourceFilepath = string(argv[1]) + string("/") + string("index.noun");
  indexFiles[NOUN] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("index.verb");
  indexFiles[VERB] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("index.adj");
  indexFiles[ADJECTIVE] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("index.adv");
  indexFiles[ADVERB] = fopen(resourceFilepath.c_str(), "r");


  resourceFilepath = string(argv[1]) + string("/") + string("noun.exc");
  excFiles[NOUN] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("verb.exc");
  excFiles[VERB] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("adj.exc");
  excFiles[ADJECTIVE] = fopen(resourceFilepath.c_str(), "r");

  resourceFilepath = string(argv[1]) + string("/") + string("adv.exc");
  excFiles[ADVERB] = fopen(resourceFilepath.c_str(), "r");


  strcpy(posTaggedFilename, argv[2]);
  parseFile(posTaggedFilename);

  for(i=0; i<MAX_FORMS; i++) {
    if(indexFiles[i] != NULL) {
      fclose(indexFiles[i]);
    }
    if(excFiles[i] != NULL) {
      fclose(excFiles[i]);
    }
  }
}



