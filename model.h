#ifndef _MODEL_H
#define _MODEL_H

extern const int num_feats;
extern const int num_langs;
extern const int num_states;

extern const unsigned tk_nextmove[9118][256];
extern const unsigned tk_output_c[9118];
extern const unsigned tk_output_s[9118];
extern const unsigned tk_output[];
extern const double nb_pc[97];
const double nb_ptc[7480][97];
extern const char *nb_classes[97];

#endif
