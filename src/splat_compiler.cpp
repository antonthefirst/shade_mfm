#include "splat_compiler.h"
#include "splat_internal.h"
#include "core/file_stat.h"
#include "core/log.h"
#include "core/dir.h"
#include "core/cpu_timer.h"
#include "imgui/imgui.h"
#include <stdlib.h> // for malloc/free
#include <stdio.h> // for FILE

#include "core/shader_loader.h" // maybe better to move this out..

namespace {

#define PROJECTS_DIRECTORY "projects/"
#define STDLIB_DIRECTORY "stdlib/"
#define FILE_NOT_FOUND_IDX (-1)

struct FileWatcher {
	String pathfile;
	StringRange file_name;
	String project_name;
	time_t last_modified = 0;
	bool not_found = true;
	String raw_text;
	void init(StringRange pathfile, StringRange project_name);
	void checkForUpdates();
};

};

static bool file_change = true;
static bool project_change = true;
static String current_project;
static StringRange hack_project_name;
static Bunch<FileWatcher> files;
static String splat_concat;
static Bunch<StringRange> file_ranges;
static Bunch<StringRange> file_names;
static Errors err;
static bool show_errors = false;
static Emitter emi_decl;
static Emitter emi_elem;
static Node* root;

void FileWatcher::init(StringRange pathfile_in, StringRange project_name_in) {
	pathfile.set(pathfile_in);
	project_name.set(project_name_in);

	// parse out the name
	const char* end = pathfile.str + pathfile.len;
	while (end != pathfile.str && *end != '/') --end;
	if (*end == '/') end++;
	file_name.str = end;
	file_name.len = (pathfile.str + pathfile.len) - end;
	last_modified = 0;
	not_found = true;
}
void FileWatcher::checkForUpdates() {
	// check if file is present and when it was last modified
	struct FileStats buf;
	int res = fileStat(pathfile.str, &buf);
	if (res != 0) {
		if (not_found == false)
			file_change = true; // if we were found but now aren't, recompile
		not_found = true;
		return;
	}

	not_found = false;

	// if it hasn't been modified since we saw it last, skip it
	if (last_modified == buf.fs_mtime)
		return;
	
	// try and open the file
	FILE* f;
#ifdef _WIN32
	fopen_s(&f, pathfile.str, "rb");
#else
	f = fopen(pathfile.str, "rb");
#endif
	if (!f) {
		// if we can't access the file, that's ok, just come back later (can be not found, or no access)
		return;
	}

	// update monitoring variables
	file_change = true; // signal changes to outer function
	last_modified = buf.fs_mtime;
	

	// load file into memory and close it
	size_t text_len = 0;
	fseek(f, 0, SEEK_END);
	text_len = ftell(f);
	fseek(f, 0, SEEK_SET);
	raw_text.clear();
	raw_text.reservebytes(text_len+1);
	fread(raw_text.str, text_len, 1, f);
	raw_text.str[text_len] = 0; // zero terminate
	raw_text.setlen(text_len); // but keep the length to not include the zero #DODGY
	fclose(f);

	log("Read in '%s', %d characters\n", pathfile.str, raw_text.len);
}

static int getFileEntry(StringRange pathfile, StringRange project_name) {
	int file_idx = FILE_NOT_FOUND_IDX;
	for (int i = 0; i < files.count; ++i) {
		if (files[i].pathfile.range() == pathfile) {
			file_idx = i;
			break;
		}
	}
	if (file_idx == FILE_NOT_FOUND_IDX) {
		file_idx = files.count;
		FileWatcher& F = files.push();
		F.init(pathfile, project_name);
	}
	files[file_idx].checkForUpdates();
	return file_idx;
}

