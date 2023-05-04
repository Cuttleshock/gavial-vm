#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "src/common.h"

#define SUBSYSTEM_IMPL
#include "subsystems.h"
#include "renderer.h"
#include "window.h"

FileCb on_file_drop = NULL;

static void glfw_error_callback(int error, const char *message)
{
	gvm_error("GLFW E%4d: %s\n", error, message);
}

static void glfw_drop_callback(GLFWwindow *window, int path_count, const char *paths[])
{
	glfwFocusWindow(window);

	if (NULL == on_file_drop) {
		return;
	}

	for (int i = 0; i < path_count; ++i) {
		// Return value ignored - it's not obviously better to quit if one fails
		on_file_drop(paths[i]);
	}
}

bool init_subsystems(int window_width, int window_height, int pixel_scale, const char *title, FileCb file_cb, void (*save_cb)(void))
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return false;
	}

	on_file_drop = file_cb;
	if (!init_window(window_width * pixel_scale, window_height * pixel_scale, title, glfw_drop_callback, save_cb)) {
		glfwTerminate();
		return false;
	}

	if (!init_renderer(window_width, window_height, pixel_scale, glfwGetProcAddress)) {
		close_window();
		glfwTerminate();
		return false;
	}

	return true;
}

void input()
{
	input_impl();
}

bool button_pressed(int button)
{
	return button_pressed_impl(button);
}

// Returns: success
bool fill_rect(int x, int y, int w, int h, uint8_t palette, uint8_t colour)
{
	return fill_rect_impl(x, y, w, h, palette, colour);
}

// Returns: success
bool sprite(int x, int y, uint8_t sheet_x, uint8_t sheet_y, uint8_t palette, uint8_t h_flip, uint8_t v_flip)
{
	return sprite_impl(x, y, sheet_x, sheet_y, palette, h_flip, v_flip);
}

// Returns: success
bool bind_palette(uint8_t bind_point, uint8_t target)
{
	return bind_palette_impl(bind_point, target);
}

void set_camera(int x, int y)
{
	set_camera_impl(x, y);
}

// Returns: success
bool set_palette_colour(uint8_t palette, uint8_t colour, float r, float g, float b)
{
	return set_palette_colour_impl(palette, colour, r, g, b);
}

// Returns: success
bool define_sprite_row(const uint8_t *data, int row)
{
	return define_sprite_row_impl(data, row);
}

// Returns: success
bool register_map(const uint8_t (*map)[4], int width, int height)
{
	return register_map_impl(map, width, height);
}

void draw()
{
	render();
	swap_buffers();
}

// Returns: true if close has been requested by the OS
bool window_should_close()
{
	return window_should_close_impl();
}

void close_subsystems()
{
	close_renderer();
	close_window();
	glfwTerminate();
}
