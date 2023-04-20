#ifndef SUBSYSTEM_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef WINDOW_H
#define WINDOW_H

#include "src/common.h"

bool window_should_close_impl();
bool init_window(int window_width, int window_height, const char *title);
void input_impl();
bool button_pressed_impl(int button);
void swap_buffers();
void close_window();

#endif // WINDOW_H
