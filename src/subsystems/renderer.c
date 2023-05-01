#include "src/filesystem.h"
#include "src/memory.h"
#define SUBSYSTEM_IMPL
#include "renderer.h"

// Pixel-to-screen conversion, set at init
GLfloat g_window_width = 1.0f;
GLfloat g_window_height = 1.0f;
GLfloat g_pixel_scale = 1.0f;

#define PX_TO_SCALE_X(x) (2.0f * (GLfloat)(x) / g_window_width)
#define PX_TO_SCALE_Y(y) (2.0f * (GLfloat)(y) / g_window_height)
#define PX_TO_POS_X(x)   (PX_TO_SCALE_X(x) - 1.0f)
#define PX_TO_POS_Y(y)   (PX_TO_SCALE_Y(y) - 1.0f)

// Uniform block object locations
enum {
	UBO_PALETTE,
};

static GLuint vao;

static struct Rect {
	GLfloat position[2];
	GLfloat scale[2];
	GLuint palette;
	GLuint colour;
} rects[16]; // TODO: magic number
static int rect_count;
static GLuint vbo_rect;

static struct Colour {
	GLfloat r;
	GLfloat g;
	GLfloat b;
	GLfloat a; // unused
} colours[64]; // 16 palettes of 4 colours; TODO: magic number
static GLuint ubo_palette;

// 4 palettes of 4 colours; TODO: magic number
static struct Colour bound_palettes[16]; // Inline copies from colours[]
static uint8_t bound_indices[4]; // What does each bound_palette copy?
static bool bound_touched; // If true, bound_palettes needs refreshing before render

enum {
	PROG_QUAD,
	PROG_COUNT,
};
static GLuint programs[PROG_COUNT];

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
	char *buffer = gvm_malloc(log_length + 1); // TODO: more NULL-checking all round
	buffer[log_length] = '\0';
	glGetProgramInfoLog(program, log_length, NULL, buffer);
	gvm_log("%s\n", buffer);
	gvm_free(buffer);
}

// Loads a shader given a path relative to executable
// Returns: shader handle, or 0 on failure
static GLuint load_shader(char *path, GLenum shader_type)
{
	char *buffer = read_file_executable(path, NULL);
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
bool init_renderer(int window_width, int window_height, int pixel_scale, GLADloadfunc opengl_loader)
{
	// TODO: Error checking
	if (window_width == 0 || window_height == 0) { // Negative values are fine
		return false;
	}
	g_window_width = (GLfloat)window_width;
	g_window_height = (GLfloat)window_height;
	g_pixel_scale = (GLfloat)pixel_scale;

	int version = gladLoadGL(opengl_loader);

	if (!have_gl_debug_output(version)) {
		gvm_log("GL_DEBUG_OUTPUT unavailable\n");
	} else {
		glEnable(GL_DEBUG_OUTPUT);
		glDebugMessageCallback(gl_debug_message_callback, NULL);
	}

	// TODO: Currently using one VAO for everything; watch that assumption
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	glGenBuffers(1, &vbo_rect);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_rect); {
		glBufferData(GL_ARRAY_BUFFER, sizeof(rects), NULL, GL_DYNAMIC_DRAW);
	}

	glGenBuffers(1, &ubo_palette);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_palette); {
		glBufferData(GL_UNIFORM_BUFFER, sizeof(bound_palettes), NULL, GL_DYNAMIC_DRAW);
	}
	glBindBufferRange(GL_UNIFORM_BUFFER, UBO_PALETTE, ubo_palette, 0, sizeof(bound_palettes));

	{
		GLuint quad_shaders[2];
		quad_shaders[0] = load_shader("shaders/quad.vert", GL_VERTEX_SHADER);
		quad_shaders[1] = load_shader("shaders/flat_colour.frag", GL_FRAGMENT_SHADER);
		gvm_assert(quad_shaders[0] != 0 && quad_shaders[1] != 0, "Failed to compile rectangle shaders\n");
		char *quad_outs[] = { "out_color" };
		programs[PROG_QUAD] = create_shader_program(2, quad_shaders, 1, quad_outs);
		gvm_assert(programs[PROG_QUAD] != 0, "Failed to link rectangle shaders\n");
		glDeleteShader(quad_shaders[0]);
		glDeleteShader(quad_shaders[1]);

		GLuint bind_point_palette = glGetUniformBlockIndex(programs[PROG_QUAD], "Palettes");
		glUniformBlockBinding(programs[PROG_QUAD], bind_point_palette, UBO_PALETTE);
	}

	glUseProgram(programs[PROG_QUAD]); {
		GLint in_position = glGetAttribLocation(programs[PROG_QUAD], "position");
		glEnableVertexAttribArray(in_position);
		glVertexAttribDivisor(in_position, 1);
		GLint in_scale = glGetAttribLocation(programs[PROG_QUAD], "scale");
		glEnableVertexAttribArray(in_scale);
		glVertexAttribDivisor(in_scale, 1);
		GLint in_palette = glGetAttribLocation(programs[PROG_QUAD], "palette");
		glEnableVertexAttribArray(in_palette);
		glVertexAttribDivisor(in_palette, 1);
		GLint in_colour = glGetAttribLocation(programs[PROG_QUAD], "colour");
		glEnableVertexAttribArray(in_colour);
		glVertexAttribDivisor(in_colour, 1);

		glBindBuffer(GL_ARRAY_BUFFER, vbo_rect); {
			typedef struct Rect r; // convenience abbreviation
			glVertexAttribPointer(in_position, 2, GL_FLOAT, GL_FALSE, sizeof(r), (void *)offsetof(r, position));
			glVertexAttribPointer(in_scale, 2, GL_FLOAT, GL_FALSE, sizeof(r), (void *)offsetof(r, scale));
			glVertexAttribIPointer(in_palette, 1, GL_INT, sizeof(r), (void *)offsetof(r, palette));
			glVertexAttribIPointer(in_colour, 1, GL_INT, sizeof(r), (void *)offsetof(r, colour));
		}
	}

	// Transient state
	rect_count = 0;

	for (int i = 0; i < 4; ++i) {
		set_palette_colour_impl(0, i, 0.0, 0.0, 0.0);
	}

	for (int i = 0; i < sizeof(bound_indices) / sizeof(bound_indices[0]); ++i) {
		bind_palette_impl(i, 0);
	}

	set_camera_impl(0, 0);

	return true;
}

