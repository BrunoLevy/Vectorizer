#include "CDT_2d.h"
#include "ST_NICCC/io.h"

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

bool constraint_is_degenerate(
    const GEO::CDT2di& triangulation,
    std::vector<GEO::index_t>& vertices
) {
    for(int i=0; i<vertices.size(); ++i) {
        for(int j=i+1; j<vertices.size(); ++j) {
            for(int k=j+1; k<vertices.size(); ++k) {
                if(
                    triangulation.orient2d(
                        vertices[i], vertices[j], vertices[k]
                    ) != GEO::ZERO
                ) {
                    return false;
                }
            }
        }
    }
    return true;
}
    
// Parse .fig file
// Reference: https://mcj.sourceforge.net/fig-format.html
bool fig_2_ST_NICCC(const std::string& filename, ST_NICCC_IO* io) {

    GEO::CDT2di triangulation;
    
    int xmin = 4048;
    int xmax = 6152;
    int ymin = 5811;
    int ymax = 7389;
    int L = xmax-xmin;
    
    triangulation.clear();
    triangulation.create_enclosing_rectangle(0,0,255,255);
    
    std::ifstream in(filename);
    if(!in) {
        return false;
    }
    std::cerr << "Loading " << filename << std::endl;
    int nb_paths = 0;

    // Read xfig file and send contents to constrained Delaunay
    // triangulation
    try {
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
            float style_val;    // 1/80 inch,specification for dash/dotted lines
            int	cap_style;      // enumeration type, only used for open splines
            int	forward_arrow;  // 0: off, 1: on
            int	backward_arrow; // 0: off, 1: on
            int	npoints	;       // number of control points in spline

            int xmin_tmp, ymin_tmp, xmax_tmp, ymax_tmp;
            
            if( // object 3: polyline
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
                    x = std::max(x,xmin);
                    x = std::min(x,xmax);
                    y = std::max(y,ymin);
                    y = std::min(y,ymax);
                    x = (x-xmin)*255/L;
                    y = (y-ymin)*255/L;
                    vertices.push_back(triangulation.insert(GEO::vec2ih(x,y)));
                }
                
                // If all vertices on constraint are aligned, then
                // ignore the constraint, because it would break the
                // in/out rule.
                if(!constraint_is_degenerate(triangulation, vertices)) {
                    for(int i=0; i<npoints; ++i) {
                        triangulation.insert_constraint(
                            vertices[i], vertices[(i+1)%npoints]
                        );
                    }
                }
                ++nb_paths;
            } else if( // object 6: bounding box
                sscanf(
                    line.c_str(),
                    "%d %d %d %d %d",
                    &object_code, &xmin_tmp, &ymin_tmp, &xmax_tmp, &ymax_tmp
                ) == 5 &&
                object_code == 6
            ) {
                xmin = xmin_tmp;
                ymin = ymin_tmp;
                xmax = xmax_tmp;
                ymax = ymax_tmp;
                L = xmax - xmin;
            }
        }
        triangulation.save(filename+"_triangulation.obj");
        triangulation.remove_external_triangles();
        /*
        if(triangulation.nv() > 255) {
            throw(std::logic_error("Too many vertices in frame"));
        }
        */
    } catch(const std::logic_error& e) {
        // I still have a problem with intersecting constraint for
        // integer coordinates (to be fixed).
        // We also have a problem when number of points is greater
        // than 255 (cannot be stored in ST_NICCC file). Well, in
        // fact we could use "direct" mode in that case (TODO...)
        std::cerr << "Exception caught: " << e.what() << std::endl;
        
        // If there was a problem, create an empty frame
        ST_NICCC_FRAME frame;
        st_niccc_frame_init(&frame);
        st_niccc_write_frame(io,&frame);
        st_niccc_write_end_of_frame(io);
        return true;
    }

    // Write data to ST_NICCC file
    ST_NICCC_FRAME frame;
    st_niccc_frame_init(&frame);
    st_niccc_frame_clear(&frame);
    if(triangulation.nv() <= 255) {
        for(GEO::index_t v=0; v<triangulation.nv(); ++v) {
            int x = (triangulation.point(v).x / triangulation.point(v).w);
            int y = (triangulation.point(v).y / triangulation.point(v).w);        
            st_niccc_frame_set_vertex(&frame, v, x, y);
        }
        st_niccc_write_frame(io,&frame);    
        for(GEO::index_t t=0; t<triangulation.nT(); ++t) {
            st_niccc_write_triangle_indexed(
                io,1,triangulation.Tv(t,0),triangulation.Tv(t,1),triangulation.Tv(t,2)
            );
        }
    } else {
        st_niccc_write_frame(io,&frame);
        for(GEO::index_t t=0; t<triangulation.nT(); ++t) {
            int x[3];
            int y[3];
            for(int lv=0; lv<3; ++lv) {
                GEO::index_t v = triangulation.Tv(t,lv);
                x[lv] = (triangulation.point(v).x / triangulation.point(v).w);
                y[lv] = (triangulation.point(v).y / triangulation.point(v).w);
            }
            st_niccc_write_triangle(
                io,1,x[0],y[0],x[1],y[1],x[2],y[2]
            );
        }
    }
    st_niccc_write_end_of_frame(io);
    
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

    ST_NICCC_IO io;

    st_niccc_open(&io,"stream.bin",ST_NICCC_WRITE);
    
    ST_NICCC_FRAME frame;
    st_niccc_frame_init(&frame);
    st_niccc_frame_set_color(&frame, 0,   0,   0,   0);
    st_niccc_frame_set_color(&frame, 1, 255, 255, 255);
    st_niccc_write_frame(&io,&frame);
    st_niccc_write_end_of_frame(&io);
    
    while(fig_2_ST_NICCC(basename+to_string(id,4)+".fig",&io)) {
        ++id;
    }

    st_niccc_frame_init(&frame);
    st_niccc_write_frame(&io,&frame);
    st_niccc_write_end_of_stream(&io);
    
}
