MODEL := ldpy.model
#CFLAGS := -g -O0 -Wall -DDEBUG
CFLAGS := -Os -Wall
LDLIBS:= -lprotobuf-c  

OBJS:=liblangid model sparseset langid.pb-c

.PHONY: all clean

all: langid

clean:
	rm -f langid ${OBJS:=.o} model.c model.h langid.pb-c.c langid.pb-c.h langid_pb2.py

liblangid.o: langid.pb-c.h model.h

model.o: model.h

model.h: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py --header $< -o $@

model.c: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py $< -o $@

langid: langid.c ${OBJS:=.o} liblangid.h model.h sparseset.h langid.pb-c.h

langid_pb2.py: langid.proto
	protoc --python_out=. $<

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=. $<

%.pmodel: %.model langid_pb2.py ldpy2ldc.py
	python ldpy2ldc.py --protobuf -o $@ $<
