#version 330 core

uniform uint palette;
uniform uint color_index;

out uint out_color;

void main()
{
	out_color = 3u * palette + color_index;
}
