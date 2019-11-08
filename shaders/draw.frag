//include "defines.inl"
//include "draw_shared.inl"
//include "hash.inl"
//include "maths.inl"

layout(binding = 0) uniform  sampler2D color_img;
layout(binding = 1) uniform usampler2D dev_img;

layout(location = 0) in vec2 st;
layout(location = 0) out vec4 out_col;

float dBox(vec2 p, vec2 s) {
	vec2 d = abs(p) - s;
	return min(max(d.x,d.y),0.0) + length(max(d,0.0));
}
float dCircle(vec2 p, float r) {
	return length(p) - r;
}
float shape_sd(vec2 p, float r) {
	//return dBox(p, vec2(r));
	return dCircle(p, r);
}

void main() {
	vec2 site_uv = st;
	site_uv.y = 1.0 - site_uv.y;

	vec4 col;

	vec2 grid_res = textureSize(color_img, 0);
	
	const float rad = 0.45;

	float amt_sum = 0.0;
	float alpha_sum = 0.0;
	vec4 col_sum = vec4(0.0);
	vec2 grid_pos = st * grid_res;
	//vec2 center_grid_pos = floor(st * grid_res - 0.5); // for doing [0,1] averaging, rather than centering on the middle and wasting (when zoomed way in)
	vec2 center_grid_pos = floor(st * grid_res);
	for (int x = -2; x <= 2; ++x) { // this R value is what we want to control based on zoom level (just have to figure out how)
		for (int y = -2; y <= 2; ++y) {
			vec2 grid_site_pos = center_grid_pos + vec2(x,y) + 0.5;
			float grid_sd = shape_sd(grid_pos - grid_site_pos, rad);

			float screen_sd = screen_from_grid_scale * grid_sd;
			const float edge_halfwidth = 2.0;
			const float edge_width = edge_halfwidth*2.0;
			float amt = 1.0 - smoothstep(0.0, edge_width, screen_sd + edge_halfwidth);
			vec4 site_col = texture(color_img, grid_site_pos / grid_res);
			site_col.xyz = lrgb_from_srgb(site_col.xyz);
			col_sum += site_col * amt;
			
			alpha_sum += site_col.w;
			amt_sum += amt;
		}
	}
	if (amt_sum > 1.0)
		col_sum /= amt_sum;

	col_sum.xyz = srgb_from_lrgb(col_sum.xyz);
	col = col_sum;

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
	out_col = vec4(col.xyz, 1.0);
}
