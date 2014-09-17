from distutils.core import setup, Extension

langid = Extension("_langid", 
                   language = 'c',
                   libraries = ['protobuf-c'],
                   sources = ["_langid.c", "liblangid.c", "model.c", "sparseset.c", "langid.pb-c.c"],
                   )

setup(
    ext_modules=[langid],
)
