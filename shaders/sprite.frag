#version 330 core

in vec2 frag_tex_position;

uniform uint palette;
uniform usampler2D sprite_sheet;

out uint out_color;

void main()
{
	uvec4 colour = texture(sprite_sheet, frag_tex_position);
	if (colour.r == 0u) {
		discard;
	}
	out_color = 3u * palette + colour.r;
}