Node* compile(const char* code_start, const char* code_end, StringRange* file_ranges, int file_count, Emitter* emi_decl, Emitter* emi_elem, Errors* err, ProgramInfo* info) {
	*info = ProgramInfo();

	info->time_to_lex = time_counter();
	Lexer lex = Lexer(code_start, code_end, file_ranges, file_count);

	Token tok;
	tok.str = "root";
	tok.len = strlen(tok.str);
	tok.type = Token_unknown;

	Parser par;
	Node* root = makeNode(Node_braces, tok);
	while (Node* block = parseGroups(&par, &lex, err))
		addKid(root, block);
	info->time_to_lex = time_counter() - info->time_to_lex;
	
	if (err->errors.count == 0) {
		info->time_to_parse = time_counter();
		if (root->kid)
			transformAST(root, root->kid, err);
		info->time_to_parse = time_counter() - info->time_to_parse;

		if (err->errors.count == 0) {
			info->time_to_emit = time_counter();
			if (root->kid) {
				emitForwardDeclarationsAndTypes(emi_decl, root, err, info);
				emitElements(emi_decl, emi_elem, root, err, info);
			}
			info->time_to_emit = time_counter() - info->time_to_emit;
		}
	}
	return root;
}

static void splatCallback(const char* pathfile, const char* name, const char* ext) {
	getFileEntry(StringRange(pathfile, strlen(pathfile)), hack_project_name);
} 
static void projectsLoadCallback(const char* pathfile, const char* name) {
	if (current_project.range() == StringRange(name)) {
		hack_project_name = StringRange(name);
		getFileEntry(StringRange(TempStr("%s%s/init.gpulam", PROJECTS_DIRECTORY, name)), hack_project_name);
		dirScan(TempStr("%s%s/", PROJECTS_DIRECTORY, name), ".splat", splatCallback);
	}
}
static void setProject(StringRange name) {
	current_project.set(name);
	project_change = true;
}
static void projectsSelectCallback(const char* pathfile, const char* name) {
	StringRange n = StringRange(name);
	if (gui::Selectable(name, current_project.range() == n))
		setProject(n);
}
void checkForSplatProgramChanges(bool* file_change_out, bool* project_change_out, ProgramInfo* info) {
	if (current_project.len == 0)
		current_project.set("basic");
	if (gui::Begin("Projects")) {
		dirScan(PROJECTS_DIRECTORY, projectsSelectCallback);
	} gui::End();

	// watch for changes in STDLIB and in PROJECTS
	hack_project_name = StringRange("stdlib");
	dirScan(STDLIB_DIRECTORY, ".splat", splatCallback);
	dirScan(PROJECTS_DIRECTORY, projectsLoadCallback);

	// check everyone in this project for updates, which includes already existing files, that may have been deleted from the project directory and weren't caught by the above scan
	for (int i = 0; i < files.count; ++i) {
		if (((files[i].project_name.range() == current_project.range()) || (files[i].project_name.range() == StringRange("stdlib"))) && !(files[i].file_name == StringRange("init.gpulam"))) {
			files[i].checkForUpdates();
		}
	}
			

	*file_change_out = file_change;
	*project_change_out = project_change;

	bool force_recompile = false;

	gui::PushStyleColor(ImGuiCol_WindowBg, vec4(vec3(0.0f), 1.0f));
	if (gui::Begin("Compiler Debug")) {
		static int which = 0;
		gui::RadioButton("input", &which, 0); gui::SameLine();
		gui::RadioButton("output", &which, 1); gui::SameLine();
		gui::RadioButton("ast", &which, 2);
		gui::SameLine();
		force_recompile = gui::Button("recompile"); gui::SameLine();
		if (gui::Button("dump compiled code")) {
			FILE* f = fopen("debug_shaders/code.txt", "wb");
			if (f) {
				fwrite(emi_decl.code.str, emi_decl.code.len, 1, f);
				fwrite(emi_elem.code.str, emi_elem.code.len, 1, f);
				fclose(f);
			}
		}
		gui::Separator();
		gui::BeginChild("code");
		if (which == 0) {
			if (splat_concat.len)
				gui::TextUnformatted(splat_concat.str, splat_concat.str + splat_concat.len);
		} else if (which == 1) {
			if (emi_decl.code.len > 0)
				printCode(emi_decl.code.range());
			if (emi_elem.code.len > 0)
				printCode(emi_elem.code.range());	
		} else if (which == 2) {
			if (root)
				printNode(root);
		}
		gui::EndChild();
	} gui::End();
	gui::PopStyleColor();
	
	if (file_change || project_change || force_recompile) {
		splat_concat.clear();
		file_ranges.clear();
		file_names.clear();
		for (int i = 0; i < files.count; ++i) {
			if (!files[i].not_found && ((files[i].project_name.range() == current_project.range()) || (files[i].project_name.range() == StringRange("stdlib"))) && !(files[i].file_name == StringRange("init.gpulam"))) {
				char* cleaned = (char*)malloc(files[i].raw_text.len + 1);
				char* w = cleaned;
				for (const char* r = files[i].raw_text.str; r != (files[i].raw_text.str + files[i].raw_text.len); ++r) {
					if (*r != '\r') {
						*w++ = *r;
					}
				}
				// although compiler takes start, end, the string MUST be null terminated!
				// this is because strtol is used internally, and that uses null term to do it's thing
				*w = 0;
				file_names.push(files[i].file_name);
				splat_concat.append(StringRange(cleaned, w-cleaned));
				splat_concat.append("\n");
				// add just the length for now, and compute the pointers later after splat_concat is fully complete and no longer reallocs
				StringRange& file_range = file_ranges.push();
				file_range.len = w-cleaned + 1; // string + newline (can't compare pointers, since String potentially reallocs inside during append)
			}
		}
		if (file_ranges.count > 0) {
			file_ranges[0].str = splat_concat.str;
			for (int i = 1; i < file_ranges.count; ++i) {
				file_ranges[i].str = file_ranges[i-1].str + file_ranges[i-1].len;
			}
		}
		err = Errors();
		emi_decl.code.free(); // Gross.
		emi_elem.code.free();
		emi_decl = Emitter();
		emi_elem = Emitter();
		emi_elem.file_names = file_names.ptr;
		emi_elem.file_ranges = file_ranges.ptr;
		emi_elem.file_count = file_names.count;

		if (root) freeNode(root);
		root = compile(splat_concat.str, splat_concat.str + splat_concat.len, file_ranges.ptr, file_ranges.count, &emi_decl, &emi_elem, &err, info);

		emi_elem.code.append("\n");
		bool init_exists = false;
		for (int i = 0; i < files.count; ++i) {
			if ((files[i].project_name.range() == current_project.range()) && files[i].file_name == StringRange("init.gpulam")) {
				if (!files[i].not_found) {
					emi_elem.code.append(files[i].raw_text.range());
					init_exists = true;
					break;
				}
			}
		}
		if (!init_exists)
			emi_elem.code.append("void Init(C2D c, S2D s) { return; }\n");

		if (err.errors.count == 0) {
			injectProceduralFile("shaders/atom_decls.inl", emi_decl.code.str, emi_decl.code.len);
			injectProceduralFile("shaders/atoms.inl", emi_elem.code.str, emi_elem.code.len);
		}
		file_change = false;
		project_change = false;
	}
}

