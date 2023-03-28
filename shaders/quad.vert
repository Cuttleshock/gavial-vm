#version 300 es

// A quad for glDrawArrays(GL_TRIANGLES, 0, 6):
// 1.0, 1.0,
// 1.0, -1.0,
// -1.0, 1.0,
// -1.0, -1.0,
// -1.0, 1.0,
// 1.0, -1.0,

in vec2 position;
in vec2 scale;
in int palette;
in int colour;

layout (std140) uniform Palettes
{
	vec4 colours[16];
};

out vec4 frag_color;

void main()
{
	float x = position.x + (1.0 - 2.0 * float(((gl_VertexID + 1) / 3) % 2)) * scale.x;
	float y = position.y + (1.0 - 2.0 * float(gl_VertexID % 2)) * scale.y;
	gl_Position = vec4(x, y, 0.0, 1.0);
	int color_index = (4 * palette + colour) % 16;
	frag_color = colours[color_index];
}
