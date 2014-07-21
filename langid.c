/*
 * Port of langid.py to pure C.
 *
 * Marco Lui <saffsd@gmail.com>, July 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model.h"



/*
 * Unpack the packed version of tk_output into a data structure
 * we can actually use. tk_output provides a mapping from a state
 * to a list of indexes of features that are completed by entering 
 * that state.
 */
void unpack_tk_output(void){
    return;
}

/* 
 * Convert a text stream into a feature vector. The feature vector counts
 * how many times each sequence is seen.
 */
void text_to_fv(char *text, int textlen, int fv[]){
  int i, j, s=0;
  int sv[num_states];
  
  /* initialize sv and fv to zeros */
  /* an alternative data structure would allow us to skip this step:
   * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.30.7319
   */
  for (i=0; i < num_states; i++) sv[i] = 0;
  for (i=0; i < num_feats; i++) fv[i] = 0;


  for (i=0; i < textlen; i++){
      s = tk_nextmove[s][(unsigned char) text[i]];
      sv[s] += 1;
  }

  /* convert the SV into the FV */
  for (i=0; i < num_states; i++){
      if (sv[i] > 0) {
          for (j=0; j<tk_output_c[i]; j++){
              fv[tk_output[tk_output_s[i]+j]] += sv[i];
          }
      }
  }

  return;
}

void fv_to_logprob(int fv[], double logprob[]){
    int i, j;
    /* Initialize using prior */
    for (i=0; i < num_langs; i++){
        logprob[i] = nb_pc[i];
    }

    /* Compute posterior for each class */
    for (i=0; i < num_feats; i++){
        if (fv[i] > 0){
          for (j=0; j < num_langs; j++)
              logprob[j] += fv[i] * nb_ptc[i][j];
        }
    }

    return;
}

int logprob_to_pred(double logprob[]){
    int m=0, i;

    for (i=1; i<num_langs; i++){
        if (logprob[m] < logprob[i]) m = i;
    }

    return m;
}

const char *identify(char *text, int textlen){
    int fv[num_feats];
    double lp[num_langs];

    text_to_fv(text, textlen, fv);
    fv_to_logprob(fv, lp);
    return nb_classes[logprob_to_pred(lp)];
}

int main(int argc, char **argv){
    const char* lang;
    char buf[256];

    while (1){
      printf(">>> ");
      fgets(buf, sizeof(buf), stdin);
      lang = identify(buf, strlen(buf));
      printf("Identified as %s\n", lang);

    }
    return 0;
}


