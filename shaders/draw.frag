//include "defines.inl"
//include "globals.inl"
//include "hash.inl"
//include "maths.inl"
//include "cpu_gpu_shared.inl"
//include "bit_packing.inl"

layout(location = 2) uniform usampler2D site_bits_img;
layout(location = 3) uniform usampler2D dev_img;
layout(location = 4) uniform  sampler2D color_img;
layout(location = 5) uniform usampler2D vote_img;

layout(location = 21) uniform vec2 camera_from_world_scale;
layout(location = 23) uniform float screen_from_grid_scale;

layout(location = 30) uniform float event_window_vis;

in vec2 st;
out vec4 out_col;

float dBox(vec2 p, vec2 s) {
	vec2 d = abs(p) - s;
	return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}
float dCircle(vec2 p, float r) {
	return length(p) - r;
}

float smin( float a, float b, float k )
{
    float res = exp2( -k*a ) + exp2( -k*b );
    return -log2( res )/k;
}
float smin_poly( float a, float b, float k )
{
    float h = clamp( 0.5+0.5*(b-a)/k, 0.0, 1.0 );
    return mix( b, a, h ) - k*h*(1.0-h);
}
vec3 smax( vec3 a, vec3 b, float k) {
	vec3 res = exp2(k*a) + exp2(k*b);
    return log2(res)/k;
}

float shape_sd(vec2 p, float r) {
	//return dBox(p, vec2(r));
	return dCircle(p, r);
}
float sdConnector(vec2 pos, float rad, vec2 off, AtomType T_center, vec2 site_uv) {
	uvec4 S = texture(site_bits_img, site_uv + vec2(off.x,-off.y)/textureSize(site_bits_img,0));
	AtomType T = _UNPACK_TYPE(S);
	if (T == T_center) {
		vec2 site_pos = floor(pos) + 0.5;
		return dBox(pos - (site_pos + off*0.5), vec2(rad));
	} else {
		return FLT_MAX;
	}
}
float sdDiagConnector(vec2 pos, float rad, vec2 off, AtomType T_center, vec2 site_uv) {
	uvec4 S = texture(site_bits_img, site_uv + vec2(off.x,-off.y)/textureSize(site_bits_img,0));
	AtomType T = _UNPACK_TYPE(S);
	if (T == T_center) {
		vec2 site_pos = floor(pos) + 0.5;
		return dBox(rotate(normalize(off), pos - (site_pos + off*0.5)), vec2(rad*sqrt(2.0),rad));
	} else {
		return FLT_MAX;
	}
}

