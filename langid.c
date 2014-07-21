/*
 * Port of langid.py to pure C.
 *
 * Marco Lui <saffsd@gmail.com>, July 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sparseset.h"
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
void text_to_fv(char *text, int textlen, Set *sv, Set *fv){
  unsigned i, j, m, s=0;
  
  clear(sv);
  clear(fv);

  for (i=0; i < textlen; i++){
      s = tk_nextmove[s][(unsigned char) text[i]];
      add(sv, s, 1);
  }

  /* convert the SV into the FV */
  for (i=0; i < sv->members; i++) {
  		m = sv->dense[i];
  		for (j=0; j<tk_output_c[m]; j++){
				  add(fv, tk_output[tk_output_s[m]+j], sv->counts[i]);
			}
	}

  return;
}

void fv_to_logprob(Set *fv, double logprob[]){
    unsigned i, j, m;
    /* Initialize using prior */
    for (i=0; i < num_langs; i++){
        logprob[i] = nb_pc[i];
    }

    /* Compute posterior for each class */
    for (i=0; i< fv->members; i++){
				m = fv->dense[i];
    	  for (j=0; j < num_langs; j++){
						logprob[j] += fv->counts[i] * nb_ptc[m][j];
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
    double lp[num_langs];
		Set *sv, *fv;
		sv = alloc_set(num_states);
		fv = alloc_set(num_feats);

    text_to_fv(text, textlen, sv, fv);
    fv_to_logprob(fv, lp);

    free_set(sv);
    free_set(fv);
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


