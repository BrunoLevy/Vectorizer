#include "CDT_2d.h"
#include "ST_NICCC/io.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

const int domain_size = 255;


namespace GEO {

    void load(const std::string& filename, GEO::CDT2di& triangulation) {
        triangulation.create_enclosing_rectangle(
            0, 0, domain_size, domain_size
        );
        std::ifstream in(filename);
        if(!in) {
            std::cerr << "Could not open " << filename << std::endl;
            exit(-1);
        }
        std::vector<index_t> vertices;
        std::string line;
        while(std::getline(in,line)) {
            if(line.length() > 1) {
                if(line[0] == 'v') {
                    double x,y,z;
                    sscanf(line.c_str()+1, "%lf %lf %lf", &x, &y, &z);
                    vertices.push_back(
                        triangulation.insert(vec2ih(int(x),int(y)))
                    );
                } else if(line[0] == 'l') {
                    int v1,v2;
                    sscanf(line.c_str()+1, "%d %d", &v1, &v2);
                    std::cerr << v1 << " " << v2 << std::endl;
                    triangulation.insert_constraint(
                        vertices[v1-1], vertices[v2-1]
                    );
                }
            }
        }        
    }
}

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cerr << "usage: test_triangulation input_filename.obj"
                  << std::endl;
        return(-1);
    }
    GEO::CDT2di triangulation;
    triangulation.set_delaunay(true);
    load(argv[1],triangulation);
    triangulation.save("triangulation.obj");
}
