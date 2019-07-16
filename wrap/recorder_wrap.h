#pragma once

void recFrame(const char* name, int x, int y, int w, int h);
void recUI(bool capture_req);
void recUpdate();
void recSetFilename(const char* name);
void recUIToggle();