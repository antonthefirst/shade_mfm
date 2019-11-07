#version 450 core
layout(location = 0) out vec4 out_col;
layout(set=0, binding=0) uniform sampler2D tex_col;
layout(location = 0) in vec2 st;

void main()
{
	out_col = vec4(texture(tex_col, st).xyz, 1.0);
}