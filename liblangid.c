/*
 * Implementation of the Language Identification method of Lui & Baldwin 2011
 * in pure C, based largely on langid.py, using the sparse set structures suggested
 * by Dawid Weiss.
 *
 * Marco Lui <saffsd@gmail.com>, July 2014
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "langid.pb-c.h"
#include "liblangid.h"
#include "sparseset.h"
#include "model.h"

/* Return a pointer to a LanguageIdentifier based on the in-built default model
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

LanguageIdentifier *load_identifier(char *model_path) {
		Langid__LanguageIdentifier *msg;
		int fd, model_len;
		char *model_buf;
    LanguageIdentifier *lid;

		/* Use mmap to access the model file */
		if ((fd = open(model_path, O_RDONLY))==-1) {
			fprintf(stderr, "unable to open: %s\n", model_path);
			exit(-1);
		}
		model_len = lseek(fd, 0, SEEK_END);
		model_buf = (char *) mmap(NULL, model_len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

		/*printf("read in a model of size %d\n", model_len);*/
		msg = langid__language_identifier__unpack(NULL, model_len, model_buf);

		if (msg == NULL) {
			fprintf(stderr, "error unpacking model from: %s\n", model_path);
			exit(-1);
		}

    if ((lid = (LanguageIdentifier *) malloc(sizeof(LanguageIdentifier))) == 0) exit(-1);

    lid->sv = alloc_set(msg->num_states);
    lid->fv = alloc_set(msg->num_feats);

    lid->num_feats = msg->num_feats;
    lid->num_langs = msg->num_langs;
    lid->num_states = msg->num_states;

    if ((lid->tk_nextmove = malloc(msg->n_tk_nextmove * sizeof(unsigned))) == 0) exit(-1);
    memcpy(lid->tk_nextmove, msg->tk_nextmove, msg->n_tk_nextmove * sizeof(unsigned));

    if ((lid->tk_output_c = malloc(msg->n_tk_output_c * sizeof(unsigned))) == 0) exit(-1);
    memcpy(lid->tk_output_c, msg->tk_output_c, msg->n_tk_output_c * sizeof(unsigned));

    if ((lid->tk_output_s = malloc(msg->n_tk_output_s * sizeof(unsigned))) == 0) exit(-1);
    memcpy(lid->tk_output_s, msg->tk_output_s, msg->n_tk_output_s * sizeof(unsigned));

    if ((lid->tk_output = malloc(msg->n_tk_output * sizeof(unsigned))) == 0) exit(-1);
    memcpy(lid->tk_output, msg->tk_output, msg->n_tk_output * sizeof(unsigned));

    if ((lid->nb_pc = malloc(msg->n_nb_pc * sizeof(double))) == 0) exit(-1);
    memcpy(lid->nb_pc, msg->nb_pc, msg->n_nb_pc * sizeof(double));

    if ((lid->nb_ptc = malloc(msg->n_nb_ptc * sizeof(double))) == 0) exit(-1);
    memcpy(lid->nb_ptc, msg->nb_ptc, msg->n_nb_ptc * sizeof(double));

    if ((lid->nb_classes = malloc(msg->n_nb_classes * sizeof(char *))) == 0) exit(-1);
    memcpy(lid->nb_classes, msg->nb_classes, msg->n_nb_classes * sizeof(char *));

    /* TODO: We are only copying the pointers to the strings and not the strings themselves!
     *       This means we cannot free the underlying model. It also seems silly to
     *       be copying all these values when the correct type casting should suffice.
     *langid__language_identifier__free_unpacked(msg, NULL);
     */

    return lid;
}

void destroy_identifier(LanguageIdentifier *lid){
		/* TODO: destroying this is tricky as there is no need to free the arrays if the
		 *       identifier is based on the in-built model, but there may be a need to do
		 *       so if it is based on a read-in model.
		 */
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

