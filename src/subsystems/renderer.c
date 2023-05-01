#include "src/filesystem.h"
#include "src/memory.h"
#define SUBSYSTEM_IMPL
#include "renderer.h"

// TODO: Here and elsewhere, standardise color/colour

// Uninstantiated struct to match uniform Palettes
// [ 8 pals of 3 cols of 4 floats ] [ 4 uints ]
#define N_PALETTES 8
#define N_BIND_POINTS 4
#define N_COLORS 3 // per palette
struct palettes {
	GLfloat colors[N_PALETTES * N_COLORS][4];
	GLuint bind_points[N_BIND_POINTS];
};

// Pixel-to-screen conversion, set at init
GLfloat g_window_width = 1.0f;
GLfloat g_window_height = 1.0f;
GLfloat g_pixel_scale = 1.0f;
// Direction vectors (geometry scale, camera)
#define PX_TO_DIR_X(x) (2.0f * (GLfloat)(x) / g_window_width)
#define PX_TO_DIR_Y(y) (2.0f * (GLfloat)(y) / g_window_height)
// Position vectors (geometry/sprite location)
#define PX_TO_POS_X(x) (PX_TO_DIR_X(x) - 1.0f)
#define PX_TO_POS_Y(y) (PX_TO_DIR_Y(y) - 1.0f)

static GLuint vao;

static GLuint vbo_quad;
static GLuint ubo_palette;

static GLuint tex_uncoloured_buffer;
static GLuint fb_uncoloured_buffer;

// glBindTexture locations
enum {
	TEX_UNRESOLVED,
};

// glBindBufferRange locations
enum {
	UBO_PALETTE,
};

enum {
	PROG_PALETTE_RESOLUTION,
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

	// Quad vertices
	glGenBuffers(1, &vbo_quad);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_quad); {
		GLfloat quad_data[] = {
			1.0, 1.0,
			1.0, 0.0,
			0.0, 1.0,
			0.0, 0.0,
			0.0, 1.0,
			1.0, 0.0,
		};
		glBufferData(GL_ARRAY_BUFFER, sizeof(quad_data), quad_data, GL_STATIC_DRAW);
	}

	// Framebuffer for unresolved texture
	glGenTextures(1, &tex_uncoloured_buffer);
	glBindTexture(GL_TEXTURE_2D, tex_uncoloured_buffer); {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, window_width, window_height, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	}

	glGenFramebuffers(1, &fb_uncoloured_buffer);
	glBindFramebuffer(GL_FRAMEBUFFER, fb_uncoloured_buffer); {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_uncoloured_buffer, 0);
		gvm_assert(
			glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE,
			"Unresolved palette framebuffer incomplete\n"
		);

		// Clear to black - also done after every frame
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glViewport(0, 0, g_window_width, g_window_height); {
			glClear(GL_COLOR_BUFFER_BIT);
		}
	}

	// Palette UBO
	glGenBuffers(1, &ubo_palette);
	glBindBuffer(GL_UNIFORM_BUFFER, ubo_palette); {
		glBufferData(GL_UNIFORM_BUFFER, sizeof(struct palettes), NULL, GL_DYNAMIC_DRAW);
	}
	glBindBufferRange(GL_UNIFORM_BUFFER, UBO_PALETTE, ubo_palette, 0, sizeof(struct palettes));

	// Palette resolution shader
	{
		GLuint shaders[2];
		shaders[0] = load_shader("shaders/screen.vert", GL_VERTEX_SHADER);
		shaders[1] = load_shader("shaders/resolve_colour.frag", GL_FRAGMENT_SHADER);
		gvm_assert(shaders[0] != 0 && shaders[1] != 0, "Failed to compile palette resolution shaders\n");
		char *outs[] = { "out_color" };
		programs[PROG_PALETTE_RESOLUTION] = create_shader_program(2, shaders, 1, outs);
		gvm_assert(programs[PROG_PALETTE_RESOLUTION] != 0, "Failed to link palette resolution shaders\n");
		glDeleteShader(shaders[0]);
		glDeleteShader(shaders[1]);

		GLuint palette_bind = glGetUniformBlockIndex(programs[PROG_PALETTE_RESOLUTION], "Palettes");
		glUniformBlockBinding(programs[PROG_PALETTE_RESOLUTION], palette_bind, UBO_PALETTE);

		glUseProgram(programs[PROG_PALETTE_RESOLUTION]); {
			GLint loc_sampler = glGetUniformLocation(programs[PROG_PALETTE_RESOLUTION], "unresolved");
			GLint loc_position = glGetAttribLocation(programs[PROG_PALETTE_RESOLUTION], "position");
			glUniform1i(loc_sampler, TEX_UNRESOLVED);
			glEnableVertexAttribArray(loc_position);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_quad); {
				glVertexAttribPointer(loc_position, 2, GL_FLOAT, GL_FALSE, 0, 0);
			}
		}
	}

	// Rectangle drawing shader
	{
		GLuint shaders[2];
		shaders[0] = load_shader("shaders/quad.vert", GL_VERTEX_SHADER);
		shaders[1] = load_shader("shaders/flat_colour.frag", GL_FRAGMENT_SHADER);
		gvm_assert(shaders[0] != 0 && shaders[1] != 0, "Failed to compile rectangle shaders\n");
		char *outs[] = { "out_color" };
		programs[PROG_QUAD] = create_shader_program(2, shaders, 1, outs);
		gvm_assert(programs[PROG_QUAD] != 0, "Failed to link rectangle shaders\n");
		glDeleteShader(shaders[0]);
		glDeleteShader(shaders[1]);

		glUseProgram(programs[PROG_QUAD]); {
			GLint position = glGetAttribLocation(programs[PROG_QUAD], "position");
			glEnableVertexAttribArray(position);
			glBindBuffer(GL_ARRAY_BUFFER, vbo_quad); {
				glVertexAttribPointer(position, 2, GL_FLOAT, GL_FALSE, 0, 0);
			}
		}
	}

	// Set defaults (all black)
	for (int pal = 0; pal < N_PALETTES; ++pal) {
		for (int col = 0; col < N_COLORS; ++col) {
			set_palette_colour_impl(pal, col, 0.0, 0.0, 0.0);
		}
	}

	for (int bind = 0; bind < N_BIND_POINTS; ++bind) {
		bind_palette_impl(bind, 0);
	}

	set_camera_impl(0, 0);

	return true;
}

