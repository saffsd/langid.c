"""
Repack the Python model into a form that langid.c can read.

Marco Lui, July 2014
"""

import argparse
import langid.langid as langid
import array
import sys

model_template = """\
#include "model.h"

unsigned tk_nextmove[NUM_STATES][256] = {{{tk_nextmove}}};
unsigned tk_output_c[NUM_STATES] = {{{tk_output_c}}};
unsigned tk_output_s[NUM_STATES] = {{{tk_output_s}}};
unsigned tk_output[] = {{{tk_output}}};
double nb_pc[NUM_LANGS] = {{{nb_pc}}};
double nb_ptc[{nb_ptc_size}] = {{{nb_ptc}}};
char *nb_classes[NUM_LANGS] = {{{nb_classes}}};
"""

header_template = """\
#ifndef _MODEL_H
#define _MODEL_H

#define NUM_FEATS {num_feats}
#define NUM_LANGS {num_langs}
#define NUM_STATES {num_states}

extern unsigned tk_nextmove[NUM_STATES][256];
extern unsigned tk_output_c[NUM_STATES];
extern unsigned tk_output_s[NUM_STATES];
extern unsigned tk_output[];
extern double nb_pc[NUM_LANGS];
extern double nb_ptc[{nb_ptc_size}];
extern char *nb_classes[NUM_LANGS];

#endif
"""

def pack_tk_output(ident):
  num_states = len(ident.tk_nextmove) >> 8

  # tk_output is a mapping from state to list of feats completed by entering that state.
  # we encode it as a single array of 2-byte values. each "entry" is a state label, 
  # a number representing a count followed by count featlabels
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
  return tk_output_c, tk_output_s, tk_output

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument("--output", "-o", default=sys.stdout, help="write exported model to", type=argparse.FileType('w'))
  parser.add_argument("--header", action="store_true", help="produce header file")
  parser.add_argument("--protobuf", action="store_true", help="produce model in protocol buffer format")
  parser.add_argument("model", help="read model from")
  args = parser.parse_args()

  if args.protobuf and args.header:
    parser.error("can only specify one of --protobuf or --header")

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
  nb_ptc_size = num_feats * num_langs

  if args.protobuf:
    import langid_pb2

    tk_output_c, tk_output_s, tk_output = pack_tk_output(ident)

    lid = langid_pb2.LanguageIdentifier()

    # basic parameters
    lid.num_feats = num_feats
    lid.num_langs = num_langs
    lid.num_states = num_states

    # pack the tokenizer
    lid.tk_nextmove.extend(ident.tk_nextmove)
    lid.tk_output_c.extend(tk_output_c)
    lid.tk_output_s.extend(tk_output_s)
    lid.tk_output.extend(tk_output)

    # pack the classifier parameters
    lid.nb_pc.extend(ident.nb_pc.tolist())
    lid.nb_ptc.extend(ident.nb_ptc.ravel().tolist())

    # pack the class labels
    lid.nb_classes.extend('"{}"'.format(c) for c in ident.nb_classes)

    args.output.write(lid.SerializeToString())
    
  elif args.header:
    args.output.write(header_template.format(**locals()))
  else:
    # tk_nextmove is an array of size #states x 256 and encodes the transition table for the DFA
    # we encode it as a single array of 2-byte values
    tk_nextmove = ",".join(map(str,ident.tk_nextmove))

    tk_output_c, tk_output_s, tk_output = pack_tk_output(ident)
    tk_output_c = ",".join(map(str, tk_output_c))
    tk_output_s = ",".join(map(str, tk_output_s))
    tk_output = ",".join(map(str, tk_output))
        
    nb_pc =  ",".join(map(str,ident.nb_pc))
    nb_ptc = ",".join(map(str,ident.nb_ptc.ravel()))
    nb_classes = ','.join('"{}"'.format(c) for c in ident.nb_classes)

    args.output.write(model_template.format(**locals()))
