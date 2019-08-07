#include "core/log.h"
#include "core/container.h"
#include "core/cpu_timer.h"
#include "imgui/imgui.h"
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#define SYS_GL "OpenGL"

namespace {

struct Message {
	GLenum source;
	GLenum type;
	GLuint id;
	GLenum severity;
	GLsizei length;
	char message[1024]; // GL_MAX_DEBUG_MESSAGE_LENGTH = 0x9143 = 37187...so this could use some improvement if we don't want to truncate...
	int count;
	int64_t first_timestamp;
	int64_t last_timestamp;
};

static Bunch<Message> messages;

static const char* DebugTypeString(GLenum type) {
	switch(type) {
	case GL_DEBUG_TYPE_ERROR: return "Error";
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated Behavior";
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined Behavior";
	case GL_DEBUG_TYPE_PORTABILITY: return "Portability";
	case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
	case GL_DEBUG_TYPE_MARKER: return "Marker";
	case GL_DEBUG_TYPE_PUSH_GROUP: return "Push Group";
	case GL_DEBUG_TYPE_POP_GROUP: return "Pop Group";
	case GL_DEBUG_TYPE_OTHER: return "Other";
	default: return "Unknown";
	}
}
static const char* DebugSourceString(GLenum source) {
	switch(source) {
	case GL_DEBUG_SOURCE_API: return "API";
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "Window System";
	case GL_DEBUG_SOURCE_SHADER_COMPILER: return "Shader Compiler";
	case GL_DEBUG_SOURCE_THIRD_PARTY: return "Third Party";
	case GL_DEBUG_SOURCE_APPLICATION: return "Application";
	case GL_DEBUG_SOURCE_OTHER: return "Other";
	default: return "Unknown";
	}
}
static const char* DebugSeverityString(GLenum severity) {
	switch(severity) {
	case GL_DEBUG_SEVERITY_LOW: return "low";
	case GL_DEBUG_SEVERITY_MEDIUM: return "medium";
	case GL_DEBUG_SEVERITY_HIGH: return "high";
	case GL_DEBUG_SEVERITY_NOTIFICATION: return "notification";
	default: return "unknown";
	}
}

static void APIENTRY GLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const char* message, const void* userParam) {
	(void)userParam;
	int64_t timestamp = timeCounterSinceStart();
	if (type == GL_DEBUG_TYPE_ERROR || type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) // log all catastrophic problems to the console
		logInfo(SYS_GL, "'%s' of severity '%s' from '%s: %s", DebugTypeString(type), DebugSeverityString(severity), DebugSourceString(source), message);

	bool found = false;
	for (int i = 0; i < messages.count; ++i) {
		if (source == messages[i].source && type == messages[i].type && id == messages[i].id && severity == messages[i].severity && length == messages[i].length && (strcmp(message, messages[i].message) == 0)) {
			const char* other_message = messages[i].message;
			bool same = strcmp(message, other_message) == 0;
			messages[i].count += 1;
			messages[i].last_timestamp = timestamp;
			found = true;
			break;
		}
	}
	if (!found) {
		Message& m = messages.push();
		m.source = source;
		m.type = type;
		m.id = id;
		m.severity = severity;
		m.length = length;
		m.count = 1;
		m.first_timestamp = timestamp;
		m.last_timestamp = timestamp;
		size_t len = min(sizeof(Message::message)-1, (size_t)length);
		memcpy(m.message, message, len);
		m.message[len] = 0;
	}
}

}