// Bind runtime palette to saved palette
// Returns: success
bool bind_palette_impl(uint8_t bind_point, uint8_t target)
{
	if (bind_point >= N_BIND_POINTS) {
		gvm_error("Invalid palette bind point %d\n", bind_point);
		return false;
	} else if (target >= N_PALETTES) {
		gvm_error("Invalid palette selected for bind: %d\n", target);
		return false;
	}

	glBindBuffer(GL_UNIFORM_BUFFER, ubo_palette); {
		int offset = offsetof(struct palettes, bind_points[bind_point]);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(target), &target);
	}

	return true;
}

// Sets camera for all subsequent draws
// Drawing before calling this on a given frame yields undefined results
void set_camera_impl(int x, int y)
{
	glUseProgram(programs[PROG_QUAD]); {
		GLint location = glGetUniformLocation(programs[PROG_QUAD], "camera");
		glUniform2f(location, PX_TO_DIR_X(x), PX_TO_DIR_Y(y));
	}
}

// Store colour in one of the palettes - intended for initialisation only
// Returns: success
bool set_palette_colour_impl(uint8_t palette, uint8_t colour, float r, float g, float b)
{
	if (palette >= N_PALETTES) {
		gvm_error("Invalid palette index %d\n", palette);
		return false;
	} else if (colour >= N_COLORS) {
		gvm_error("Invalid colour index %d\n", colour);
		return false;
	}

	glBindBuffer(GL_UNIFORM_BUFFER, ubo_palette); {
		GLfloat sub_color[4] = { r, g, b, 0.0 };
		int offset = offsetof(struct palettes, colors[N_COLORS * palette + colour]);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, sizeof(sub_color), sub_color);
	}

	return true;
}

// Draw a rectangle
// Returns: success
bool fill_rect_impl(int x, int y, int w, int h, uint8_t palette, uint8_t colour)
{
	if (palette >= N_BIND_POINTS) {
		gvm_error("Invalid palette index %d\n", palette);
		return false;
	} else if (colour >= N_COLORS) {
		gvm_error("Invalid colour index %d\n", colour);
		return false;
	}

	glUseProgram(programs[PROG_QUAD]); {
		int loc_displacement = glGetUniformLocation(programs[PROG_QUAD], "displacement");
		glUniform2f(loc_displacement, PX_TO_POS_X(x), PX_TO_POS_Y(y));
		int loc_scale = glGetUniformLocation(programs[PROG_QUAD], "scale");
		glUniform2f(loc_scale, PX_TO_DIR_X(w), PX_TO_DIR_Y(h));
		glUniform1ui(glGetUniformLocation(programs[PROG_QUAD], "palette"), palette);
		glUniform1ui(glGetUniformLocation(programs[PROG_QUAD], "color_index"), colour);

		glBindFramebuffer(GL_FRAMEBUFFER, fb_uncoloured_buffer);
		glViewport(0, 0, g_window_width, g_window_height);
		glBindBuffer(GL_ARRAY_BUFFER, vbo_quad); {
			glDrawArrays(GL_TRIANGLES, 0, 6);
		}
	}

	return true;
}

void render()
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, g_window_width * g_pixel_scale, g_window_height * g_pixel_scale);
	glActiveTexture(GL_TEXTURE0 + TEX_UNRESOLVED); {
		glBindTexture(GL_TEXTURE_2D, tex_uncoloured_buffer);
	}
	glUseProgram(programs[PROG_PALETTE_RESOLUTION]); {
		// Shouldn't need to glClearColor() before drawing
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	// Clean back buffer for next frame
	glBindFramebuffer(GL_FRAMEBUFFER, fb_uncoloured_buffer);
	glViewport(0, 0, g_window_width, g_window_height);
	glClearColor(0.0, 0.0, 0.0, 0.0); {
		glClear(GL_COLOR_BUFFER_BIT);
	}
}

void close_renderer()
{
	for (int i = 0; i < PROG_COUNT; ++i) {
		glDeleteProgram(programs[i]);
	}

	glBindVertexArray(vao); {
		glDeleteBuffers(1, &ubo_palette);
		glDeleteBuffers(1, &vbo_quad);
	}

	glDeleteTextures(1, &tex_uncoloured_buffer);
	glDeleteFramebuffers(1, &fb_uncoloured_buffer);

	glDeleteVertexArrays(1, &vao);
}
