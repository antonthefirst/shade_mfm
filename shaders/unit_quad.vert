layout(location = 0) in vec2 pos_in;
layout(location = 1) in vec2 st_in;

layout(location = 20) uniform vec2 camera_from_world_shift;
layout(location = 21) uniform vec2 camera_from_world_scale;
layout(location = 22) uniform float inv_camera_aspect;

out vec2 st;

void main()
{
	vec2 pos = camera_from_world_shift + pos_in * camera_from_world_scale;
	pos.x *= inv_camera_aspect;
	gl_Position = vec4(pos.x, pos.y, 0.f, 1.0);
	st = st_in;
}