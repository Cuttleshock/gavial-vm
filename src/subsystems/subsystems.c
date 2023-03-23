#include "../deps/glad_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "common.h"

static GLFWwindow *window = NULL;

static void glfw_error_callback(int error, const char *message)
{
	gvm_error("GLFW E%4d: %s\n", error, message);
}

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

static void render()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);
}

bool init_subsystems()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return false;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(512, 512, "Gavial VM", NULL, NULL);
	if (!window) {
		glfwTerminate();
		return false;
	}
	// TODO: More error checking
	glfwMakeContextCurrent(window);
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, glfw_key_callback);

	return true;
}

bool run_subsystems()
{
	glfwPollEvents();
	render(); // TODO: Unlink vsync from update logic
	return glfwWindowShouldClose(window);
}

void close_subsystems()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}
