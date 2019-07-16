#include "maths.h"
#include <math.h>

void rotate(float rx, float ry, float& dx, float& dy)
{
	float tx = dx;
	float ty = dy;
	dx = tx*rx - ty*ry;
	dy = tx*ry + ty*rx;
}

void rotation(float rad, float & dx, float & dy)
{
	dx = cosf(rad);
	dy = sinf(rad);
}

void normalize(float & dx, float & dy)
{
	float il = 1.f/sqrtf(dx*dx + dy*dy);
	dx *= il;
	dy *= il;
}

#if 0 //useful unused code

static void ortho2d(float* mat, float left, float right, float bottom, float top, float zNear = -1.f, float zFar = 1.f)
{
	// this is basically from
	// http://en.wikipedia.org/wiki/Orthographic_projection_(geometry)

	const float inv_z = 1.0f / (zFar - zNear);
	const float inv_y = 1.0f / (top - bottom);
	const float inv_x = 1.0f / (right - left);

	//first column
	*mat++ = (2.0f*inv_x);
	*mat++ = (0.0f);
	*mat++ = (0.0f);
	*mat++ = (0.0f);

	//second
	*mat++ = (0.0f);
	*mat++ = (2.0f*inv_y);
	*mat++ = (0.0f);
	*mat++ = (0.0f);

	//third
	*mat++ = (0.0f);
	*mat++ = (0.0f);
	*mat++ = (-2.0f*inv_z);
	*mat++ = (0.0f);

	//fourth
	*mat++ = (-(right + left)*inv_x);
	*mat++ = (-(top + bottom)*inv_y);
	*mat++ = (-(zFar + zNear)*inv_z);
	*mat++ = (1.0f);
}

#endif