#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define SUBSYSTEM_IMPL
#include "window.h"

typedef enum {
	BUTTON_A,
	BUTTON_B,
	BUTTON_C,
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_START,
	BUTTON_MAX,
} Button;

#define MAP(gvm, glfw) { 1, { GLFW_KEY_ ## glfw } }

// TODO: It's inefficient at runtime but more intuitive logically - trade-off?
static struct {
	int count;
	int glfw_key[16];
} key_mappings[BUTTON_MAX] = {
	MAP(A, Z),
	MAP(B, X),
	MAP(C, C),
	MAP(UP, UP),
	MAP(DOWN, DOWN),
	MAP(LEFT, LEFT),
	MAP(RIGHT, RIGHT),
	MAP(START, ENTER),
};

#undef MAP

static bool button_initial_press[BUTTON_MAX];
static bool button_latest_press[BUTTON_MAX];

static void (*on_save)(void) = NULL;
static void (*on_reload)(void) = NULL;

static GLFWwindow *window = NULL;

static bool key_registered_to(int glfw_key, Button button)
{
	for (int j = 0; j < key_mappings[button].count; ++j) {
		if (key_mappings[button].glfw_key[j] == glfw_key) {
			return true;
		}
	}

	return false;
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		return;
	}

	if (key == GLFW_KEY_S && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
		if (NULL != on_save) {
			on_save();
		}
		return;
	}

	if (key == GLFW_KEY_R && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
		if (NULL != on_reload) {
			on_reload();
		}
		return;
	}

	for (Button button = 0; button < BUTTON_MAX; ++button) {
		if (key_registered_to(key, button)) {
			// Awkward dance to account for GLFW_REPEAT
			if (action == GLFW_RELEASE && button_initial_press[button]) {
				button_latest_press[button] = false;
			} else if (!button_initial_press[button]) {
				button_latest_press[button] = true;
			}
		}
	}
}

bool window_should_close_impl()
{
	return glfwWindowShouldClose(window);
}

bool init_window(int window_width, int window_height, const char *title, GLFWdropfun drop_callback, void (*save_cb)(void), void (*reload_cb)(void))
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
	glfwSetDropCallback(window, drop_callback);
	glfwSwapInterval(1);

	for (int i = 0; i < BUTTON_MAX; ++i) {
		button_initial_press[i] = false;
		button_latest_press[i] = false;
	}

	on_save = save_cb;
	on_reload = reload_cb;

	return true;
}

bool button_pressed_impl(int button)
{
	if (button < 0 || button >= BUTTON_MAX) {
		gvm_error("Invalid button index: %d\n", button);
		return false;
	}

	return button_latest_press[button];
}

void input_impl()
{
	glfwPollEvents();

	for (int i = 0; i < BUTTON_MAX; ++i) {
		button_initial_press[i] = false;
		button_latest_press[i] = false;
		for (int j = 0; j < key_mappings[i].count; ++j) {
			int state = glfwGetKey(window, key_mappings[i].glfw_key[j]);
			if (state == GLFW_PRESS) {
				button_initial_press[i] = true;
				button_latest_press[i] = true;
				break;
			}
		}
	}
}

void swap_buffers()
{
	glfwSwapBuffers(window);
}

void close_window()
{
	glfwDestroyWindow(window);
}
