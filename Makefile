MODEL := ldpy.model
CFLAGS := -g -O0

.PHONY: all

all: langid

model.o: model.h

model.h: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py --header $< -o $@

model.c: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py $< -o $@


langid: langid.h langid.c model.o sparseset.o model.h sparseset.h
