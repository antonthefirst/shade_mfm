#include "recorder_wrap.h"
#include "imgui/imgui.h"
//#include "jogif/jogif.h"
#include "wrap/app_wrap.h"
#include "core/maths.h"
#include "core/file_stat.h"
#include "stb/stb_image_write.h"
#include <stdlib.h>
#include <stdio.h>

#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
void drawReadPixels(int x, int y, int width, int height, unsigned char* rgba_pixels) {
	glReadPixels(x,y,width,height, GL_RGBA, GL_UNSIGNED_BYTE, rgba_pixels);
}

struct RecSource {
	const char* name = 0;
	int w = 0;
	int h = 0;
};

#define SCREENSHOT 0
#define GIF 1
#define FILENAME_SIZE 64
#define FILENAME_SUFFIX_SIZE (4+3) //.png/gif + _nn for number suffix
#define IMG_DIRECTORY "screenshots"
#define VID_DIRECTORY "videos"

#define MAX_SOURCES 8
static RecSource sources[MAX_SOURCES];
static bool ui_enable = false;
static char filename[FILENAME_SIZE] = "capture";
static bool recording = false;
static bool snap = false;
static int mode = SCREENSHOT;
static int source_idx = 0;
static int source_count = 0;
static unsigned char* frame = 0;
static int frame_bytesize = 0;
static bool frame_valid = false;
//static jo_gif_t gif;

void recFrame(const char* name, int x, int y, int w, int h) {
	if (source_count >= MAX_SOURCES) return;

	AppState app; appGetState(app);

	x = max(0, min(app.res_x-1,x));
	y = max(0, min(app.res_y-1,y));
	w = min(app.res_x-x, w);
	h = min(app.res_y-y, h);

	RecSource& src = sources[source_count];
	src.name = name;
	src.w = w;
	src.h = h;

	if ((recording || snap) && source_idx == source_count) {
		int size = src.w*src.h*4;
		if (size > frame_bytesize) {
			if (frame)  { free(frame); frame = NULL; }
			frame = (unsigned char*)malloc(size);
			if (frame)
				frame_bytesize = size;
		}
		if (frame) {
			drawReadPixels(x,y,w,h, frame);
			int bpp = 4;
			unsigned char t;
			for (int j=0; j < h/2; ++j) {
				unsigned char *p1 = frame +      j     *w*bpp;
				unsigned char *p2 = frame + (h-1-j)*w*bpp;
				for (int i=0; i < w*bpp; ++i) {
					t = p1[i], p1[i] = p2[i], p2[i] = t;
				}
			}
			frame_valid = true;
		}
	}
	source_count++;
}
void recUI(bool capture_req) {
	if (ui_enable) {
		if (gui::Begin("recorder")) {
			gui::InputText("filename", filename, FILENAME_SIZE-FILENAME_SUFFIX_SIZE); // save space for .gif/.png
			gui::Text("Mode: "); gui::SameLine();
			gui::RadioButton("screenshot", &mode, SCREENSHOT); gui::SameLine();
			gui::RadioButton("gif", &mode, GIF);

			gui::Text("Source:"); gui::SameLine();
			for (int i = 0; i < source_count; ++i) {
				if (gui::RadioButton(sources[i].name, &source_idx, i))
					source_idx = i;
				if (i != source_count-1) gui::SameLine();
			}
			if (source_count) {
				if (mode == GIF) {
					if (recording) {
						if (gui::Button("stop")) {
							//jo_gif_end(&gif);
							recording = false;
						}
						//gui::Text("frame %d", gif.frame);
					} else {
						if (gui::Button("start")) {
							const RecSource& src = sources[source_idx];
							char fname[FILENAME_SIZE];
#ifdef _WIN32
							sprintf_s(fname, FILENAME_SIZE, VID_DIRECTORY"/%s.gif", filename);
#else
							snprintf(fname, FILENAME_SIZE, VID_DIRECTORY"/%s.gif", filename);
#endif
							//gif = jo_gif_start(fname, src.w,src.h, 0, 32);
							recording = true;							
							frame_valid = false;
						}
					}
				}
				if (mode == SCREENSHOT) {
					if (gui::Button("snap"))
						snap = true;
				}
			}
		} gui::End();
	}
	snap |= capture_req;

	source_count = 0;
}
void recUpdate() {
	if (recording && frame_valid) {
		//jo_gif_frame(&gif, frame, 4, false);
	}
	if (snap && frame_valid) {
		char fname[FILENAME_SIZE];
#ifdef _WIN32
		sprintf_s(fname, IMG_DIRECTORY"/%s.png", filename);
#else
		sprintf(fname, IMG_DIRECTORY"/%s.png", filename);
#endif
		int suffix = 0;
		FileStats buf;
		int res = fileStat(fname, &buf);
		while (res == 0) {
#ifdef _WIN32
			sprintf_s(fname, IMG_DIRECTORY"/%s_%02d.png", filename, suffix);
#else
			sprintf(fname, IMG_DIRECTORY"/%s_%02d.png", filename, suffix);
#endif
			res = fileStat(fname, &buf);
			suffix++;
		}
		stbi_write_png(fname, sources[source_idx].w, sources[source_idx].h, 4, frame, sources[source_idx].w*4);
		snap = false;
	}
	frame_valid = false;
}
void recSetFilename(const char* name) {
#ifdef _WIN32
	sprintf_s(filename, "%s", name);
#else
	sprintf(filename, "%s", name);
#endif
}
void recUIToggle() {
	ui_enable = !ui_enable;
}