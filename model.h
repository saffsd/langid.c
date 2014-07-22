#ifndef _MODEL_H
#define _MODEL_H

#define NUM_FEATS 7480
#define NUM_LANGS 97
#define NUM_STATES 9118

extern const unsigned tk_nextmove[NUM_STATES][256];
extern const unsigned tk_output_c[NUM_STATES];
extern const unsigned tk_output_s[NUM_STATES];
extern const unsigned tk_output[];
extern const double nb_pc[NUM_LANGS];
const double nb_ptc[NUM_FEATS][NUM_LANGS];
extern const char *nb_classes[NUM_LANGS];

#endif
