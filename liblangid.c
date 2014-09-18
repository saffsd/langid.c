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

    lid->protobuf_model = NULL;

    return lid;
}

LanguageIdentifier *load_identifier(char *model_path) {
		Langid__LanguageIdentifier *msg;
		int fd, model_len;
		unsigned char *model_buf;
    LanguageIdentifier *lid;
#ifdef DEBUG
		int i;
#endif

#ifdef DEBUG
		fprintf(stderr, "loading a model from: %s\n", model_path);
#endif

		/* Use mmap to access the model file */
		if ((fd = open(model_path, O_RDONLY))==-1) {
			fprintf(stderr, "unable to open: %s\n", model_path);
			exit(-1);
		}
		model_len = lseek(fd, 0, SEEK_END);
		model_buf = (unsigned char *) mmap(NULL, model_len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);

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

    lid->tk_nextmove = (unsigned (*)[][256])msg->tk_nextmove;
    lid->tk_output_c = (unsigned (*)[])msg->tk_output_c;
    lid->tk_output_s = (unsigned (*)[])msg->tk_output_s;
    lid->tk_output = (unsigned (*)[])msg->tk_output;

    lid->nb_pc = (double (*)[]) msg->nb_pc;
    lid->nb_ptc =(double (*)[]) msg->nb_ptc;
    lid->nb_classes = (char *(*)[]) msg->nb_classes;

#ifdef DEBUG
		fprintf(stderr, "num_feats: %d num_langs: %d num_states: %d\n", lid->num_feats, lid->num_langs, lid->num_states);

		for (i=0; i<lid->num_langs; i++){
			fprintf(stderr, "  lang:%s\n", (*lid->nb_classes)[i]);
			fprintf(stderr, "  lid->nb_pc:%lf\n", (*lid->nb_pc)[i]);
			fprintf(stderr, "  msg->nb_pc:%lf\n", (msg->nb_pc)[i]);
			/*fprintf(stderr, "  lang:%s msg->nb_pc: %lf lid->nb_pc: %lf\n", (*lid->nb_classes)+i, msg->nb_pc+i, lid->nb_pc+i);
			 */
		}
#endif

    lid->protobuf_model = msg;

    return lid;
}

void destroy_identifier(LanguageIdentifier *lid){
    if (lid->protobuf_model != NULL) 
        langid__language_identifier__free_unpacked(lid->protobuf_model, NULL);
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
    for (i=0; i < lid->num_langs; i++){
        logprob[i] = (*lid->nb_pc)[i];
    }

    /* Compute posterior for each class */
    for (i=0; i< fv->members; i++){
				m = fv->dense[i];
				/* NUM_FEATS * NUM_LANGS */
				nb_ptc_p = &(*lid->nb_ptc)[m*lid->num_langs];
    	  for (j=0; j < lid->num_langs; j++){
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
    double lp[lid->num_langs];
    int pred;
#ifdef DEBUG
		int i;
#endif

    text_to_fv(lid, text, textlen, lid->sv, lid->fv);
    fv_to_logprob(lid, lid->fv, lp);
		pred = logprob_to_pred(lid,lp);

#ifdef DEBUG
		fprintf(stderr,"pred lang: %s logprob: %lf\n", (*lid->nb_classes)[pred], lp[pred]);
		for (i=0; i<lid->num_langs; i++){
			fprintf(stderr,"  lang: %s logprob: %lf\n", (*lid->nb_classes)[i], lp[i]);
		}
#endif

    return (*lid->nb_classes)[pred];
}

