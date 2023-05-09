#include "CDT_2d.h"
#include "ST_NICCC/io.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>

bool filled = true;

/**
 * \brief Generates a string of length \p len from integer \p i
 *  padded with zeroes.
 */
std::string to_string(int i, int len) {
    std::ostringstream out; 
    out << i;
    std::string result = out.str();
    while(int(result.length()) < len) {
        result = "0" + result;
    }
    return result;
}

namespace GEO {
   /**
    * \brief Tests whether a constraint is degenerate
    * \details A constraint is degenerate if all vertices are 
    *  aligned. Such constraints make the in/out classification
    *  algorithm fail because they do not have an interior.
    * \param[in] triangulation a const reference to the CDT2di
    * \param[in] vertices a vector with the indices of the vertices
    *  on the constraint
    * \retval true if all vertices are aligned
    * \retval false otherwise
    */
    bool constraint_is_degenerate(
        const GEO::CDT2di& triangulation,
        std::vector<GEO::index_t>& vertices
    ) {
        for(int i=0; i<int(vertices.size()); ++i) {
            for(int j=i+1; j<int(vertices.size()); ++j) {
                for(int k=j+1; k<int(vertices.size()); ++k) {
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

    int black_pixel_borders=0;
    
    /**
     * \brief Classifies triangles, keeps only the ones inside the shape
     */
    void classify_triangles(CDT2di& T) {
        
        CDT2di::DList S(T, CDT2di::DLIST_S_ID);
        
        // Step 1: get triangles adjacent to the border
        for(index_t t=0; t<T.nT(); ++t) {
            if(
                (T.Tadj(t,0) == index_t(-1)) ||
                (T.Tadj(t,1) == index_t(-1)) ||
                (T.Tadj(t,2) == index_t(-1))
            ) {
                T.Tset_flag(t, CDT2di::T_MARKED_FLAG);
                if(
                    (T.Tadj(t,0)==index_t(-1)&&!T.Tedge_is_constrained(t,0)) ||
                    (T.Tadj(t,1)==index_t(-1)&&!T.Tedge_is_constrained(t,1)) ||
                    (T.Tadj(t,2)==index_t(-1)&&!T.Tedge_is_constrained(t,2))
                ) {
                    T.Tset_flag(t, CDT2di::T_REGION1_FLAG);
                }
                S.push_back(t);
            }
        }
        
        // Step 2: recursive traversal
        while(!S.empty()) {
            index_t t1 = S.pop_back();
            for(index_t le=0; le<3; ++le) {
                index_t t2 = T.Tadj(t1,le); 
                if(
                    t2 != index_t(-1) &&
                    !T.Tflag_is_set(t2, CDT2di::T_MARKED_FLAG)
                ) {
                    T.Tset_flag(t2,CDT2di::T_MARKED_FLAG);
                    if(
                        T.Tedge_is_constrained(t1,le) ^ 
                        T.Tflag_is_set(t1,CDT2di::T_REGION1_FLAG)
                    ) {
                        T.Tset_flag(t2,CDT2di::T_REGION1_FLAG);
                    }
                    S.push_back(t2);
                }
            }
        }

        double area_black = -double(black_pixel_borders);
        double area_white = 0.0;
        for(index_t t=0; t<T.nT(); ++t) {
            double x[3];
            double y[3];
            for(index_t lv=0; lv<3; ++lv) {
                index_t v = T.Tv(t,lv);
                x[lv] = double(T.point(v).x) / double(T.point(v).w);
                y[lv] = double(T.point(v).y) / double(T.point(v).w);
            }
            double At = 0.5*fabs((x[1]-x[0]*y[2]-y[0])-(x[2]-x[0]*y[1]-y[0]));
            if(T.Tflag_is_set(t,CDT2di::T_REGION1_FLAG)) {
                area_black += At;
            } else {
                area_white += At;                
            }
        }
        bool inverted = (area_white > area_black);
        inverted = false;
        
        // Step 3: mark triangles to be discarded
        for(index_t t=0; t<T.nT(); ++t) {
            if(inverted ^ T.Tflag_is_set(t,CDT2di::T_REGION1_FLAG)) {
                T.Tset_flag(t, CDT2di::T_MARKED_FLAG);
            } else {
                T.Treset_flag(t, CDT2di::T_MARKED_FLAG);
            }
        }

        // Step 4: remove marked triangles
        if(filled) {
            for(index_t t=0; t<T.nT(); ++t) {
                T.Treset_flag(t, CDT2di::T_MARKED_FLAG);
            }
        } else {
            T.remove_marked_triangles();
        }
    }

    /**
     * \brief Greedely merge triangles that have the same
     *  color while they form a convex polygon.
     */
    void get_convex_polygon(
        CDT2di& T, index_t t, std::vector<GEO::index_t>& P
    ) {
        P.resize(0);
        P.push_back(T.Tv(t,0));
        P.push_back(T.Tv(t,1));
        P.push_back(T.Tv(t,2));
        CDT2di::DList S(T, CDT2di::DLIST_S_ID);
        T.Tset_flag(t, CDT2di::T_MARKED_FLAG);
        S.push_back(t);

        while(!S.empty() && P.size()<15) {
            index_t t1 = S.front();
            S.pop_front();
            for(index_t le1=0; (le1<3 && P.size()<15); ++le1) {
                index_t t2 = T.Tadj(t1,le1);

                if(t2 == index_t(-1)) {
                    continue;
                }

                if(T.Tflag_is_set(t2,CDT2di::T_MARKED_FLAG)) {
                    continue;
                }
                
                if(
                    T.Tflag_is_set(t1,CDT2di::T_REGION1_FLAG) !=
                    T.Tflag_is_set(t2,CDT2di::T_REGION1_FLAG)
                ) {
                    continue;
                }
                
                index_t le2 = T.Tadj_find(t2,t1);
                index_t v1 = T.Tv(t1,(le1+1)%3);
                index_t v2 = T.Tv(t1,(le1+2)%3);                
                index_t v3 = T.Tv(t2,le2);

                if(false) {
                    std::cerr << "v1=" << v1
                              << " v2=" << v2
                              << " v3=" << v3 << std::endl;
                    std::cerr << "P=[";
                    for(index_t i=0; i<P.size(); ++i) {
                        std::cerr << P[i] << " ";
                    }
                    std::cerr << "]" << std::endl;
                }
                
                index_t i1 = index_t(
                    std::find(P.begin(), P.end(), v1)-P.begin()
                );
                assert(i1 < P.size());
                index_t i2 = index_t(
                    std::find(P.begin(), P.end(), v2)-P.begin()
                );
                assert(i2 < P.size());
                assert((i1+1)%P.size() == i2);

                index_t i1_prev = (i1+P.size()-1)%P.size();
                index_t i2_next = (i2+1)%P.size();

                if(
                    T.orient2d(P[i1_prev],P[i1],v3) >= 0 &&
                    T.orient2d(v3,P[i2],P[i2_next]) >= 0 
                ) {
                    T.Tset_flag(t2,CDT2di::T_MARKED_FLAG);
                    S.push_back(t2);
                    P.insert(P.begin()+i2,v3);
                }
            }
        }
    }
}

// Parse .fig file and append content to ST_NICCC file
// Reference: https://mcj.sourceforge.net/fig-format.html
bool fig_2_ST_NICCC(const std::string& filename, ST_NICCC_IO* io) {

    GEO::CDT2di triangulation;
    
    int xmin = 0;
    int xmax = 0;
    int ymin = 0;
    int ymax = 0;
    int L = 0;

    static int win_xmin = 1000;
    static int win_xmax = -1;
    static int win_ymin = 1000;
    static int win_ymax = -1;
    
    triangulation.create_enclosing_rectangle(0,0,255,255);
    
    std::ifstream in(filename);
    if(!in) {
        return false;
    }
    std::cerr << "Loading " << filename << std::endl;
    int nb_paths = 0;

    // Read xfig file and send contents to constrained Delaunay
    // triangulation
    {
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
                bool update_win = (win_xmax == -1 && win_ymax == -1);
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

                    if(update_win) {
                        win_xmin = std::min(win_xmin,x);
                        win_xmax = std::max(win_xmax,y);
                        win_ymin = std::min(win_ymin,x);
                        win_ymax = std::max(win_ymax,y);                        
                    }
                    vertices.push_back(triangulation.insert(GEO::vec2ih(x,y)));
                }

                if(update_win) {
                    GEO::black_pixel_borders =
                        (256*256)-(win_xmax-win_xmin)*(win_ymax-win_ymin);
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
        GEO::classify_triangles(triangulation);
    } 

    // Write data to ST_NICCC file
    ST_NICCC_FRAME frame;
    st_niccc_frame_init(&frame);
    if(!filled) {
        st_niccc_frame_clear(&frame);
    }
    if(triangulation.nv() <= 255) {
        for(GEO::index_t v=0; v<triangulation.nv(); ++v) {
            int x = (triangulation.point(v).x / triangulation.point(v).w);
            int y = (triangulation.point(v).y / triangulation.point(v).w);
            st_niccc_frame_set_vertex(&frame, v, x, y);
        }
        st_niccc_write_frame_header(io,&frame);

        std::vector<GEO::index_t> P;
        uint8_t P8[15];
        for(GEO::index_t t=0; t<triangulation.nT(); ++t) {
            if(!triangulation.Tflag_is_set(t,GEO::CDT2di::T_MARKED_FLAG)) {
                uint8_t color = triangulation.Tflag_is_set(
                    t,GEO::CDT2di::T_REGION1_FLAG
                ) ? 0 : 1;
                get_convex_polygon(triangulation, t, P);
                for(int i=0; i<int(P.size()); ++i) {
                    P8[i] = uint8_t(P[i]);
                }
                st_niccc_write_polygon_indexed(
                    io,color,P.size(),P8
                );
            }
        }
    } else {
        st_niccc_write_frame_header(io,&frame);
        uint8_t x[15];
        uint8_t y[15];
        std::vector<GEO::index_t> P;
        for(GEO::index_t t=0; t<triangulation.nT(); ++t) {
            if(!triangulation.Tflag_is_set(t,GEO::CDT2di::T_MARKED_FLAG)) {
                uint8_t color = triangulation.Tflag_is_set(
                    t,GEO::CDT2di::T_REGION1_FLAG
                ) ? 0 : 1;
                get_convex_polygon(triangulation, t, P);
                for(int i=0; i<int(P.size()); ++i) {
                    GEO::index_t v = P[i];
                    x[i] = uint8_t(triangulation.point(v).x / triangulation.point(v).w);
                    y[i] = uint8_t(triangulation.point(v).y / triangulation.point(v).w);
                }
                st_niccc_write_polygon(
                    io,color,P.size(),x,y
                );
            }
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
    st_niccc_write_frame_header(&io,&frame);
    st_niccc_write_end_of_frame(&io);
    
    while(fig_2_ST_NICCC(basename+to_string(id,4)+".fig",&io)) {
        ++id;
    }

    st_niccc_frame_init(&frame);
    st_niccc_write_frame_header(&io,&frame);
    st_niccc_write_end_of_stream(&io);
    
}
