#ifndef SUBSYSTEMS_H
#define SUBSYSTEMS_H

#include "src/common.h"
#include "src/value.h"

bool init_subsystems();
void input();
bool fill_rect(GvmConstant position, GvmConstant scale, uint8_t palette, uint8_t colour);
void draw();
bool window_should_close();
void close_subsystems();

#endif // SUBSYSTEMS_H
