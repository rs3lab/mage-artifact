# Setup Instructions

- Build metis (compile should fail with an error, producing a half-empty `obj` dir). 
- Build pkg/libstreamflow using its makefile. 
- Copy the generated libstreamflow.a into the `obj/lib` dir, next to libmetis.a. 
- Build metis with regular `make`. Build should succeed!
