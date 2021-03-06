//include "draw_shared.inl"

layout(location = 0) in vec2 pos_in;
layout(location = 1) in vec2 st_in;

out gl_PerVertex { vec4 gl_Position; };
layout(location = 0) out vec2 st;

void main()
{
	vec2 pos = camera_from_world_shift + pos_in * camera_from_world_scale;
	pos.x *= inv_camera_aspect;
	gl_Position = vec4(pos.x, pos.y, 0.f, 1.0);
	gl_Position.y = -gl_Position.y;
	st = st_in;
}
