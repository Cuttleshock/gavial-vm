#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define SUBSYSTEM_IMPL
#include "subsystems.h"
#include "renderer.h"
#include "window.h"

static void glfw_error_callback(int error, const char *message)
{
	gvm_error("GLFW E%4d: %s\n", error, message);
}

bool init_subsystems()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return false;
	}

	if (!init_window()) {
		glfwTerminate();
		return false;
	}

	if (!init_renderer(glfwGetProcAddress)) {
		close_window();
		glfwTerminate();
		return false;
	}

	return true;
}

// TODO: See window_should_close(). Also, unclear if it's necessary to run this
// separately at the start of a tick.
void input()
{
	input_impl();
}

// Returns: success
// TODO: See window_should_close().
bool fill_rect(GvmConstant position, GvmConstant scale, uint8_t palette, uint8_t colour)
{
	// TODO: Type checking
	return fill_rect_impl(position.as.vec2[0], position.as.vec2[1], scale.as.vec2[0], scale.as.vec2[1], palette, colour);
}

// Returns: success
// TODO: See window_should_close().
bool bind_palette(uint8_t bind_point, uint8_t target)
{
	return bind_palette_impl(bind_point, target);
}

// Returns: success
// TODO: Should we just have subsystems.h declare set_palette_colour_impl() directly?
bool set_palette_colour(int palette, int colour, float r, float g, float b)
{
	return set_palette_colour_impl(palette, colour, r, g, b);
}

void draw()
{
	render();
	swap_buffers();
}

// Returns: true if program should close
// TODO: The extra level of indirection is necessary to keep the window
// private - is it worth it?
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
