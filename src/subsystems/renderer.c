#include <stdio.h>

#include "src/filesystem.h"
#include "src/memory.h"
#define SUBSYSTEM_IMPL
#include "renderer.h"

// Returns: if truthy, glEnable(GL_DEBUG_OUTPUT) and related functions can be called.
static bool have_gl_debug_output(int glad_gl_version)
{
	if (glad_gl_version >= GLAD_MAKE_VERSION(4, 3)) {
		return true;
	} else {
		return GLAD_GL_KHR_debug;
	}
}

// Fully verbose caller for OpenGL debug output.
// Usage: glDebugMessageCallback(gl_debug_message_callback, NULL);
static void gl_debug_message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *user_param)
{
	const char *str_source;
	const char *str_type;
	const char *str_severity;

#define CASE(x) case GL_DEBUG_SOURCE_##x: str_source = #x; break
	switch (source) {
		CASE(API);
		CASE(APPLICATION);
		CASE(OTHER);
		CASE(SHADER_COMPILER);
		CASE(THIRD_PARTY);
		CASE(WINDOW_SYSTEM);
		default:
			str_source = "[unknown]";
			break;
	}
#undef CASE

#define CASE(x) case GL_DEBUG_TYPE_##x: str_type = #x; break
	switch (type) {
		CASE(DEPRECATED_BEHAVIOR);
		CASE(ERROR);
		CASE(MARKER);
		CASE(OTHER);
		CASE(PERFORMANCE);
		CASE(POP_GROUP);
		CASE(PORTABILITY);
		CASE(PUSH_GROUP);
		CASE(UNDEFINED_BEHAVIOR);
		default:
			str_type = "[unknown]";
			break;
	}
#undef CASE

#define CASE(x, color) case GL_DEBUG_SEVERITY_##x: str_severity = color #x COLOR_RESET; break
	switch (severity) {
		CASE(HIGH, RED);
		CASE(LOW, GRN);
		CASE(MEDIUM, YLW);
		CASE(NOTIFICATION, BLU);
		default:
			str_severity = "[unknown]";
			break;
	}
#undef CASE

	gvm_log("GL_DEBUG: severity %-12s source %-15s type %s\n", str_severity, str_source, str_type);
	gvm_log("          %s\n", message);
}

// Prints contents of glGetShaderInfoLog(), handling character buffer allocation.
static void print_shader_log(GLuint shader)
{
	GLint log_length;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
	char *buffer = gvm_malloc(log_length + 1);
	buffer[log_length] = '\0';
	glGetShaderInfoLog(shader, log_length, NULL, buffer);
	gvm_log("%s\n", buffer);
	gvm_free(buffer);
}

// Prints contents of glGetProgramInfoLog(), handling character buffer allocation.
static void print_program_log(GLuint program)
{
	GLint log_length;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
	char *buffer = gvm_malloc(log_length + 1);
	buffer[log_length] = '\0';
	glGetProgramInfoLog(program, log_length, NULL, buffer);
	gvm_log("%s\n", buffer);
	gvm_free(buffer);
}

// Loads a shader relative given a relative filepath.
// Returns: shader handle, or 0 on failure.
static GLuint load_shader(char *path, GLenum shader_type)
{
	char *buffer = read_file(path);
	if (buffer == NULL) {
		return 0;
	}

	// Compile shader
	GLuint shader = glCreateShader(shader_type);
	glShaderSource(shader, 1, (const char * const *)&buffer, NULL);
	glCompileShader(shader);

	gvm_free(buffer);

	GLint compile_status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
	if (compile_status != GL_TRUE) {
		print_shader_log(shader);
		return 0;
	}

	// Success!
	return shader;
}

// Creates a program given a list of valid shader handles and out variables.
// Returns: program handle, or 0 on failure.
static GLuint create_shader_program(int num_shaders, GLuint *shaders, int num_outs, char **outs)
{
	GLuint program = glCreateProgram();

	for (int i = 0; i < num_shaders; ++i) {
		glAttachShader(program, shaders[i]);
	}
	for (int i = 0; i < num_outs; ++i) {
		glBindFragDataLocation(program, i, outs[i]);
	}

	glLinkProgram(program);

	for (int i = 0; i < num_shaders; ++i) {
		glDetachShader(program, shaders[i]);
	}

	GLint link_status;
	glGetProgramiv(program, GL_LINK_STATUS, &link_status);
	if (link_status != GL_TRUE) {
		print_program_log(program);
		return 0;
	}

	GLint validate_status;
	glValidateProgram(program);
	glGetProgramiv(program, GL_VALIDATE_STATUS, &validate_status);
	if (validate_status != GL_TRUE) {
		print_program_log(program);
		return 0;
	}

	return program;
}

// Returns: success
// TODO: Passing opengl_loader protects renderer.c from any GLFW logic but in
// exchange leaks glad to the owner - fair trade?
bool init_renderer(GLADloadfunc opengl_loader)
{
	// TODO: Error checking
	int version = gladLoadGL(opengl_loader);

	if (!have_gl_debug_output(version)) {
		gvm_log("GL_DEBUG_OUTPUT unavailable\n");
	} else {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_debug_message_callback, NULL);
	}

	// TODO: dummy calls to hush compiler warnings, incomplete
	GLuint quad_shaders[2];
	quad_shaders[0] = load_shader("shaders/quad.vert", GL_VERTEX_SHADER);
	quad_shaders[1] = load_shader("shaders/flat_colour.frag", GL_FRAGMENT_SHADER);
	char *quad_outs[] = { "out_color" };
	if (quad_shaders[0] != 0 && quad_shaders[1] != 0) {
		GLuint program = create_shader_program(2, quad_shaders, 1, quad_outs);
		if (program != 0) {
			glDeleteProgram(program);
		}
	}
	glDeleteShader(quad_shaders[0]);
	glDeleteShader(quad_shaders[1]);

	return true;
}

// Queue a rectangle for rendering this frame
bool fill_rect_impl(int x, int y, int w, int h, uint8_t palette, uint8_t colour)
{
	if (rect_count >= sizeof(rects) / sizeof(rects[0])) {
		return false;
	}

	rects[rect_count++] = (struct Rect){ { NORM_X(x), NORM_Y(y) }, { NORM_X(w), NORM_Y(h) }, palette, colour };
	return true;
}

void render()
{
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);
}

void close_renderer()
{
	// Nothing to do
}
