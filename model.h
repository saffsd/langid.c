#ifndef _MODEL_H
#define _MODEL_H

#define NUM_FEATS 7480
#define NUM_LANGS 97
#define NUM_STATES 9118

extern unsigned tk_nextmove[NUM_STATES][256];
extern unsigned tk_output_c[NUM_STATES];
extern unsigned tk_output_s[NUM_STATES];
extern unsigned tk_output[];
extern double nb_pc[NUM_LANGS];
extern double nb_ptc[725560];
extern char *nb_classes[NUM_LANGS];

#endif
