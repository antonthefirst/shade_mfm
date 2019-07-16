#pragma once

#include <stddef.h>     /* For size_t */

struct ProgramStats {
	float time_to_compile = 0.f; // time in seconds it took to compile all shaders
	float time_to_link = 0.f;    // time it took to link the program
	const char* comp_log = 0;   
	bool rebuild_occurred = false;
	int program_handle = 0;
};

void injectProceduralFile(const char* pathfile, const char* text, size_t text_len);
bool useProgram(const char* vert, const char* frag, ProgramStats* stats = 0);
bool useProgram(const char* comp, ProgramStats* stats = 0);
void guiShader(bool* open = 0);
void requestFinalShaderSource(const char* pathfile, char** source_location);
bool checkForShaderErrors();

// convenience functions for setting common uniforms. each is internally followed by an assert on gl errors
#include "core/basic_types.h"
#include "core/vec2.h"
#include "core/vec3.h"
#include "core/vec4.h"
#include "core/pose.h"
void setUniformBool(int loc, bool val);
void setUniformInt(int loc, int val);
void setUniformUint(int loc, u32 val);
void setUniformFloat(int loc, float val);
void setUniformVec4(int loc, vec4 val);
void setUniformVec3(int loc, vec3 val);
void setUniformVec2(int loc, vec2 val);
void setUniformIvec2(int loc, ivec2 val);
//void setUniformMat44(int loc, const Pose& p);
void setUniformTexture(int loc, int unit, int target, int tex);
void setUniformImage(int loc, int unit, int access, int format, int tex);
void unsetUniformTexture(int unit, int target);

// manually check for gl errors and print them
bool checkForGLErrors();
