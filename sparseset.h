#ifndef _SPARSESET_H
#define _SPARSESET_H
#include <stdlib.h>

typedef struct {
    unsigned members;
    unsigned *sparse;
    unsigned *dense;
    unsigned *counts;
} Set;

extern Set *alloc_set(size_t size);
extern void free_set(Set *s);
extern void clear(Set *s);
extern void add(Set *s, unsigned key, unsigned val);

#endif
