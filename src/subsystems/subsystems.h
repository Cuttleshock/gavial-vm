#ifndef SUBSYSTEMS_H
#define SUBSYSTEMS_H

#include "src/common.h"

bool init_subsystems();
void input();
void draw();
bool window_should_close();
void close_subsystems();

#endif // SUBSYSTEMS_H
