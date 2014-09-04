/*
 * Port of langid.py to pure C.
 *
 * Marco Lui <saffsd@gmail.com>, July 2014
 */

#include <unistd.h>
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

LanguageIdentifier *load_identifier(char *model_path) {
    LanguageIdentifier *lid;
    fprintf(stderr, "loading model from file not implemented!\n");
    exit(-1);
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
const char* not_file = "NOTAFILE";


int main(int argc, char **argv){
    const char* lang;
    size_t path_size = 4096, text_size=4096;
    unsigned pathlen, textlen;
    char *path = NULL, *text = NULL; /* NULL init required for use with getline/getdelim*/
    FILE *file;
    LanguageIdentifier *lid;

    /* for use with getopt */
    char *model_path = NULL;
    int c, l_flag = 0, b_flag = 0;
    opterr = 0;

    /* valid options are:
     * l: line-mode
     * b: batch-mode
     * m: load a model file
     */

    while ((c = getopt (argc, argv, "lbm:")) != -1) 
      switch (c) {
        case 'l':
          l_flag = 1;
          break;
        case 'b':
          b_flag = 1;
          break;
        case 'm':
          model_path = optarg;
          break;
        case '?':
          if (optopt == 'm')
            fprintf (stderr, "Option -%c requires an argument.\n", optopt);
          else if (isprint (optopt))
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
          else
            fprintf (stderr,
                    "Unknown option character `\\x%x'.\n",
                    optopt);
          return 1;
        default:
          abort();
      }

    /* validate getopt options */
    if (l_flag && b_flag) {
      fprintf(stderr, "Cannot specify both -l and -b.\n");
      exit(-1);
    }
    
    /* load an identifier */
    lid = model_path ? load_identifier(model_path) : get_default_identifier();

    /* enter appropriate operating mode.
     * the three modes are file-mode (default), line-mode and batch-mode
     */

    if (l_flag) { /*line mode*/

      while ((textlen = getline(&text, &text_size, stdin)) != -1){
        lang = identify(lid, text, textlen);
        printf("%s,%d\n", lang, textlen);
      }

    }
    else if (b_flag) { /*batch mode*/

      /* loop on stdin, interpreting each line as a path */
      while ((pathlen = getline(&path, &path_size, stdin)) != -1){
        path[pathlen-1] = '\0';
        /* TODO: ensure that path is a real file. 
         * the main issue is with directories I think, no problem reading from a pipe or socket
         * presumably. Anything that returns data should be fair game.*/
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

    }
    else { /*file mode*/

      /* read all of stdin and process as a single file */
      textlen = getdelim(&text, &text_size, EOF, stdin);
      lang = identify(lid, text, textlen);
      printf("%s,%d\n", lang, textlen);

    }

    if (text != NULL) free(text);
    destroy_identifier(lid);
    return 0;
}


