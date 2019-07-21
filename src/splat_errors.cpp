#include "splat_internal.h"
#include "mfm_utils.h"
#include "imgui/imgui.h"

void Errors::add(Token tok, const char* msg) {
	ParserError& e = errors.push();
	e.tok = tok;
	e.msg.set(msg);
}

static void printDiagramImg(Node* who) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float char_size = 16.0f;
	vec2 orig = gui::GetCursorScreenPos();
	gui::InvisibleButton("drawing area", vec2(who->full_img.dims) * char_size);
	for (int y = 0; y < who->full_img.dims.y; ++y) {
		for (int x = 0; x < who->full_img.dims.x; ++x) {
			char key = who->full_img(x, y);
			char comp = who->comp_img(x, y);

			vec2 p = vec2(x + 0.5f, y + 0.5f) * char_size;
			vec2 r = vec2(char_size) * 0.45f;
			vec4 bg_col = COLOR_COMPONENT(comp);
			vec3 text_col = vec3(0.0f);

			vec2 text_r = vec2(-gui::GetFont()->GetCharAdvance(key), -gui::GetFontSize()) * 0.5f;
			dl->AddRectFilled(orig + p - r, orig + p + r, gui::ColorConvertFloat4ToU32(bg_col));
			dl->AddText(orig + p + text_r, gui::ColorConvertFloat4ToU32(vec4(text_col, 1.0f)), &key, &key + 1);
		}
	}
}

static void printDiagram(ImDrawList* dl, vec2 orig, float char_size, const char* keys) {
	for (int i = 0; i < 41; ++i) {
		vec2 p = (vec2(getSiteCoord(i)) + vec2(0.5f)) * char_size;
		vec2 r = vec2(char_size) * 0.45f;
		vec4 bg_col = vec4(vec3(0.3f), 1.0f);
		if (keys[i] != ' ') {
			bg_col = lerp(COLOR_COMPONENT(taxilen(getSiteCoord(i)) + 1), vec4(1.0f), 0.8f);
		}
		vec3 text_col = vec3(0.0f);

		vec2 text_r = vec2(-gui::GetFont()->GetCharAdvance(keys[i]), -gui::GetFontSize()) * 0.5f;
		dl->AddRectFilled(orig + p - r, orig + p + r, gui::ColorConvertFloat4ToU32(bg_col));
		dl->AddText(orig + p + text_r, gui::ColorConvertFloat4ToU32(vec4(text_col, 1.0f)), keys + i, keys + i + 1);
	}
}
static void printDiagramPair(Diagram* d) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	float char_size = 16.0f;
	vec2 orig = gui::GetCursorScreenPos();
	gui::InvisibleButton("drawing area", vec2(9 + 1 + 9, 9) * char_size);
	printDiagram(dl, orig + vec2(4, 4) * char_size, char_size, d->lhs);
	printDiagram(dl, orig + vec2(13 + 1, 4) * char_size, char_size, d->rhs);
}

void printNode(Node* who, int depth) {
	gui::Indent(gui::GetStyle().IndentSpacing * (depth+1));
	gui::Text("Node: %s, Token: %s, %.*s", toStr(who->type), toStr(who->tok.type), who->tok.len, who->tok.str);
	gui::Unindent(gui::GetStyle().IndentSpacing * (depth+1));
	if (gui::IsItemHovered()) {
		gui::PushStyleColor(ImGuiCol_PopupBg, vec4(vec3(0.0f), 1.0f));
		gui::BeginTooltip();
		printDiagramImg(who);
		printDiagramPair(&who->diag);
		gui::EndTooltip();
		gui::PopStyleColor(1);
	}
	if (who->kid) printNode(who->kid, depth + 1);
	if (who->sib) printNode(who->sib, depth);
}

void printErrors(Errors* err) {
	if (err->errors.count == 0) {
		gui::Text("Parsing complete.");
		return;
	}
	for (ParserError* e = err->errors.ptr; e != err->errors.end(); ++e) {
		gui::PushStyleVar(ImGuiStyleVar_ItemSpacing, vec2(0.f, gui::GetStyle().ItemSpacing.y));
		gui::TextColored(COLOR_ERROR, "ERROR "); gui::SameLine();
		gui::Text("in "); gui::SameLine();
		gui::TextColored(COLOR_ERROR, "'%s' ", "test.splat"); gui::SameLine();
		if (e->show_diagram) {
			gui::Text("diagram after "); gui::SameLine();
			gui::TextColored(COLOR_ERROR, "line %d: ", e->tok.line_num); gui::SameLine();
		} else {
			gui::Text("on "); gui::SameLine();
			gui::TextColored(COLOR_ERROR, "line %d: ", e->tok.line_num); gui::SameLine();
		}
		gui::Text("%.*s", e->msg.len, e->msg.str);
		gui::PopStyleVar();
		//gui::TextColored(COLOR_ERROR, "ERROR in file '%s' on line %d:", "test.splat", e->line); gui::SameLine();
		//gui::Text("%.*s", e->msg.len, e->msg.str);

		if (e->show_diagram) {
			for (int y = 0; y < e->img.dims.y; ++y) {
				for (int x = 0; x < e->img.dims.x; ++x) {
					vec4 col = e->img_color[y][x] == vec4(0.0f) ? COLOR_TEXT : e->img_color[y][x];
					gui::TextColored(col, "%c", e->img(x, y));
					if (x != (e->img.dims.x - 1))
						gui::SameLine();
				}
			}
		}

		gui::Spacing();
	}
}

void printCode(StringRange code) {
	gui::TextUnformatted(code.str, code.str + code.len);
}