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
    for (i=0; i < NUM_LANGS; i++){
        logprob[i] = nb_pc[i];
    }

    /* Compute posterior for each class */
    for (i=0; i< fv->members; i++){
				m = fv->dense[i];
    	  for (j=0; j < NUM_LANGS; j++){
						logprob[j] += fv->counts[i] * nb_ptc[m][j];
				}
		}

    return;
}

int logprob_to_pred(double logprob[]){
    int m=0, i;

    for (i=1; i<NUM_LANGS; i++){
        if (logprob[m] < logprob[i]) m = i;
    }

    return m;
}

const char *identify(char *text, int textlen){
    double lp[NUM_LANGS];
		Set *sv, *fv;
		sv = alloc_set(NUM_STATES);
		fv = alloc_set(NUM_FEATS);

    text_to_fv(text, textlen, sv, fv);
    fv_to_logprob(fv, lp);

    free_set(sv);
    free_set(fv);
    return nb_classes[logprob_to_pred(lp)];
}

const char* no_file = "NOSUCHFILE";

int main(int argc, char **argv){
    const char* lang;
    size_t path_size = 4096, text_size=4096;
    unsigned pathlen, textlen;
    char *path, *text;
    FILE *file;

    if ((path = (char *) malloc(path_size)) == 0) exit(-1);

    while ((pathlen = getline(&path, &path_size, stdin)) != -1){
      path[pathlen-1] = '\0';
      if ((file = fopen(path, "r")) == NULL) {
        lang = no_file;
      }
      else {
        textlen = getdelim(&text, &text_size, EOF, file);
        lang = identify(text, textlen);
        fclose(file);
      }
      printf("%s,%s\n", path, lang);
    }
    return 0;
}


