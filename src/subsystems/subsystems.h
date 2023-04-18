#ifndef SUBSYSTEMS_H
#define SUBSYSTEMS_H

#include <stdbool.h>
#include <stdint.h>

bool init_subsystems(int window_width, int window_height, const char *title);
void input();
bool bind_palette(uint8_t bind_point, uint8_t target);
bool set_palette_colour(uint8_t palette, uint8_t colour, float r, float g, float b);
bool fill_rect(int x, int y, int w, int h, uint8_t palette, uint8_t colour);
void draw();
bool window_should_close();
void close_subsystems();

#endif // SUBSYSTEMS_H
