#ifndef SUBSYSTEMS_H
#define SUBSYSTEMS_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*FileCb)(const char *path);

// TODO: The only real reason other headers are hidden is for the convenience
// of draw(). Suggest removing this indirection.
bool init_subsystems(int window_width, int window_height, int pixel_scale, const char *title, FileCb file_cb, void (*save_cb)(void));
void input();
bool button_pressed(int button);
bool bind_palette(uint8_t bind_point, uint8_t target);
void set_camera(int x, int y);
bool set_palette_colour(uint8_t palette, uint8_t colour, float r, float g, float b);
bool define_sprite_row(const char *data, int row);
bool fill_rect(int x, int y, int w, int h, uint8_t palette, uint8_t colour);
bool sprite(int x, int y, uint8_t sheet_x, uint8_t sheet_y, uint8_t palette);
void draw();
bool window_should_close();
void close_subsystems();

#endif // SUBSYSTEMS_H
