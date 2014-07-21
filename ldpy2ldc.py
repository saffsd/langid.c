"""
Repack the Python model into a form that langid.c can read.

Marco Lui, July 2014
"""

import argparse
import langid.langid as langid
import array
import sys

model_template = """\
const int num_feats = {num_feats};
const int num_langs = {num_langs};
const int num_states = {num_states};

const unsigned tk_nextmove[{num_states}][256] = {{{tk_nextmove}}};
const unsigned tk_output_c[{num_states}] = {{{tk_output_c}}};
const unsigned tk_output_s[{num_states}] = {{{tk_output_s}}};
const unsigned tk_output[] = {{{tk_output}}};
const double nb_pc[{num_langs}] = {{{nb_pc}}};
const double nb_ptc[{num_feats}][{num_langs}] = {{{nb_ptc}}};
const char *nb_classes[{num_langs}] = {{{nb_classes}}};
"""

header_template = """\
#ifndef _MODEL_H
#define _MODEL_H

extern const int num_feats;
extern const int num_langs;
extern const int num_states;

extern const unsigned tk_nextmove[{num_states}][256];
extern const unsigned tk_output_c[{num_states}];
extern const unsigned tk_output_s[{num_states}];
extern const unsigned tk_output[];
extern const double nb_pc[{num_langs}];
const double nb_ptc[{num_feats}][{num_langs}];
extern const char *nb_classes[{num_langs}];

#endif
"""

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--output", "-o", default=sys.stdout, help="write exported model to", type=argparse.FileType('w'))
  parser.add_argument("--header", action="store_true", help="produce header file")
  parser.add_argument("model", help="read model from")
  args = parser.parse_args()

  ident = langid.LanguageIdentifier.from_modelpath(args.model)

  print "NB_PTC", type(ident.nb_ptc), ident.nb_ptc.shape, ident.nb_ptc.dtype
  print "NB_PC", type(ident.nb_pc), ident.nb_pc.shape, ident.nb_pc.dtype
  print "NB_NUMFEATS", type(ident.nb_numfeats), ident.nb_numfeats
  print "NB_CLASSES", type(ident.nb_classes), len(ident.nb_classes)
  print "TK_NEXTMOVE", type(ident.tk_nextmove), len(ident.tk_nextmove), ident.tk_nextmove.typecode, ident.tk_nextmove.itemsize
  print "TK_OUTPUT", type(ident.tk_output), len(ident.tk_output)
  print

  num_feats, num_langs = ident.nb_ptc.shape
  num_states = len(ident.tk_nextmove) >> 8

  if args.header:
    args.output.write(header_template.format(**locals()))
  else:
    # tk_nextmove is an array of size #states x 256 and encodes the transition table for the DFA
    # we encode it as a single array of 2-byte values
    tk_nextmove = ",".join(map(str,ident.tk_nextmove))

    # tk_output is a mapping from state to list of feats completed by entering that state.
    # we encode it as a single array of 2-byte values. each "entry" is a state label, a number representing a count followed by count featlabels
    tk_output_c = []
    tk_output_s = []
    tk_output = []
    for i in range(num_states):
      if i in ident.tk_output and ident.tk_output[i]:
        count = len(ident.tk_output[i])
        feats = ident.tk_output[i]
      else:
        count = 0
        feats = []

      tk_output_c.append(count)
      tk_output_s.append(len(tk_output))
      tk_output.extend(feats)
    tk_output_c = ",".join(map(str, tk_output_c))
    tk_output_s = ",".join(map(str, tk_output_s))
    tk_output = ",".join(map(str, tk_output))
        
    nb_pc =  ",".join(map(str,ident.nb_pc))
    nb_ptc = ",".join(map(str,ident.nb_ptc.ravel()))
    nb_classes = ','.join('"{}"'.format(c) for c in ident.nb_classes)

    args.output.write(model_template.format(**locals()))
