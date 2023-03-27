#ifndef SUBSYSTEM_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef RENDERER_H
#define RENDERER_H

#include "deps/glad_gl.h"
#include "src/common.h"

bool init_renderer(GLADloadfunc opengl_loader);
bool fill_rect_impl(int x, int y, int w, int h, uint8_t palette, uint8_t colour);
void render();
void close_renderer();

#endif // RENDERER_H
