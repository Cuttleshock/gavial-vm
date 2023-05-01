#version 330 core

in vec2 position;

out vec2 frag_tex_position;

void main()
{
	frag_tex_position = position;
	gl_Position = vec4(position * 2 - vec2(1), 0.0, 1.0);
}
