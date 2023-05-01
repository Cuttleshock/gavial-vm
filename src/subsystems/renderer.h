#ifndef SUBSYSTEM_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef RENDERER_H
#define RENDERER_H

#include "deps/glad_gl.h"
#include "src/common.h"

bool init_renderer(int window_width, int window_height, int pixel_scale, GLADloadfunc opengl_loader);
bool bind_palette_impl(uint8_t bind_point, uint8_t target);
bool set_palette_colour_impl(uint8_t palette, uint8_t colour, float r, float g, float b);
bool fill_rect_impl(int x, int y, int w, int h, uint8_t palette, uint8_t colour);
void render();
void close_renderer();

#endif // RENDERER_H