void showSplatCompilerErrors(ProgramInfo* info, StringRange glsl_err, float glsl_time_to_compile) {
	bool any_errors = err.errors.count != 0 || glsl_err.len != 0;
	show_errors |= any_errors;
	if (show_errors) {
		if (any_errors) {
			gui::PushStyleColor(ImGuiCol_TitleBg, vec4(COLOR_ERROR.xyz()*0.6f,1.0f));
			gui::PushStyleColor(ImGuiCol_TitleBgActive, vec4(COLOR_ERROR.xyz()*0.6f,1.0f));
			gui::PushStyleColor(ImGuiCol_TitleBgCollapsed, vec4(COLOR_ERROR.xyz()*0.6f,1.0f));
		}
		if (gui::Begin("Compiler Output", any_errors ? NULL : &show_errors)) { // don't allow to close when there are errors
			if (any_errors) {
				printErrors(&err, glsl_err, file_names.ptr, file_ranges.ptr, file_ranges.count);
			} else {
				/* #PORT
				gui::AlignFirstTextHeightToWidgets();
				*/
				gui::Text("SPLAT compiled successfully."); gui::SameLine();
				if (gui::Button("OK")) {
					show_errors = false;
				}
				gui::Text("Lex time:   %6.3f sec", time_to_sec(info->time_to_lex));
				gui::Text("Parse time: %6.3f sec", time_to_sec(info->time_to_parse));
				gui::Text("Emit time:  %6.3f sec", time_to_sec(info->time_to_emit));
				gui::Text("GLSL time:  %6.3f sec", glsl_time_to_compile);
			}
		} gui::End();
		if (any_errors) {
			gui::PopStyleColor(3);
		}
	}
}
