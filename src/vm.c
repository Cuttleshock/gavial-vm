#define _POSIX_C_SOURCE 200809L // nanosleep() and related definitions

#include <stdlib.h>
#include <time.h>

#include "../deps/glad_gl.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "vm.h"
#include "memory.h"

#define INSTRUCTIONS_INITIAL_SIZE 256
#define FRAME_LENGTH (1.0 / 60.0)

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

// Returns control after the target timestamp
static void delay_until(double target)
{
	double now = glfwGetTime();
	while (now < target) { // if not, exit immediately without a system call
		double duration = target - now;
		struct timespec duration_ts;
		duration_ts.tv_sec = duration;
		duration_ts.tv_nsec = (duration - duration_ts.tv_sec) * 1000000000.0;
		nanosleep(&duration_ts, NULL);
		now = glfwGetTime();
	}
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

	bool loop_done = false;

	while (!loop_done && !glfwWindowShouldClose(window)) {
		double next_frame = glfwGetTime() + FRAME_LENGTH;
		loop_done = run_chunk();
		render();
		glfwPollEvents();
		delay_until(next_frame);
	}

	close_vm();
	return true;
}
