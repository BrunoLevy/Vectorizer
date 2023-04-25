#include "graphics.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#ifdef __linux__
#include <unistd.h>
#endif

#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

int gfx_wireframe = 0;
int gfx_color = 0;

#define GFX_SIZE 256

/************************************************************/

#ifdef GFX_BACKEND_GLFW

#include <GLFW/glfw3.h>
GLFWwindow* gfx_window_;
unsigned short gfx_framebuffer_[GFX_SIZE*GFX_SIZE];

static inline void gfx_setpixel_internal(int x, int y) {
    assert(x >= 0 && x < GFX_SIZE);
    assert(y >= 0 && y < GFX_SIZE);    
    gfx_framebuffer_[(GFX_SIZE-1-y)*GFX_SIZE+x] = gfx_color;
}

static inline void gfx_hline_internal(int x1, int x2, int y) {
    assert(x1 >= 0 && x1 < GFX_SIZE);
    assert(x2 >= 0 && x2 < GFX_SIZE);    
    assert(y >= 0 && y < GFX_SIZE);
    if(x2 < x1) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    for(int x=x1; x<x2; ++x) {
        gfx_setpixel_internal(x,y);
    }
}

void gfx_init() {
    if(!glfwInit()) {
        fprintf(stderr,"Could not initialize glfw\n");
        exit(-1);
    }
    glfwWindowHint(GLFW_RESIZABLE,GL_FALSE);
    gfx_window_  = glfwCreateWindow(
        GFX_SIZE*4,GFX_SIZE*4,"ST_NICCC",NULL,NULL
    );
    glfwMakeContextCurrent(gfx_window_);
    glfwSwapInterval(1);
    glPixelZoom(4.0f,4.0f);
}

void gfx_swapbuffers() {
    glRasterPos2f(-1.0f,-1.0f);
    glDrawPixels(
        GFX_SIZE, GFX_SIZE, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
        gfx_framebuffer_
    );
    glfwSwapBuffers(gfx_window_);
#ifdef __linux__       
    usleep(20000);
#endif       
}

void gfx_clear() {
    memset(gfx_framebuffer_, 0, GFX_SIZE*GFX_SIZE*2);
}

void gfx_setcolor(int r, int g, int b) {
    int r3 = r>>5;
    int g3 = g>>5;
    int b3 = b>>5;
    gfx_color = (b3 << 2) | (g3 << 8) | (r3 << 13);
}

#endif

/************************************************************/

#ifdef GFX_BACKEND_ANSI

static inline void gfx_setpixel_internal(int x, int y) {
    assert(x >= 0 && x < GFX_SIZE);
    assert(y >= 0 && y < GFX_SIZE);    
    x = x >> 1;
    y = y >> 2;
    printf("\033[%d;%dH ",y,x); // move cursor to x,y and print space
}

static inline void gfx_hline_internal(int x1, int x2, int y) {
    assert(x1 >= 0 && x1 < GFX_SIZE);
    assert(x2 >= 0 && x2 < GFX_SIZE);    
    assert(y >= 0 && y < GFX_SIZE);
    x1 = x1 >> 1;
    x2 = x2 >> 1;   
    y  = y  >> 2;
    if(x2 < x1) {
        int tmp = x1;
        x1 = x2;
        x2 = tmp;
    }
    printf("\033[%d;%dH",y,x1); // move cursor to x1,y
    for(int x=x1; x<x2; ++x) {
       putchar(' ');
    }
}

void gfx_init() {
    gfx_wireframe = 0;
    gfx_clear();
    printf("\x1B[?25l"); // hide cursor
}

void gfx_swapbuffers() {
    fflush(stdout);
#ifdef __linux__       
        usleep(20000);
#endif       
}

void gfx_clear() {
    printf("\033[48;5;16m"   /* set background color black */
           "\033[2J");       /* clear screen */
    gfx_color = 0;
}

void gfx_setcolor(int r, int g, int b) {
    int r3 = r>>5;
    int g3 = g>>5;
    int b3 = b>>5;
    // Re-encode r,g,b as ANSI 8-bits color
    b3 = b3 * 6 / 8;
    g3 = g3 * 6 / 8;
    r3 = r3 * 6 / 8;
    int new_color = 16 + b3 + 6*(g3 + 6*r3);
    if(new_color != gfx_color) {
        printf("\033[48;5;%dm",new_color);
    }
    gfx_color = new_color;
}

