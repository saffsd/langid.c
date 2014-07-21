MODEL := acquis.model
CFLAGS := -g

.PHONY: all

all: langid

model.h: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py --header $< -o $@

model.c: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py $< -o $@

langid: langid.c model.o sparseset.o model.h sparseset.h
