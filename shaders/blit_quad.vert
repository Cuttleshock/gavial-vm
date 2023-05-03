#version 330 core

in vec2 position;

uniform vec2 displacement;
uniform vec2 scale;
uniform vec2 camera;

out vec2 frag_tex_position;

void main()
{
	frag_tex_position = position;
	vec2 screen_position = displacement + position * scale - camera;
	gl_Position = vec4(screen_position, 0.0, 1.0);
}
