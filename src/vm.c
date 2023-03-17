#include <stdlib.h>

#include "../deps/glad_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vm.h"
#include "memory.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;

static GLFWwindow *window = NULL;

static void glfw_error_callback(int error, const char *message)
{
	gvm_error("GLFW E%4d: %s\n", error, message);
}

bool init_vm()
{
	vm.instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.count = 0;
	if (!vm.instructions) {
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
	gvm_free(vm.instructions);
}

bool instruction(uint8_t byte)
{
	if (vm.count >= vm.capacity) {
		vm.instructions = gvm_realloc(vm.instructions, vm.capacity, 2 * vm.capacity);
		if (!vm.instructions) {
			return false;
		}
	}

	vm.instructions[vm.count++] = byte;
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
	for (int i = 0; i < vm.count; ++i) {
		switch (vm.instructions[i]) {
			case OP_SET:
			case OP_GET:
			case OP_LOAD_CONST:
			case OP_ADD:
			case OP_SUBTRACT:
			case OP_GET_X:
			case OP_GET_Y:
			case OP_IF:
			case OP_LESS_THAN:
			case OP_GREATER_THAN:
			case OP_LOAD_PAL:
			case OP_CLEAR_SCREEN:
			case OP_FILL_RECT:
			case OP_SWAP:
			case OP_DUP:
			case OP_POP:
			case OP_RETURN:
				return false;
			default: // Unreachable if our parser works correctly
				gvm_error("Unexpected opcode %2x\n", vm.instructions[i]);
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
