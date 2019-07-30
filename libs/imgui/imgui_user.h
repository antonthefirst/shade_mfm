namespace gui = ImGui;

namespace ImGui {

inline void ColorPip(const char* id, unsigned int col) {
	const vec2 s = vec2(gui::GetFontSize()); 
	ImDrawList* DL = gui::GetWindowDrawList();
	const vec2 cp = gui::GetCursorScreenPos();
	gui::InvisibleButton(id, s);
	DL->AddRectFilled(cp, cp + s, col);
}

inline void ColorPip(const char* id, vec4 col) { return ColorPip(id, gui::ColorConvertFloat4ToU32(col)); }

inline void ColorBar(const char* id, unsigned int col, float percentage, vec2 size = vec2(0.0f)) {
	vec2 s = vec2(size.x == 0.0f ? gui::GetContentRegionAvailWidth() : size.x, size.y == 0.0f ? gui::GetFontSize() : size.y); 
	ImDrawList* DL = gui::GetWindowDrawList();
	const vec2 cp = gui::GetCursorScreenPos();
	gui::InvisibleButton(id, s);
	DL->AddRectFilled(cp, cp + vec2(s.x * percentage, s.y), col);
}

}
