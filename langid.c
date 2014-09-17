/* 
 * Command-line driver for liblangid
 *
 * Marco Lui <saffsd@gmail.com>, September 2014
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "liblangid.h"

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

      printf("Bye!\n");

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


