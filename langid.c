/*
 * Port of langid.py to pure C.
 *
 * Marco Lui <saffsd@gmail.com>, July 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "langid.h"
#include "sparseset.h"
#include "model.h"

/* Return a pointer to a LanguageIdentifier based on the in-build default model
 */
LanguageIdentifier *get_default_identifier(void) {
    LanguageIdentifier *lid;

    if ((lid = (LanguageIdentifier *) malloc(sizeof(LanguageIdentifier))) == 0) exit(-1);

    lid->sv = alloc_set(NUM_STATES);
    lid->fv = alloc_set(NUM_FEATS);

    lid->num_feats = NUM_FEATS;
    lid->num_langs = NUM_LANGS;
    lid->num_states = NUM_STATES;
    lid->tk_nextmove = &tk_nextmove;
    lid->tk_output_c = &tk_output_c;
    lid->tk_output_s = &tk_output_s;
    lid->tk_output = &tk_output;
    lid->nb_pc = &nb_pc;
    lid->nb_ptc = &nb_ptc;
    lid->nb_classes = &nb_classes;

    return lid;
}

void destroy_identifier(LanguageIdentifier *lid){
    free_set(lid->sv);
    free_set(lid->fv);
    free(lid);
}

/* 
 * Convert a text stream into a feature vector. The feature vector counts
 * how many times each sequence is seen.
 */
void text_to_fv(LanguageIdentifier *lid, char *text, int textlen, Set *sv, Set *fv){
  unsigned i, j, m, s=0;
  
  clear(sv);
  clear(fv);

  for (i=0; i < textlen; i++){
      s = (*lid->tk_nextmove)[s][(unsigned char) text[i]];
      add(sv, s, 1);
  }

  /* convert the SV into the FV */
  for (i=0; i < sv->members; i++) {
  		m = sv->dense[i];
  		for (j=0; j<(*lid->tk_output_c)[m]; j++){
				  add(fv, (*lid->tk_output)[(*lid->tk_output_s)[m]+j], sv->counts[i]);
			}
	}

  return;
}

void fv_to_logprob(LanguageIdentifier *lid, Set *fv, double logprob[]){
    unsigned i, j, m;
    double *nb_ptc_p;
    /* Initialize using prior */
    for (i=0; i < NUM_LANGS; i++){
        logprob[i] = nb_pc[i];
    }

    /* Compute posterior for each class */
    for (i=0; i< fv->members; i++){
				m = fv->dense[i];
				/* NUM_FEATS * NUM_LANGS */
				nb_ptc_p = nb_ptc + m * NUM_LANGS;
    	  for (j=0; j < NUM_LANGS; j++){
						logprob[j] += fv->counts[i] * (*nb_ptc_p);
						nb_ptc_p += 1;
				}
		}

    return;
}

int logprob_to_pred(LanguageIdentifier *lid, double logprob[]){
    int m=0, i;

    for (i=1; i<lid->num_langs; i++){
        if (logprob[m] < logprob[i]) m = i;
    }

    return m;
}

const char *identify(LanguageIdentifier *lid, char *text, int textlen){
    double lp[NUM_LANGS];

    text_to_fv(lid, text, textlen, lid->sv, lid->fv);
    fv_to_logprob(lid, lid->fv, lp);

    return (*lid->nb_classes)[logprob_to_pred(lid,lp)];
}

const char* no_file = "NOSUCHFILE";


int main(int argc, char **argv){
    const char* lang;
    size_t path_size = 4096, text_size=4096;
    unsigned pathlen, textlen;
    char *path = NULL, *text = NULL; /* NULL init required for use with getline/getdelim*/
    FILE *file;
    LanguageIdentifier *lid;

    /* load a default LanguageIdentifier using the in-built model */
    lid = get_default_identifier();

    /* loop on stdin, interpreting each line as a path */
    while ((pathlen = getline(&path, &path_size, stdin)) != -1){
      path[pathlen-1] = '\0';
      /* TODO: ensure that path is a real file. */
      if ((file = fopen(path, "r")) == NULL) {
        lang = no_file;
      }
      else {
        textlen = getdelim(&text, &text_size, EOF, file);
        lang = identify(lid, text, textlen);
        fclose(file);
      }
      printf("%s,%d,%s\n", path, textlen, lang);
    }

    if (text != NULL) free(text);
    destroy_identifier(lid);
    return 0;
}


