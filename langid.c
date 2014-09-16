/*
 * Port of langid.py to pure C.
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
#include "langid.h"
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

const char* no_file = "NOSUCHFILE";
const char* not_file = "NOTAFILE";


int main(int argc, char **argv){
    const char* lang;
    size_t path_size = 4096, text_size=4096;
    ssize_t pathlen, textlen;
    char *path = NULL, *text = NULL; /* NULL init required for use with getline/getdelim*/
    LanguageIdentifier *lid;

    /* for use while accessing files through mmap*/
    int fd;

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
     * we have an interactive mode determined by isatty, and then
     * the three modes are file-mode (default), line-mode and batch-mode
     */

    if (isatty(fileno(stdin))){
      printf("langid.c interactive mode.\n");

      for(;;) {
        printf(">>> ");
        textlen = getline(&text, &text_size, stdin);
        if (textlen == 1 || textlen == -1) break; /* -1 for EOF and 1 for only newline */
        lang = identify(lid, text, textlen);
        printf("%s,%zd\n", lang, textlen);
      } 

    }
    else if (l_flag) { /*line mode*/

      while ((textlen = getline(&text, &text_size, stdin)) != -1){
        lang = identify(lid, text, textlen);
        printf("%s,%zd\n", lang, textlen);
      }

    }
    else if (b_flag) { /*batch mode*/

      /* loop on stdin, interpreting each line as a path */
      while ((pathlen = getline(&path, &path_size, stdin)) != -1){
        path[pathlen-1] = '\0';
        /* TODO: ensure that path is a real file. 
         * the main issue is with directories I think, no problem reading from a pipe or socket
         * presumably. Anything that returns data should be fair game.*/
        if ((fd = open(path, O_RDONLY))==-1) {
          lang = no_file;
        }
        else {
          textlen = lseek(fd, 0, SEEK_END);
          text = (char *) mmap(NULL, textlen, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
          lang = identify(lid, text, textlen);

          /* no need to munmap if textlen is 0 */
          if (textlen && (munmap(text, textlen) == -1)) {
            fprintf(stderr, "failed to munmap %s of length %zd \n", path, textlen);
            exit(-1);
          }

          close(fd);
        }
        printf("%s,%zd,%s\n", path, textlen, lang);
      }

    }
    else { /*file mode*/

      /* read all of stdin and process as a single file */
      textlen = getdelim(&text, &text_size, EOF, stdin);
      lang = identify(lid, text, textlen);
      printf("%s,%zd\n", lang, textlen);
      free(text);

    }

    destroy_identifier(lid);
    return 0;
}


