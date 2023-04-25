#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <stdio.h>

/* Uncomment one of them or define on command line */
// #define GFX_BACKEND_ANSI
// #define GFX_BACKEND_GLFW

extern int gfx_wireframe;

void gfx_init();
void gfx_swapbuffers();
void gfx_clear();
void gfx_setcolor(int r, int g, int b);
void gfx_line(int x1, int y1, int x2, int y2);
void gfx_fillpoly(int nb_pts, int* points);

#endif
