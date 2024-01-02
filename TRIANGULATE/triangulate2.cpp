
#include "Delaunay_psm.h"
#include "ST_NICCC/io.h"

#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <vector>
#include <cmath>


namespace GEO {


    class Triangulation : public ExactCDT2d {
    public:
        index_t insert(double x, double y) {
            return ExactCDT2d::insert(exact::vec2h(x,y,1.0));
        }
        
        void classify() {
            classify_triangles("union",true); // classify only
            T_region_.assign(nT(),-1);
            for(index_t t=0; t<nT(); ++t) {
                T_region_[t] = Tflag_is_set(t,T_MARKED_FLAG) ? 1 : 0;
                Treset_flag(t, T_MARKED_FLAG);
            }
        }
        
        int get_x(index_t v) const {
            double x = point(v).x.estimate();
            double w = point(v).w.estimate();
            return int(x/w);
        }
        
        int get_y(index_t v) const {
            double y = point(v).y.estimate();
            double w = point(v).w.estimate();
            return int(y/w);
        }

        int Tregion(index_t t) const {
            return T_region_[t];
        }

        bool Tis_marked(index_t t) {
            return Tflag_is_set(t,T_MARKED_FLAG);
        }

        /**
         * \brief Greedely merge triangles that have the same
         *  color while they form a convex polygon.
         */
        void get_convex_polygon(
            index_t t, std::vector<GEO::index_t>& P
        ) {
            P.resize(0);
            P.push_back(Tv(t,0));
            P.push_back(Tv(t,1));
            P.push_back(Tv(t,2));
            DList S(*this, DLIST_S_ID);
            Tset_flag(t, T_MARKED_FLAG);
            S.push_back(t);

            while(!S.empty() && P.size()<15) {
                index_t t1 = S.front();
                S.pop_front();
                for(index_t le1=0; (le1<3 && P.size()<15); ++le1) {
                    index_t t2 = Tadj(t1,le1);

                    if(t2 == index_t(-1)) {
                        continue;
                    }

                if(Tflag_is_set(t2,T_MARKED_FLAG)) {
                    continue;
                }
                
                if(Tregion(t1) != Tregion(t2)) {
                    continue;
                }
                
                index_t le2 = Tadj_find(t2,t1);
                index_t v1 = Tv(t1,(le1+1)%3);
                index_t v2 = Tv(t1,(le1+2)%3);                
                index_t v3 = Tv(t2,le2);

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
                    orient2d(P[i1_prev],P[i1],v3) >= 0 &&
                    orient2d(v3,P[i2],P[i2_next]) >= 0 
                ) {
                    Tset_flag(t2,T_MARKED_FLAG);
                    S.push_back(t2);
                    P.insert(P.begin()+i2,v3);
                }
                }
            }
        }
        
    private:
        vector<int> T_region_;
    };
    
}


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


// Parse .fig file and append content to ST_NICCC file
// Reference: https://mcj.sourceforge.net/fig-format.html
bool fig_2_ST_NICCC(const std::string& filename, ST_NICCC_IO* io) {

    GEO::Triangulation triangulation;
    triangulation.set_delaunay(true);
    
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
                    vertices.push_back(triangulation.insert(x,y));
                }
                for(int i=0; i<npoints; ++i) {
                    triangulation.insert_constraint(
                        vertices[i], vertices[(i+1)%npoints], 1
                    );
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
        GEO::Logger::out("Triangulation") << "Saving to "
                                          << filename
                                          << "_triangulation.obj"
                                          << std::endl;

        triangulation.classify();
        //triangulation.save(filename+"_triangulation.obj");
    } 

    // Write data to ST_NICCC file
    ST_NICCC_FRAME frame;
    st_niccc_frame_init(&frame);
    // st_niccc_frame_clear(&frame); Not needed, we clear the frame

    if(triangulation.nv() <= 255) {
        for(GEO::index_t v=0; v<triangulation.nv(); ++v) {
            int x = triangulation.get_x(v);
            int y = triangulation.get_y(v);
            st_niccc_frame_set_vertex(&frame, v, x, y);
        }
        st_niccc_write_frame_header(io,&frame);

        std::vector<GEO::index_t> P;
        uint8_t P8[15];
        for(GEO::index_t t=0; t<triangulation.nT(); ++t) {
            if(!triangulation.Tis_marked(t)) {
                uint8_t color = uint8_t(triangulation.Tregion(t));
                triangulation.get_convex_polygon(t, P);
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
            if(!triangulation.Tis_marked(t)) {
                uint8_t color = uint8_t(triangulation.Tregion(t));
                triangulation.get_convex_polygon(t, P);
                for(int i=0; i<int(P.size()); ++i) {
                    GEO::index_t v = P[i];
                    x[i] = uint8_t(triangulation.get_x(v));
                    y[i] = uint8_t(triangulation.get_y(v));
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
    GEO::initialize();
    GEO::CmdLine::import_arg_group("standard");
    GEO::CmdLine::import_arg_group("algo");

    GEO::CmdLine::set_arg("sys:assert","abort");
    
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
