
#CXXFLAGS="-g -Wall -Wpedantic -I../"
#CXXFLAGS="-g -Wall -Wpedantic -DCDT_DEBUG -I../"
CXXFLAGS="-Wall -Wpedantic -O3 -DNDEBUG -I../"

g++ $CXXFLAGS test_triangulation.cpp CDT_2d.cpp -o test_triangulation
g++ $CXXFLAGS triangulate.cpp CDT_2d.cpp ../ST_NICCC/io.c -o triangulate
