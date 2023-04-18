#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define SUBSYSTEM_IMPL
#include "window.h"

static GLFWwindow *window = NULL;

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	switch (key) {
		case GLFW_KEY_ESCAPE:
			glfwSetWindowShouldClose(window, GLFW_TRUE);
			break;
		default:
			break;
	}
}

bool window_should_close_impl()
{
	return glfwWindowShouldClose(window);
}

bool init_window(int window_width, int window_height, const char *title)
{
	// TODO: More error checking
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(window_width, window_height, title, NULL, NULL);
	if (!window) {
		return false;
	}
	glfwMakeContextCurrent(window);
	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSwapInterval(1);

	return true;
}

void input_impl()
{
	glfwPollEvents();
}

void swap_buffers()
{
	glfwSwapBuffers(window);
}

void close_window()
{
	glfwDestroyWindow(window);
}
