/*
 * Player for ST-NICCC 
 */

#include "graphics.h"
#include "io.h"
#include <stdlib.h>

ST_NICCC_IO io;
ST_NICCC_FRAME frame;
ST_NICCC_POLYGON polygon;

int main(int argc, char** argv) {
    const char* scene_file = "scene1.bin";
    if(argc == 2) {
        scene_file = argv[1];
    }
    if(!st_niccc_open(&io,scene_file,ST_NICCC_READ)) {
        fprintf(stderr,"could not open data file\n");
        exit(-1);
    }
    gfx_init();
    for(;;) {
        st_niccc_rewind(&io);
	while(st_niccc_read_frame(&io,&frame)) {
            if(gfx_wireframe || frame.flags & CLEAR_BIT) {
                gfx_clear();
            }
            while(st_niccc_read_polygon(&io,&frame,&polygon)) {
                uint8_t color = polygon.color;
                // gfx_setcolor(255,255,255);
                gfx_setcolor(
                    frame.cmap_r[color],
                    frame.cmap_g[color],
                    frame.cmap_b[color]
                );
                gfx_fillpoly(
                    polygon.nb_vertices,
                    polygon.XY
                );
            }
            gfx_swapbuffers();
	}
        gfx_wireframe = !gfx_wireframe;
    }
}
