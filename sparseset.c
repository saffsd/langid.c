/* Implementation of sparse sets based on
 * http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.30.7319
 *
 * Adapted from Dawid Weiss's implementation at
 * https://github.com/carrotsearch/langid-java/blob/master/langid-java/src/main/java/com/carrotsearch/labs/langid/DoubleLinkedCountingSet.java
 *
 * Marco Lui, July 2014
 */
#include <stdlib.h>
#include "sparseset.h"

Set *alloc_set(size_t size){
    Set *s;
    if ( (void *)(s = (Set *) malloc(sizeof(Set))) == 0 ) exit(-1);

    s->members=0;
    if ( (void *)(s->sparse = (unsigned *) malloc(size * sizeof(unsigned))) == 0 ) exit(-1);
    if ( (void *)(s->dense  = (unsigned *) malloc(size * sizeof(unsigned))) == 0 ) exit(-1);
    if ( (void *)(s->counts = (unsigned *) malloc(size * sizeof(unsigned))) == 0 ) exit(-1);

    return s;
}

void free_set(Set * s){
    free(s->sparse);
    free(s->dense);
    free(s->counts);
    free(s);
}

void clear(Set *s) {
    s->members = 0;
} 

unsigned get(Set *s, unsigned key) {
    unsigned index = s->sparse[key];
    if (index < s->members && s->dense[index] == key) {
        return s->counts[index];
    }
    else {
        return 0;
    }
}

void add(Set *s, unsigned key, unsigned val){
    unsigned index = s->sparse[key];
    if (index < s->members && s->dense[index] == key) {
        s->counts[index] += val;
    }
    else {
        index = s->members++;
        s->sparse[key] = index;
        s->dense[index] = key;
        s->counts[index] = val;
    }
}
