#include "CDT_2d.h"
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>

std::string to_string(int i, int len) {
    std::ostringstream out; 
    out << i;
    std::string result = out.str();
    while(int(result.length()) < len) {
        result = "0" + result;
    }
    return result;
}

// Parse .fig file
// Reference: https://mcj.sourceforge.net/fig-format.html

bool load_file(const std::string& filename, GEO::CDT2di& triangulation) {

    const int xmin =  2850;
    const int xmax =  7350;
    const int ymin = -8288;
    const int ymax = -4913;
    const int L = xmax-xmin;
    
    triangulation.clear();
    triangulation.create_enclosing_rectangle(0,0,255,255);
    
    /*
    triangulation.create_enclosing_quad(
        GEO::vec2ih(xmin,ymin),
        GEO::vec2ih(xmax,ymin),
        GEO::vec2ih(xmax,ymax),
        GEO::vec2ih(xmin,ymax)
    );
    */
    
    std::ifstream in(filename);
    if(!in) {
        return false;
    }
    std::cerr << "Loading " << filename << std::endl;

    int nb_paths = 0;
    
    std::string line;
    while(std::getline(in,line)) {
        int	object_code;    // always 3
        int	sub_type;       // 0: open approximated spline
                                // 1: closed approximated spline
	                        // 2: open   interpolated spline
	                        // 3: closed interpolated spline
	                        // 4: open   x-spline
	                        // 5: closed x-spline
	int	line_style;     // enumeration type, solid, dash, dotted, etc.
	int	thickness;      // 1/80 inch
	int	pen_color;      // enumeration type, pen color
	int	fill_color;     // enumeration type, fill color
	int	depth;          // enumeration type
	int	pen_style;      // pen style, not used
	int	area_fill;      // enumeration type, -1 = no fill
	float	style_val;      // 1/80 inch,specification for dash/dotted lines
	int	cap_style;      // enumeration type, only used for open splines
	int	forward_arrow;  // 0: off, 1: on
	int	backward_arrow; // 0: off, 1: on
	int	npoints	;       // number of control points in spline
        if(
            sscanf(
                line.c_str(),
                "%d %d %d %d %d %d %d %d %d %f %d %d %d %d",
                &object_code, &sub_type, &line_style, &thickness,
                &pen_color, &fill_color, &depth, &pen_style, &area_fill,
                &style_val, &cap_style, &forward_arrow, &backward_arrow,
                &npoints
            ) == 14 &&
            object_code==3
        ) {
            std::vector<GEO::index_t> vertices;
            for(int i=0; i<npoints; ++i) {
                std::getline(in,line);
                int x,y;
                if(sscanf(line.c_str(), "%d %d", &x, &y) != 2) {
                    std::cerr << "error while loading "
                              << filename
                              << std::endl;
                    exit(-1);
                }
                y = -y;
                x = std::max(x,xmin);
                x = std::min(x,xmax);
                y = std::max(y,ymin);
                y = std::min(y,ymax);

                x = (x-xmin)*255/L;
                y = (y-ymin)*255/L;
                
                vertices.push_back(triangulation.insert(GEO::vec2ih(x,y)));
            }
            for(int i=0; i<npoints; ++i) {
                triangulation.insert_constraint(
                    vertices[i], vertices[(i+1)%npoints]
                );
            }
            ++nb_paths;
        }
    }
    std::cerr << "Loaded " << nb_paths << " paths" << std::endl;
    return true;
}

int main(int argc, char** argv) {
    if(argc != 2) {
        std::cerr << argv[0] << ": invalid number of arguments"
                  << std::endl;
        exit(-1);
    }
    std::string basename = argv[1];
    int id=1;
    GEO::CDT2di triangulation;
    while(load_file(basename+to_string(id,4)+".fig",triangulation)) {
        triangulation.remove_external_triangles();
        triangulation.save(basename+to_string(id,4)+"_triangulation.obj");
        ++id;
    }
}