void main() {
	vec2 site_uv = st;
	site_uv.y = 1.0 - site_uv.y;
	uvec4 S_center = texture(site_bits_img, site_uv);
	AtomType T_center = _UNPACK_TYPE(S_center);
	//uvec4 D = texture(dev_img, st);

	vec4 col;

	vec2 grid_res = textureSize(color_img, 0);
	
	const float rad = 0.5;
#if 0
	float final_grid_sd = FLT_MAX;
	vec2 grid_pos = st * grid_res;
	vec2 grid_site_pos = floor(st * grid_res) + 0.5;
	float center_grid_sd = shape_sd(grid_pos - grid_site_pos, rad);
	/*uni*/ float smooth_k = 6.0;
	for (int x = -1; x <= 1; ++x) {
		for (int y = -1; y <= 1; ++y) {
			uvec4 S = texture(site_bits_img, site_uv + vec2(x,-y)/textureSize(site_bits_img,0));
			AtomType T = _UNPACK_TYPE(S);
			if (T == T_center) {
				vec2 grid_pos = st * grid_res;
				vec2 grid_site_pos = floor(st * grid_res) + vec2(x,y) + 0.5;
				float grid_sd = shape_sd(grid_pos - grid_site_pos, rad);
				//float blend_sd = smin(center_grid_sd, grid_sd, smooth_k);
				//final_grid_sd =smin(center_grid_sd, grid_sd, smooth_k);// min(final_grid_sd, blend_sd);
				final_grid_sd = smin(final_grid_sd, grid_sd, smooth_k);
			}
		}
	}
#endif

#if 1
	vec2 grid_pos = st * grid_res;
	vec2 grid_site_pos = floor(st * grid_res) + 0.5;
	float final_grid_sd = shape_sd(grid_pos - grid_site_pos, rad);
	final_grid_sd = min(final_grid_sd, sdConnector(grid_pos, rad, vec2(+1.0,0.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdConnector(grid_pos, rad, vec2(-1.0,0.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdConnector(grid_pos, rad, vec2(0.0,+1.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdConnector(grid_pos, rad, vec2(0.0,-1.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdDiagConnector(grid_pos, rad, vec2(+1.0,+1.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdDiagConnector(grid_pos, rad, vec2(-1.0,-1.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdDiagConnector(grid_pos, rad, vec2(-1.0,+1.0), T_center, site_uv));
	final_grid_sd = min(final_grid_sd, sdDiagConnector(grid_pos, rad, vec2(+1.0,-1.0), T_center, site_uv));
#endif

#if 0
	vec2 grid_pos = st * grid_res;
	vec2 grid_site_pos = floor(st * grid_res) + 0.5;
	float final_grid_sd = shape_sd(grid_pos - grid_site_pos, rad);
#endif

	float screen_sd = screen_from_grid_scale * final_grid_sd;

	const float edge_halfwidth = 2.0;
	const float edge_width = edge_halfwidth*2.0;

	if (screen_from_grid_scale > 8) {//(edge_width)) {
		col = texture(color_img, st);
		col.xyz = lrgb_from_srgb(col.xyz) * col.w;
		col.xyz = mix(col.xyz, vec3(0.0), smoothstep(0.0, edge_width, screen_sd + edge_halfwidth));
	} else {
		col = vec4(0.0);
		int A = int(1.0 / screen_from_grid_scale * 0.5) + 1;
		float weight_sum = 0.0;
		for (int x = -A; x <= A; ++x) {
			for (int y = -A; y <= A; ++y) {
				//float d = length(vec2(x,y)) / float(A);
				//float w = (1.0 - d);
				float w = 1.0;
				weight_sum += w;
				vec4 c = texture(color_img, st + vec2(x,y)/grid_res/ screen_from_grid_scale * 0.5 ) * w;
				c.xyz = lrgb_from_srgb(c.xyz) * c.w;
				col += c;
			}
		}
		col /= weight_sum;
	}

	//if (T_center == 1)
	//	col = vec4(1.0);

	if (event_window_vis > 0.0) {
		if (texture(dev_img, site_uv).z != 1) { // colorize around the active site
			const int R = 4;
			int near_active_count = 0;
			vec2 uv_active;
			for (int y = -R; y <= R; ++y) {
				for (int x = -R; x <= R; ++x) {
					int m = abs(x) + abs(y);
					if (m <= R) {
						vec2 other_uv = site_uv + vec2(x,-y) / textureSize(dev_img,0);
						if (texture(dev_img, other_uv).z == 1) {
							near_active_count++;
							uv_active = other_uv;
						}
					}
				}
			}

			if (near_active_count == 1) {
				vec3 site_col = mix(color_from_bits(uint_hash(floor(uv_active*textureSize(dev_img,0)))), vec3(1.0), 0.2);
				col = vec4(mix(col.xyz, site_col, event_window_vis), 1.0);
			} else if (near_active_count != 0) {
				col = vec4(1,0,1,0);
			}
		}
	}

	out_col = col;
}

#if 0 

#if 0
	vec4 col = texture(color_img, st);
	col.xyz = lrgb_from_srgb(col.xyz) * col.w;
#else // this isn't principled....need to actually plumb the resolution, and compute proper pixel footprint..
	vec4 col = vec4(0.0);
	const int A = 5;
	// HACKED UP vec2 tex_size = 1.0/(vec2(1920,1080)*A*camera_from_world_scale);
	for (int x = -A; x <= A; ++x) {
		for (int y = -A; y <= A; ++y) {
			vec4 c = texture(color_img, st + vec2(x,y)*tex_size);
			c.xyz = lrgb_from_srgb(c.xyz) * c.w;
			col += c;
		}
	}
	col /= (A*2+1)*(A*2+1);
#endif


#endif