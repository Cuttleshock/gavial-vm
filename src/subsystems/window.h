#ifndef SUBSYSTEM_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef WINDOW_H
#define WINDOW_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "src/common.h"

bool window_should_close_impl();
bool init_window(int window_width, int window_height, const char *title, GLFWdropfun drop_callback);
void input_impl();
bool button_pressed_impl(int button);
void swap_buffers();
void close_window();

#endif // WINDOW_H
