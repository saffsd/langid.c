#ifndef _LANGID_H
#define _LANGID_H

#include "sparseset.h"

/* Structure containing all the state required to
 * implement a language identifier
 */
typedef struct {
    unsigned int num_feats;
    unsigned int num_langs;
    unsigned int num_states;

    unsigned (*tk_nextmove)[][256];
    unsigned (*tk_output_c)[];
    unsigned (*tk_output_s)[];
    unsigned (*tk_output)[];

    double (*nb_pc)[];
    double (*nb_ptc)[];

    char *(*nb_classes)[];
    
    /* sparsesets for counting states and features. these are
     * part of LanguageIdentifier as the clear operation on them
     * is much less costly than allocating them from scratch
     */
		Set *sv, *fv;
} LanguageIdentifier;

#endif
