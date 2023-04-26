
CXXFLAGS="-g -Wall -Wpedantic"
#CXXFLAGS="-g -Wall -Wpedantic -DCDT_DEBUG"
#CXXFLAGS="-Wall -Wpedantic -DNDEBUG"

g++ $CXXFLAGS triangulate.cpp CDT_2d.cpp -o triangulate