void glMessageLogInit() {

	glDebugMessageCallback((GLDEBUGPROC)GLDebugCallback, nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glEnable(GL_DEBUG_OUTPUT);

	//glClear(GL_DEPTH);   // make an error to test
}
void glMessageLogUI() {
	bool any_errors = false;
	for (int i = 0; i < messages.count; ++i) {
		if (messages[i].type == GL_DEBUG_TYPE_ERROR || messages[i].type == GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR) {
			any_errors = true;
			break;
		}
	}
	if (any_errors) {
		const vec3 col_red = vec3(0.9, 0.1, 0.1);
		gui::PushStyleColor(ImGuiCol_TitleBg, vec4(col_red*0.8f, 1.f));
		gui::PushStyleColor(ImGuiCol_TitleBgActive, vec4(col_red*0.8, 1.f));
		gui::PushStyleColor(ImGuiCol_TitleBgCollapsed, vec4(col_red*0.9f, 1.f));
	}
	static bool open = false;
	if (gui::Begin("GL Debug Message Log", &open)) {
		static int which = 0;
		static GLenum severity_enums[] = { GL_DEBUG_SEVERITY_LOW, GL_DEBUG_SEVERITY_MEDIUM, GL_DEBUG_SEVERITY_HIGH, GL_DEBUG_SEVERITY_NOTIFICATION };
		static vec4 severity_colors[] = { vec4(1.0f,1.0f,0.0f,1.0f), vec4(1.0f,0.5f,0.0f,1.0f), vec4(1.0f,0.0f,0.0f,1.0f), vec4(0.0f,1.0f,0.0f,1.0f) };
		static GLenum which_enums[] = { 0, GL_DEBUG_TYPE_ERROR, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR, GL_DEBUG_TYPE_PORTABILITY, GL_DEBUG_TYPE_PERFORMANCE, GL_DEBUG_TYPE_MARKER, GL_DEBUG_TYPE_PUSH_GROUP, GL_DEBUG_TYPE_POP_GROUP, GL_DEBUG_TYPE_OTHER };
		int counts[ARRSIZE(which_enums)];
		memset(counts, 0, sizeof(counts));
		for (int i = 0; i < messages.count; ++i) {
			counts[0] += 1;
			for (int t = 1; t < ARRSIZE(which_enums); ++t) {
				if (messages[i].type == which_enums[t]) {
					counts[t] += 1;
					break;
				}
			}
		}
		for (int i = 0; i < ARRSIZE(which_enums); ++i) {
			GLenum e = which_enums[i];
			if (!((i%5)==0)) gui::SameLine();
			if (counts[i] == 0) gui::PushStyleColor(ImGuiCol_Text, vec4(vec3(0.4f),1.0f));
			gui::RadioButton(e == 0 ? "All" : DebugTypeString(e), &which, i);
			if (counts[i] == 0) gui::PopStyleColor();
		}
		if (gui::Button("clear")) {
			messages.clear();
		}
		gui::Separator();
		for (int i = 0; i < messages.count; ++i) {
			if (which == 0 || messages[i].type == which_enums[which]) {
				vec4 col = vec4(1.0f);
				for (int s = 0; s < ARRSIZE(severity_enums); ++s) {
					if (messages[i].severity == severity_enums[s]) {
						col = severity_colors[s];
						break;
					}
				}
				bool hover = false;
				if (messages[i].count > 99)
					gui::TextColored(col, "99+ %s (%s):" , DebugTypeString(messages[i].type), DebugSourceString(messages[i].source));
				else if (messages[i].count > 1)
					gui::TextColored(col, "%3d %s (%s):" , messages[i].count, DebugTypeString(messages[i].type), DebugSourceString(messages[i].source));
				else
					gui::TextColored(col, "    %s (%s):" , DebugTypeString(messages[i].type), DebugSourceString(messages[i].source));
				hover |= gui::IsItemHovered();
				gui::SameLine();
				gui::TextWrapped("%.*s", messages[i].length, messages[i].message);
				hover |= gui::IsItemHovered();
				if (hover) {
					int n = time_frequency();
					int freq_digs = 0;
					while (n > 0) { freq_digs += 1; n /= 10; }
					gui::BeginTooltip();
					gui::Text("ID: 0x%08x", messages[i].id);
					gui::Text("Count: %d", messages[i].count);
					if (messages[i].first_timestamp != messages[i].last_timestamp) {
						gui::Text("First Timestamp: %lld.%0*lld", messages[i].first_timestamp / time_frequency(), freq_digs, messages[i].first_timestamp % time_frequency());
						gui::Text(" Last Timestamp: %lld.%0*lld", messages[i].last_timestamp / time_frequency(), freq_digs, messages[i].last_timestamp % time_frequency());
					} else {
						gui::Text("Timestamp: %lld.%0*lld", messages[i].first_timestamp / time_frequency(), freq_digs, messages[i].first_timestamp % time_frequency());
					}
					
					gui::EndTooltip();
				}
			}
		}
	} gui::End();
	if (any_errors) {
		gui::PopStyleColor(3);
	}
}