/*
 * Reading the ST-NICCC megademo data 
 */

#include "graphics.h"
#include <stdint.h>
#include <stdlib.h>

/**********************************************************************************/

/*
 * Starting address of data stream stored in the 
 * SPI.
 * I put the data stream starting from 1M offset,
 * just to make sure it does not collide with
 * FPGA wiring configuration ! (but FPGA configuration
 * only takes a few tenth of kilobytes I think).
 * Using the IO interface, it is using the physical address
 *  (starting at 1M). Using the mapped memory interface,
 *  SPI_FLASH_BASE is mapped to 1M.
 */
uint32_t spi_addr = 0;

/*
 * Word address and cached word used in mapped mode
 */
uint32_t spi_word_addr = 0;
union {
  uint32_t spi_word;
  uint8_t spi_bytes[4];
} spi_u;

#define ADDR_OFFSET 1024*1024

/*
 * Restarts reading from the beginning of the stream.
 */
void spi_reset() {
  spi_addr = ADDR_OFFSET;
  spi_word_addr = (uint32_t)(-1);
}


#ifdef __linux__

FILE* f = NULL;

/**
 * Reads one byte of data from the file (emulates read_spi_byte() when running on desktop)
 */
uint8_t next_spi_byte() {
   uint8_t result;
   if(f == NULL) {
      f = fopen("scene1.bin","rb");
      if(f == NULL) {
	 printf("Could not open data file\n");
	 exit(-1);
      }
   }
   if(spi_word_addr != spi_addr >> 2) {
      spi_word_addr = spi_addr >> 2;
      fseek(f, spi_word_addr*4-ADDR_OFFSET, SEEK_SET);
      fread(&(spi_u.spi_word), 4, 1, f);
   }
   result = spi_u.spi_bytes[spi_addr&3];
   ++spi_addr;
   return (uint8_t)(result);
}

#else


# define SPI_FLASH_BASE ((uint32_t*)(1 << 23))

/**
 * Reads one byte from the SPI flash, using the mapped SPI flash interface.
 */
static inline uint8_t next_spi_byte() {
   uint8_t result;
   if(spi_word_addr != spi_addr >> 2) {
      spi_word_addr = spi_addr >> 2;
      spi_u.spi_word = SPI_FLASH_BASE[spi_word_addr];
   }
   result = spi_u.spi_bytes[spi_addr&3];
   ++spi_addr;
   return (uint8_t)(result);
}

#endif

static inline uint16_t next_spi_word() {
   /* In the ST-NICCC file,  
    * words are stored in big endian format.
    * (see DATA/scene_description.txt).
    */
   uint16_t hi = (uint16_t)next_spi_byte();    
   uint16_t lo = (uint16_t)next_spi_byte();
   return (hi << 8) | lo;
}

/* 
 * The colormap, encoded in such a way that it
 * can be directly sent as ANSI color codes.
 */
int cmap_r[16];
int cmap_g[16];
int cmap_b[16];

/*
 * Current frame's vertices coordinates (if frame is indexed),
 *  mapped to OLED display dimensions (divide by 2 from file).
 */
uint8_t  X[255];
uint8_t  Y[255];

/*
 * Current polygon vertices, as expected
 * by gfx_fillpoly():
 * xi = poly[2*i], yi = poly[2*i+1]
 */
int      poly[30];

/*
 * Masks for frame flags.
 */
#define CLEAR_BIT   1
#define PALETTE_BIT 2
#define INDEXED_BIT 4

/*
 * Reads a frame's polygonal description from
 * SPI flash and rasterizes the polygons using
 * FemtoGL.
 * returns 0 if last frame.
 *   See DATA/scene_description.txt for the 
 * ST-NICCC file format.
 *   See DATA/test_ST_NICCC.c for an example
 * program.
 */
int read_frame();
int read_frame() {
    uint8_t frame_flags = next_spi_byte();

    // Update palette data.
    if(frame_flags & PALETTE_BIT) {
	uint16_t colors = next_spi_word();
	for(int b=15; b>=0; --b) {
	    if(colors & (1 << b)) {
		int rgb = next_spi_word();
		// Get the three 3-bits per component R,G,B
	        int b3 = (rgb & 0x007);
		int g3 = (rgb & 0x070) >> 4;
		int r3 = (rgb & 0x700) >> 8;
                cmap_r[15-b] = r3 << 5;
                cmap_g[15-b] = g3 << 5;
                cmap_b[15-b] = b3 << 5;                
	    }
	}
    }

    if(frame_flags & CLEAR_BIT) {
        gfx_clear(); 
    }

    // Update vertices
    if(frame_flags & INDEXED_BIT) {
	uint8_t nb_vertices = next_spi_byte();
	for(int v=0; v<nb_vertices; ++v) {
	    X[v] = next_spi_byte();
	    Y[v] = next_spi_byte();
	}
    }

    // Draw frame's polygons
    for(;;) {
	uint8_t poly_desc = next_spi_byte();

	// Special polygon codes (end of frame,
	// seek next block, end of stream)
	
	if(poly_desc == 0xff) {
	    break; // end of frame
	}
	if(poly_desc == 0xfe) {
	    // Go to next 64kb block
	    spi_addr -= ADDR_OFFSET;
	    spi_addr &= ~65535;
	    spi_addr +=  65536;
	    spi_addr += ADDR_OFFSET;
	    return 1; 
	}
	if(poly_desc == 0xfd) {
	    return 0; // end of stream
	}
	
	uint8_t nvrtx = poly_desc & 15;
	uint8_t poly_col = poly_desc >> 4;
	for(int i=0; i<nvrtx; ++i) {
	    if(frame_flags & INDEXED_BIT) {
		uint8_t index = next_spi_byte();
		poly[2*i]   = X[index];
		poly[2*i+1] = Y[index];
	    } else {
		poly[2*i]   = next_spi_byte();
		poly[2*i+1] = next_spi_byte();
	    }
	}
        gfx_setcolor(
            cmap_r[poly_col],
            cmap_g[poly_col],
            cmap_b[poly_col]
        );
	gfx_fillpoly(nvrtx,poly);
    }
    return 1; 
}


int main() {
    gfx_init();
    for(;;) {
        spi_reset();
	while(read_frame()) {
            gfx_swapbuffers();
            if(gfx_wireframe) {
                gfx_clear();
            }
	}
        gfx_wireframe = !gfx_wireframe;
    }
}
