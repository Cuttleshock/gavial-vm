#include <stdlib.h>

#include "../deps/glad_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vm.h"
#include "memory.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

uint8_t *instructions;
size_t instruction_capacity;
size_t instruction_count;

static GLFWwindow *window = NULL;

static void glfw_error_callback(int error, const char *message)
{
	gvm_error("GLFW E%4d: %s\n", error, message);
}

bool init_vm()
{
	instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	instruction_capacity = INSTRUCTIONS_INITIAL_SIZE;
	instruction_count = 0;
	if (!instructions) {
		return false;
	}

	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return false;
	}

	return true;
}

static void close_vm()
{
	if (window != NULL) {
		glfwDestroyWindow(window);
	}
	glfwTerminate();
	gvm_free(instructions);
}

bool instruction(uint8_t byte)
{
	if (instruction_count >= instruction_capacity) {
		instructions = gvm_realloc(instructions, instruction_capacity, 2 * instruction_capacity);
		if (!instructions) {
			return false;
		}
	}

	instructions[instruction_count++] = byte;
	return true;
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

// Return value: true if should quit
static bool run_chunk()
{
	for (int i = 0; i < instruction_count; ++i) {
		switch (instructions[i]) {
			case OP_RETURN:
				return false;
			default:
				return true;
		}
	}
	return false;
}

static void render()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
	glfwSwapBuffers(window);
}

// Return value: false if an error occurred preventing execution
bool run_vm()
{
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(512, 512, "Gavial VM", NULL, NULL);
	if (!window) {
		close_vm();
		return false;
	}
	glfwMakeContextCurrent(window); // TODO: error checking
	gladLoadGL(glfwGetProcAddress);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, glfw_key_callback);

	bool loop_done = false;

	while (!loop_done && !glfwWindowShouldClose(window)) {
		// TODO: Unlink vsync from update logic
		loop_done = run_chunk();
		render();
		glfwPollEvents();
	}

	close_vm();
	return true;
}
