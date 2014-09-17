MODEL := ldpy.model
#CFLAGS := -g -O0 
CFLAGS := -Os
#CFLAGS := -g -O0 -I/usr/local/include 
#LDLIBS:= -L/usr/local/lib -lprotobuf-c  
LDLIBS:= -lprotobuf-c  

.PHONY: all clean

all: langid

clean:
	rm langid *.o

model.o: model.h

model.h: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py --header $< -o $@

model.c: $(MODEL) ldpy2ldc.py
	python ldpy2ldc.py $< -o $@

langid: langid.c liblangid.o model.o sparseset.o langid.pb-c.o liblangid.h model.h sparseset.h langid.pb-c.h

langid_pb2.py: langid.proto
	protoc --python_out=. $<

%.pb-c.c %.pb-c.h: %.proto
	protoc-c --c_out=. $<

%.pmodel: %.model langid_pb2.py ldpy2ldc.py
	python ldpy2ldc.py --protobuf -o $@ $<
