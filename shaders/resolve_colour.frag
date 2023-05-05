#version 330 core

in vec2 frag_tex_position;

uniform usampler2D unresolved;
uniform int discard_zero; // TODO: hacky

layout (std140) uniform Palettes
{
	vec4 colors[24];
	uvec4 bind_points;
};

out vec4 out_color;

void main()
{
	uvec4 colour = texture(unresolved, frag_tex_position);
	uint index = colour.r;
	uint palette = bind_points[index / 3u];
	uint color_index = index % 3u;
	out_color = colors[palette * 3u + color_index];
	if (discard_zero != 0 && palette == 0u && color_index == 0u) {
		discard;
	}
}
