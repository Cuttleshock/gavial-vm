#version 330 core

in vec2 frag_tex_position;

uniform uint palette;
uniform vec2 sprite_location;
uniform vec2 sprite_scale;
uniform usampler2D sprite_sheet;

out uint out_color;

void main()
{
	vec2 sheet_location = sprite_location + frag_tex_position * sprite_scale;
	uvec4 colour = texture(sprite_sheet, sheet_location);
	if (colour.r == 0u) {
		discard;
	}
	out_color = 3u * palette + colour.r;
}
