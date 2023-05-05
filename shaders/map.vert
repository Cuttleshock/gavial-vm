#version 330 core

in vec2 position;
in vec2 sprite_location; // divisor - per quad
in vec2 flip; // divisor - per quad

uniform int map_width;
uniform vec2 camera;
uniform vec2 scale;
uniform vec2 sprite_scale;

out vec2 frag_tex_position;

void main()
{
	frag_tex_position = (vec2(1) - flip) * position + flip * (vec2(1) - position);
	frag_tex_position *= sprite_scale;
	frag_tex_position += sprite_location;

	vec2 map_coords = vec2(gl_InstanceID % map_width, gl_InstanceID / map_width);
	vec2 screen_position = vec2(-1, 1) + (map_coords + position) * scale - camera;
	gl_Position = vec4(screen_position, 0.0, 1.0);
}
