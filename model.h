#ifndef _MODEL_H
#define _MODEL_H

extern const int num_feats;
extern const int num_langs;
extern const int num_states;

extern const unsigned tk_nextmove[1880][256];
extern const unsigned tk_output_c[1880];
extern const unsigned tk_output_s[1880];
extern const unsigned tk_output[];
extern const double nb_pc[4];
const double nb_ptc[1200][4];
extern const char *nb_classes[4];

#endif
