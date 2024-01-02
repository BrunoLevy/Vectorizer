

#CXXFLAGS="-g -Wall -Wpedantic -I../"
#CXXFLAGS="-g -Wall -Wpedantic -DCDT_DEBUG -I../"
CXXFLAGS="-Wall -Wpedantic -O3 -DNDEBUG -I../"

g++ $CXXFLAGS triangulate.cpp Delaunay_psm.cpp ../ST_NICCC/io.c -lm -o triangulate