#endif

/************************************************************/

void gfx_line(int x1, int y1, int x2, int y2) {
    int x,y,dx,dy,sy,tmp;

    /* Swap both extremities to ensure x increases */
    if(x2 < x1) {
       tmp = x2;
       x2 = x1;
       x1 = tmp;
       tmp = y2;
       y2 = y1;
       y1 = tmp;
    }
   
    /* Bresenham line drawing */
    dy = y2 - y1;
    sy = 1;
    if(dy < 0) {
	sy = -1;
	dy = -dy;
    }

    dx = x2 - x1;
   
    x = x1;
    y = y1;
    
    if(dy > dx) {
	int ex = (dx << 1) - dy;
	for(int u=0; u<dy; u++) {
	    gfx_setpixel_internal(x,y);
	    y += sy;
	    if(ex >= 0)  {
		x++;
		ex -= dy << 1;
		gfx_setpixel_internal(x,y);
	    }
	    while(ex >= 0)  {
		x++;
		ex -= dy << 1;
	        putchar(' ');
	    }
	    ex += dx << 1;
	}
    } else {
	int ey = (dy << 1) - dx;
	for(int u=0; u<dx; u++) {
	    gfx_setpixel_internal(x,y);
	    x++;
	    while(ey >= 0) {
		y += sy;
		ey -= dx << 1;
		gfx_setpixel_internal(x,y);
	    }
	    ey += dy << 1;
	}
    }
}

void gfx_fillpoly(int nb_pts, int* points) {
    int x_left[GFX_SIZE];
    int x_right[GFX_SIZE];

    /* Determine clockwise, miny, maxy */
    int clockwise = 0;
    int miny =  1024;
    int maxy = -1024;
    
    for(int i1=0; i1<nb_pts; ++i1) {
	int i2=(i1==nb_pts-1) ? 0 : i1+1;
	int i3=(i2==nb_pts-1) ? 0 : i2+1;
	int x1 = points[2*i1];
	int y1 = points[2*i1+1];
	int dx1 = points[2*i2]   - x1;
	int dy1 = points[2*i2+1] - y1;
	int dx2 = points[2*i3]   - x1;
	int dy2 = points[2*i3+1] - y1;
	clockwise += dx1 * dy2 - dx2 * dy1;
	miny = MIN(miny,y1);
	maxy = MAX(maxy,y1);
    }

    /* Determine x_left and x_right for each scaline */
    for(int i1=0; i1<nb_pts; ++i1) {
	int i2=(i1==nb_pts-1) ? 0 : i1+1;

	int x1 = points[2*i1];
	int y1 = points[2*i1+1];
	int x2 = points[2*i2];
	int y2 = points[2*i2+1];

        if(gfx_wireframe) {
            gfx_line(x1,y1,x2,y2);
	    continue;
	}
       
	int* x_buffer = ((clockwise > 0) ^ (y2 > y1)) ? x_left : x_right;
	int dx = x2 - x1;
	int sx = 1;
	int dy = y2 - y1;
	int sy = 1;
	int x = x1;
	int y = y1;
	int ex;
	
	if(dx < 0) {
	    sx = -1;
	    dx = -dx;
	}
	
	if(dy < 0) {
	    sy = -1;
	    dy = -dy;
	}

	if(y1 == y2) {
	   x_left[y1]  = MIN(x1,x2);
	   x_right[y1] = MAX(x1,x2);
	   continue;
	}

	ex = (dx << 1) - dy;

	for(int u=0; u <= dy; ++u) {
    	    x_buffer[y] = x; 
	    y += sy;
	    while(ex >= 0) {
		x += sx;
		ex -= dy << 1;
	    }
	    ex += dx << 1;
	}
    }

    if(!gfx_wireframe) {
	for(int y = miny; y <= maxy; ++y) {
            gfx_hline_internal(
                x_left[y], x_right[y], y
            );
	}
    }
}

