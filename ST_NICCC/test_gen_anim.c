#include "io.h"
#include <math.h>

ST_NICCC_IO io;
ST_NICCC_FRAME frame;

int main() {
    st_niccc_open(&io, "test.bin", ST_NICCC_WRITE);
    st_niccc_frame_init(&frame);
    st_niccc_frame_clear(&frame);
    st_niccc_frame_set_color(&frame, 0, 0,   0,   0);
    st_niccc_frame_set_color(&frame, 1, 0, 255,   0);
    st_niccc_frame_set_color(&frame, 2, 0,   0, 255);
    st_niccc_write_frame(&io, &frame);
    st_niccc_write_end_of_frame(&io);
    
    int N = 20;
    int Nframe = 100;
    
    for(int f=0; f<Nframe; ++f) {
        st_niccc_frame_init(&frame);
        st_niccc_frame_set_vertex(&frame, 0, 128, 128);
        for(int i=0; i<N; ++i) {
            double alpha =
                (double)(f)*0.03 + (double)(i) * M_PI * 2.0 / (double)(N);
            double c = cos(alpha);
            double s = sin(alpha);
            int x = (int)(128.0 + c*30);
            int y = (int)(128.0 + s*30);
            st_niccc_frame_set_vertex(&frame,i+1,x,y);
        }
        st_niccc_write_frame(&io, &frame);
        for(int i=0; i<N; ++i) {
            st_niccc_write_triangle(
                &io, (i&1)+1, 0, 1+i, 1+((i+1)%N)
            );
        }
        if(f == Nframe-1) {
            st_niccc_write_end_of_stream(&io);
        } else {
            st_niccc_write_end_of_frame(&io);
        }
    }
    st_niccc_close(&io);
}
