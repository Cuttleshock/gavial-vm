#ifndef SUBSYSTEM_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef WINDOW_H
#define WINDOW_H

#include "src/common.h"

bool window_should_close_impl();
bool init_window();
void input_impl();
void swap_buffers();
void close_window();

#endif // WINDOW_H
