//include "defines.inl"
//include "globals.inl"
//include "hash.inl"
//include "maths.inl"

layout(location = 2) uniform usampler2D site_bits_img;
layout(location = 3) uniform usampler2D dev_img;
layout(location = 4) uniform  sampler2D color_img;
layout(location = 5) uniform usampler2D vote_img;

in vec2 st;
out vec4 out_col;


void main() {
	uvec4 S = texture(site_bits_img, st);
	uvec4 D = texture(dev_img, st);

	vec4 col = texture(color_img, st);
	col.xyz = lrgb_from_srgb(col.xyz) * col.w;
	
#if 0 // draw sites
	//col = vec4(0);
	const int R = 4;
	int near_active_count = 0;
	vec2 st_active;
    for (int y = -R; y <= R; ++y) {
        for (int x = -R; x <= R; ++x) {
            int m = abs(x) + abs(y);
            if (m <= R) {
				vec2 other_st = st + vec2(x,y) / textureSize(dev_img,0);
				if (texture(dev_img, other_st).z == 1) {
					near_active_count++;
					st_active = other_st;
				}
            }
        }
    }

	if (near_active_count == 1) {
		vec3 site_col = mix(color_from_bits(uint_hash(floor(st_active*textureSize(dev_img,0)))), vec3(1.0), 0.2);
		col = vec4(mix(col.xyz, site_col, 0.5), 1.0);
	} else if (near_active_count != 0) {
		col = vec4(1,0,1,0);
	}

	//if (D.z == 1)
	//	col = vec4(0,0,0,1);
	//if ((D.x & 1) != 0)
	//	col = vec4(1);
#endif

	out_col = col;
}