// Bind runtime palette to saved palette
// Returns: false if arguments invalid
// TODO: See fill_rect_impl
bool bind_palette_impl(uint8_t bind_point, uint8_t target)
{
	int max_bind_point = sizeof(bound_indices) / sizeof(bound_indices[0]);
	int max_target = sizeof(colours) / sizeof(colours[0]) / 4;
	if (bind_point >= max_bind_point) {
		gvm_error("Invalid palette bind point %d\n", bind_point);
		return false;
	} else if (target >= max_target) {
		gvm_error("Invalid palette selected for bind: %d\n", target);
		return false;
	}

	if (bound_indices[bind_point] != target) {
		bound_indices[bind_point] = target;
		bound_touched = true;
	}

	return true;
}

// Sets camera for all subsequent draws
// Drawing before calling this on a given frame yields undefined results
void set_camera_impl(int x, int y)
{
	// TODO
}

// Store colour in one of the palettes - intended for initialisation only
// Returns: success
// TODO: See fill_rect_impl
bool set_palette_colour_impl(uint8_t palette, uint8_t colour, float r, float g, float b)
{
	int max_palette = sizeof(colours) / sizeof(colours[0]) / 4;
	if (palette >= max_palette) {
		gvm_error("Invalid palette index %d\n", palette);
		return false;
	} else if (colour >= 4) {
		gvm_error("Invalid colour index %d\n", colour);
		return false;
	}

	colours[palette * 4 + colour].r = r;
	colours[palette * 4 + colour].g = g;
	colours[palette * 4 + colour].b = b;
	colours[palette * 4 + colour].a = 0.0;
	bound_touched = true; // TODO: Don't need this unless palette is bound

	return true;
}

// Queue a rectangle for rendering this frame
// Returns: success
// TODO: For an alternate interface, we could always succeed: silently mod
// palette and colour, and just start overwriting rects past limit.
bool fill_rect_impl(int x, int y, int w, int h, uint8_t palette, uint8_t colour)
{
	int max_palette = sizeof(colours) / sizeof(colours[0]) / 4;
	int max_rects = sizeof(rects) / sizeof(rects[0]);
	if (rect_count >= max_rects) {
		gvm_error("Reached rectangle draw limit of %d\n", max_rects);
		return false;
	} else if (palette >= max_palette) {
		gvm_error("Invalid palette index %d\n", palette);
		return false;
	} else if (colour >= 4) {
		gvm_error("Invalid colour index %d\n", colour);
		return false;
	}

	struct Rect *r = &rects[rect_count++];
	r->position[0] = PX_TO_POS_X(x);
	r->position[1] = PX_TO_POS_Y(y);
	r->scale[0] = PX_TO_SCALE_X(w);
	r->scale[1] = PX_TO_SCALE_Y(h);
	r->palette = palette;
	r->colour = colour;
	return true;
}

void render()
{
	// TODO: Watch VAO, framebuffer, viewport etc.
	if (bound_touched) {
		// Copy over all colours...
		for (int palette = 0; palette < 4; ++palette) {
			for (int colour = 0; colour < 4; ++colour) {
				struct Colour *b = &bound_palettes[4 * palette + colour];
				struct Colour *t = &colours[4 * bound_indices[palette] + colour];
				b->r = t->r;
				b->g = t->g;
				b->b = t->b;
				b->a = t->a;
			}
		}

		// ... then update OpenGL state
		glBindBuffer(GL_UNIFORM_BUFFER, ubo_palette); {
			glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(bound_palettes), bound_palettes);
		}

		bound_touched = false;
	}

	struct Colour clear_colour = bound_palettes[0]; // always clear to colour (0, 0)
	glClearColor(clear_colour.r, clear_colour.g, clear_colour.b, clear_colour.a);
	glClear(GL_COLOR_BUFFER_BIT);

	if (rect_count > 0) {
		glUseProgram(programs[PROG_QUAD]);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_rect); {
			glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rects), rects);
			glDrawArraysInstanced(GL_TRIANGLES, 0, 6, rect_count);
		}
		rect_count = 0;
	}
}

void close_renderer()
{
	for (int i = 0; i < PROG_COUNT; ++i) {
		glDeleteProgram(programs[i]);
	}

	glBindVertexArray(vao); {
		glDeleteBuffers(1, &ubo_palette);
		glDeleteBuffers(1, &vbo_rect);
	}

	glDeleteVertexArrays(1, &vao);
}
