#include "shader_loader.h"
#include "core/log.h"
#include "imgui/imgui.h"
#include "core/cpu_timer.h"
#include "core/maths.h" // for min max
#include "core/string_range.h"
#include "core/file_stat.h"
#include "core/dir.h"

#include "wrap/evk.h"
#include <GLFW/glfw3.h>


// notes:
// * programs never remove themselves from users's lists of shaders. they could (the indecies are what they need to remove) but in practice a program is defined literally by the list of shaders it uses
//   so if a program is no longer in use, we can reclaim it, and then remove it from it's shaders' lists. but in practice it's not clear how to tell when a program is no longer in use (since it might be temp or not)
//   easiest to just let it ride, and it will get removed on next reboot. the same goes for "file not founds"

// todo
// * when stat returns access denied, don't flash filenames, just let them stay light or dark
// * flash filenames when they do update
// * can i get shader types from handles instead of storing them? maybe not since you may need to know the type before it compiles correctly?
// * replace all "constant" string messages with enums and prebaked strings (or just infer them at the gui callsite)

#include <sstream>
#include <vector>

static void guiSetErroredFile(int idx);
static void guiSetErroredProg(int idx);

namespace {

	struct FileEntry {
		char* pathfile_buff = NULL;
		StringRange pathfile;
		StringRange path;
		StringRange name;
		StringRange file;
		time_t last_modified = 0;
		bool not_found = false;         // needed because last_modified doesn't detect files coming into existence
		char* text = 0;                 // text of the last found file (can be stale, if file no longer exists, or is denied access)
		size_t text_len = 0;
		bool is_procedural = false;     // file is procedurally generated, instead of read in from disk
		char* procedural_text = 0;      // the procedurally generated text and length (as if it was read from disk)
		size_t procedural_text_len = 0;
		bool procedurally_modified = false;
		std::vector<int> user_idxs;     // file indecies that are using (including) this file
		std::vector<int> include_idxs;  // file idxs that this file includes
		int shader_idx = -1;			// shader to notify on any change
		void addUser(int idx);
		void remUser(int idx);
	};

	struct ShaderEntry {
		int file_idx = -1;                      // the main text file of this shader
		VkShaderModule module = VK_NULL_HANDLE; // last successfuly compiled shader
		GLenum type = 0;                        // the type of shader
		bool stale_file = true;                 // set true by files if this shader's file or any that it includes has changed.
		bool stale_module = true;               // set true if an attempted compile failed, and the old shader module remained.
		bool changed_module = false;            // set true when a new shader module replaces the old one
		std::vector<int> prog_idxs;             // the indecies of programs to notify of changes
		String log;
		String final_text;
		s64 time_to_compile = 0;
		char** source_dump_location = 0;
	};

	struct ProgEntry {
		int handle = 0;
		int vert_idx = -1;
		int frag_idx = -1;
		int comp_idx = -1;
		bool try_relink = true;
		bool stale = true;
		bool valid = false;
		bool rebuild_occurred = false;
		String name;
		String log;
		s64 time_to_link = 0;
	};

}

static std::vector<FileEntry> file_entries;
static std::vector<ShaderEntry> shader_entries;
static std::vector<ProgEntry> prog_entries;
static bool any_errors_since_last_check = false;

static void checkForUpdates(int file_idx, StringRange path);

static int getFileEntry(StringRange pathfile, bool is_main) {
	assert(pathfile); // used below, during path extraction
 	for (int i = 0; i < (int)file_entries.size(); ++i) {
		if (file_entries[i].pathfile == pathfile) {
			return i;
		}
	}
	// create a new entry
	int idx = file_entries.size();
	int shader_idx = -1;
	if (is_main) {
		ShaderEntry s;
		s.file_idx = idx;
		const char* ext_str = pathfile.str + pathfile.len - min(size_t(4), pathfile.len);
		/* #PORT
		if (strcmp(ext_str, "vert") == 0) s.type = GL_VERTEX_SHADER;
		else if (strcmp(ext_str, "frag") == 0) s.type = GL_FRAGMENT_SHADER;
		else if (strcmp(ext_str, "comp") == 0) s.type = GL_COMPUTE_SHADER;
		else if (strcmp(ext_str, "geom") == 0) s.type = GL_GEOMETRY_SHADER;
		else if (strcmp(ext_str, "ctrl") == 0) s.type = GL_TESS_CONTROL_SHADER;
		else if (strcmp(ext_str, "eval") == 0) s.type = GL_TESS_EVALUATION_SHADER;
		*/
		shader_idx = shader_entries.size();
		shader_entries.push_back(s);
	}
	FileEntry e;
	e.pathfile_buff = (char*)malloc(pathfile.len+1);
	memcpy(e.pathfile_buff, pathfile.str, pathfile.len);
	e.pathfile_buff[pathfile.len] = 0;
	e.pathfile = StringRange(e.pathfile_buff, pathfile.len);

	const char* end = pathfile.str + pathfile.len;
	while (end != pathfile.str && *(end - 1) != '/') --end;
	e.path.str = e.pathfile_buff;
	e.path.len = end - pathfile.str;

	{ // pick an identifier 'name' (for now dir/filename seems to suffice)
		const char* end = e.pathfile_buff + e.pathfile.len;
		while (end != e.pathfile_buff && *end != '/') --end;
		e.file.str = end + 1; // don't want the slash
		e.file.len = (e.pathfile_buff + e.pathfile.len) - end + 1;
		if (end != e.pathfile_buff) --end;
		while (end != e.pathfile_buff && *(end - 1) != '/') --end;
		e.name.str = end;
		e.name.len = (e.pathfile_buff + e.pathfile.len) - end;
	}
	e.shader_idx = shader_idx;
	file_entries.push_back(e);
	checkForUpdates(idx, e.path);
	return idx;
}

void injectProceduralFile(const char* pathfile, const char* text, size_t text_len) {
	int file_idx = getFileEntry(StringRange(pathfile), false);
	FileEntry& e = file_entries[file_idx];
	e.is_procedural = true;
	e.procedurally_modified = true;
	e.procedural_text = (char*)realloc(e.procedural_text, text_len + 1);
	e.procedural_text_len = text_len;
	assert(e.procedural_text);
	e.procedural_text[text_len] = 0; // null term
	memcpy(e.procedural_text, text, text_len);
}

void FileEntry::addUser(int idx) {
	bool exists = false;
	for (int uidx : user_idxs)
		if (uidx == idx) {
			exists = true;
			break;
		}
	assert(!exists);
	if (!exists) {//#OPT shouldn't need this check
		user_idxs.push_back(idx);
	}
}
void FileEntry::remUser(int idx) {
	for (unsigned i = 0; i < user_idxs.size(); ++i)
		if (user_idxs[i] == idx) {
			user_idxs[i] = user_idxs.back();
			user_idxs.pop_back();
			return;
		}
	assert(false); // shouldn't be removing stuff that isn't there
}
static void clear(int file_idx) {
	FileEntry& f = file_entries[file_idx];
	if (f.text) free(f.text);
	f.text = 0;
	for (int idx : f.include_idxs)
		file_entries[idx].remUser(file_idx);
	f.include_idxs.clear();
}
static void alertUsersOfChange(int file_idx) {
	if (file_entries[file_idx].shader_idx != -1) {
		shader_entries[file_entries[file_idx].shader_idx].stale_file = true;
		assert(file_entries[file_idx].user_idxs.size() == 0); // shader main file shouldn't have users
	}
	else {
		for (int idx : file_entries[file_idx].user_idxs)
			alertUsersOfChange(idx);
	}
}
static void checkForUpdates(int file_idx, StringRange path) {
	time_t last_modified = 0;
	if (file_entries[file_idx].is_procedural) {
		if (!file_entries[file_idx].procedurally_modified)
			return;
		file_entries[file_idx].procedurally_modified = false;
	}
	else {
		errno = 0;
		struct FileStats buf;
		int res = fileStat(file_entries[file_idx].pathfile.str, &buf);
		if (res != 0) {
			// do something on EACCES?
			clear(file_idx);
			if (!file_entries[file_idx].not_found)
				alertUsersOfChange(file_idx);
			file_entries[file_idx].not_found = true;
			return;
		}
		if (file_entries[file_idx].last_modified == buf.fs_mtime && !file_entries[file_idx].not_found)
			return;
		last_modified = buf.fs_mtime;
	}

	clear(file_idx);

	errno = 0;
	FILE* f = NULL;
	if (!file_entries[file_idx].is_procedural) {
#ifdef _WIN32
		fopen_s(&f, file_entries[file_idx].pathfile.str, "rb");
#else
		f = fopen(file_entries[file_idx].pathfile.str, "rb");
#endif
		if (!f) {
			// if we can't access the file, that's ok, just come back later (can be not found, or no access)
			return;
		}
		file_entries[file_idx].last_modified = last_modified;
	}

	file_entries[file_idx].not_found = false;

	size_t text_len = 0;
	if (file_entries[file_idx].is_procedural) {
		text_len = file_entries[file_idx].procedural_text_len;
	} else {
		fseek(f, 0, SEEK_END);
		text_len = ftell(f);
		fseek(f, 0, SEEK_SET);
	}

#ifdef DISABLE_LINE_DIRECTIVES
	TempStr prepend = TempStr("\n");
#else
#ifdef C_STYLE_LINE_DIRECTIVES
	TempStr prepend = TempStr("\n#line 1 \"%s\"\n", file_entries[file_idx].name.str);
#else
	TempStr prepend = TempStr("\n#line 1 %d\n", file_idx);
#endif
#endif
	file_entries[file_idx].text = (char*)malloc(prepend.len + text_len + 1);
	memcpy(file_entries[file_idx].text, prepend.str, prepend.len);

	if (file_entries[file_idx].is_procedural)
		memcpy(file_entries[file_idx].text + prepend.len, file_entries[file_idx].procedural_text, text_len);
	else
		fread(file_entries[file_idx].text + prepend.len, text_len, 1, f);
	file_entries[file_idx].text[prepend.len + text_len] = 0;
	file_entries[file_idx].text_len = prepend.len + text_len;
	
	if (!file_entries[file_idx].is_procedural)
		fclose(f);

	int state = 0;
	// state 0 = first /
	// state 1 = second /
	// state 2 = reading include
	// state 3 = skip space and quote
	// state 4 = read string
	// reading string
	const char* t = file_entries[file_idx].text + prepend.len;
	int ctr = 0;
	char buff[1024];
	int file_start = 0;
	if (path) {
		memcpy(buff, path.str, min(sizeof(buff), path.len));
		file_start = path.len;
	}
	int buff_idx = file_start;
	while (char c = *t++) {
		if (state == 0) {
			if (c == ' ' || c == '\n' || c == '\r') {
				continue;
			}
			else if (c == '/') {
				state = 1;
			}
			else {
				break;
			}
		}
		else if (state == 1) {
			if (c == '/') {
				state = 2;
				ctr = 0;
			}
			else {
				break;
			}
		}
		else if (state == 2) {
			const char* inc = "include";
			if (ctr >= 7) {
				state = 3;
			}
			else if (c == ' ') {
				continue;
			}
			else if (c == inc[ctr++]) {
				continue;
			}
		}
		else if (state == 3) {
			if (c == ' ') {
				continue;
			}
			else if (c == '\"') {
				state = 4;
				buff_idx = file_start;
				buff[buff_idx] = 0;
			}
			else {
				break;
			}
		}
		else if (state == 4) { // oddness... white space is unsupported, even at the tailing end (past quotes, before newline)
			if (c == ' ') {
				log("Unsupported whitespace in include filename\n");
				break;
			}
			else if (c == '\"') {
				buff[buff_idx++] = 0;
				int include_idx = getFileEntry(StringRange(buff, buff_idx - 1), false);
				file_entries[include_idx].addUser(file_idx);
				file_entries[file_idx].include_idxs.push_back(include_idx); 
				state = 0;
			}
			else {
				buff[buff_idx++] = c;
			}
		}
	}
	alertUsersOfChange(file_idx);
}
static void listShaderFiles(int file_idx, std::vector<const char*>* texts, std::vector<int>* lens) {
	const FileEntry& f = file_entries[file_idx];
	for (int idx : f.include_idxs)
		listShaderFiles(idx, texts, lens);
	if (f.text) {
		bool already_added = false;
		for (const auto t : *texts) {
			if (t == f.text) { // each filename has a unique and unduplicated entry, so the text pointer is ok to compare directly
				already_added = true;
				break; 
			}
		}
		if (!already_added) {
			texts->push_back(f.text);
			lens->push_back((int)f.text_len);
		}
	}
}
static void checkAllFilesForUpdates() {
	for (int i = 0; i < (int)file_entries.size(); ++i)
		checkForUpdates(i, file_entries[i].path);
	for (ShaderEntry& s : shader_entries) {
		s.changed_module = false;
		if (s.stale_file) {
			//PrintTimer t(file_entries[s.file_idx].name.str);
			s.stale_file = false;
			for (int idx : s.prog_idxs) {
				prog_entries[idx].try_relink = true;
				prog_entries[idx].stale = true;
			}
			s.log.clear();

			const FileEntry& f = file_entries[s.file_idx];
			if (f.text == 0) {
				continue;
			}
			

			s64 t_start = time_counter();
			/* #PORT
			if (s.handle == 0) s.handle = glCreateShader(s.type);
			*/

			static std::vector<const char*> texts;
			static std::vector<int> lens;
			texts.clear();
			lens.clear();

			static const char* version = "#version 430 core\n";
			static const int version_len = (int)strlen(version); 
			texts.push_back(version);
			lens.push_back(version_len);
			listShaderFiles(s.file_idx, &texts, &lens);

			// concatenate everything into a single file
			static String final_text;
			final_text.clear();
			for (int i = 0; i < texts.size(); ++i)
				final_text.append(texts[i], lens[i]);

			// remove all instances of \r, if we find any #TODO Mac support?
			s.final_text.clear();
			s.final_text.reservebytes(final_text.len);
			const char* r = final_text.str;
			const char* r_end = final_text.str + final_text.len;
			char* w = s.final_text.str;
			int cr_removed_count = 0;
			while (r != r_end) {
				char c = *r++;
				if (c != '\r')
					*w++ = c;
				else
					cr_removed_count += 1;
			}
			log("[SHADER LOADER] Removed %d \\r from %s\n", cr_removed_count, file_entries[s.file_idx].name.str);
			s.final_text.len = w - s.final_text.str;
			
			FILE* file = fopen(TempStr("debug_shaders/%s", file_entries[s.file_idx].file.str), "wb");
			if (file) {
				fwrite(s.final_text.str, s.final_text.len, 1, file);
				fclose(file);
			}

			if (s.source_dump_location) {
				char*& src = *s.source_dump_location;
				int total_len = 0;
				for (unsigned i = 0; i < texts.size(); ++i)
					total_len += lens[i];
				src = (char*)realloc(src, total_len+1);
				char* s = src;
				for (unsigned i = 0; i < texts.size(); ++i) {
					memcpy(s, texts[i], lens[i]);
					s += lens[i];
				}
				src[total_len] = 0;
			}

			dirCreate("shaders_bin");

			// Write the final text to file, so that the compiler can read it.
			fileWriteBinary(TempStr("shaders_bin/%s", f.file.str), s.final_text.str, s.final_text.len);

			// Run compiler
			VkShaderModule module = evkCreateShaderFromFile(TempStr("shaders_bin/%s", f.file.str), &s.log);
			if (s.log.len > 0) { // errors or warnings
				if (module != VK_NULL_HANDLE)
					vkDestroyShaderModule(evk.dev, module, evk.alloc);
				s.stale_module = true;
				guiSetErroredFile(s.file_idx);
				any_errors_since_last_check = true;
			} else {
				s.stale_module = false;
				if (s.module != VK_NULL_HANDLE)
					vkDestroyShaderModule(evk.dev, s.module, evk.alloc);
				s.module = module;
				s.changed_module = true;
			}
			
			s.time_to_compile = time_counter() - t_start; // do this after Get COMPILE_STATUS because that seems to actually block waiting for compiler to finish
		}
	}
#if 0 //#PORT delete
	for (int i = 0; i < (int)prog_entries.size(); ++i) {
		ProgEntry& p = prog_entries[i];
		if (p.try_relink) {
			//PrintTimer t(p.name.str);
			p.try_relink = false;
			p.valid = false;
			bool any_zero = false;
			int attach_idxs[] = { p.vert_idx, p.frag_idx, p.comp_idx };
			for (unsigned i = 0; i < sizeof(attach_idxs) / sizeof(attach_idxs[0]); ++i)
				if (attach_idxs[i] != -1)
					if (shader_entries[attach_idxs[i]].handle == 0)
						any_zero = true;
			if (any_zero) 
				continue;

#if 0 //#PORT
			s64 t_start = time_counter();
			int new_handle = glCreateProgram();

			for (unsigned i = 0; i < sizeof(attach_idxs) / sizeof(attach_idxs[0]); ++i)
				if (attach_idxs[i] != -1)
					glAttachShader(new_handle, shader_entries[attach_idxs[i]].handle);

			glLinkProgram(new_handle);

			for (unsigned i = 0; i < sizeof(attach_idxs) / sizeof(attach_idxs[0]); ++i)
				if (attach_idxs[i] != -1)
					glDetachShader(new_handle, shader_entries[attach_idxs[i]].handle);

			p.log.clear();

			int params = -1;
			glGetProgramiv(new_handle, GL_LINK_STATUS, &params);
			if (params == GL_TRUE) {
				if (p.handle) glDeleteProgram(p.handle);
				p.handle = new_handle;
				int valid = -1;
				glGetProgramiv(p.handle, GL_VALIDATE_STATUS, &valid);
				p.valid = valid == GL_TRUE;
				p.stale = false;
				p.rebuild_occurred = true;
			}
			else {
				int log_bytes = 0;
				glGetProgramiv(new_handle, GL_INFO_LOG_LENGTH, &log_bytes);
				p.log.reservebytes(log_bytes);
				int log_len = 0;
				glGetProgramInfoLog(new_handle, p.log.max_bytes, &log_len, p.log.str);
				p.log.len = log_len;
				glDeleteProgram(new_handle);
				guiSetErroredProg(i);
				any_errors_since_last_check = true;
			}
			p.time_to_link = time_counter() - t_start; // do this after Get LINK_STATUS because that seems to actually block waiting for compiler to finish
#endif
		}
	}
#endif
}
int getShaderEntry(const char* pathfile) {
	if (!pathfile) return -1;
	int file_idx = getFileEntry(StringRange(pathfile), true);
	return file_entries[file_idx].shader_idx;
}
int getProgEntry(const char* vert, const char* frag, const char* comp) {
	int vert_idx = getShaderEntry(vert);
	int frag_idx = getShaderEntry(frag);
	int comp_idx = getShaderEntry(comp);
	for (int i = 0; i < (int)prog_entries.size(); ++i) {
		bool hit = true;
		hit &= (vert_idx == -1) || prog_entries[i].vert_idx == vert_idx;
		hit &= (frag_idx == -1) || prog_entries[i].frag_idx == frag_idx;
		hit &= (comp_idx == -1) || prog_entries[i].comp_idx == comp_idx;
		if (hit)
			return i;
	}
	ProgEntry e;
	int idx = prog_entries.size();
	e.vert_idx = vert_idx;
	e.frag_idx = frag_idx;
	e.comp_idx = comp_idx;
	int idxs[] = { vert_idx, frag_idx, comp_idx };
	int valid_count = 0;
	for (unsigned i = 0; i < ARRSIZE(idxs); ++i)  {
		if (idxs[i] != -1) {
			if (valid_count != 0) e.name.append(" + ");
			e.name.append(file_entries[shader_entries[idxs[i]].file_idx].name);
			shader_entries[idxs[i]].prog_idxs.push_back(idx);
			valid_count += 1;
		}
	}
	prog_entries.push_back(e);
	return idx;
}
static void programStats(ProgramStats* stats, int prog_idx) {
	if (!stats) return;
	ProgEntry& e = prog_entries[prog_idx];
	stats->time_to_link = time_to_sec(e.time_to_link);
	stats->time_to_compile = 0.f;
	stats->rebuild_occurred = e.rebuild_occurred;
	stats->program_handle = e.handle;
	e.rebuild_occurred = false;
	int idxs[] = { e.vert_idx, e.frag_idx, e.comp_idx };	
	for (unsigned i = 0; i < ARRSIZE(idxs); ++i)  {
		if (idxs[i] != -1) {
			stats->time_to_compile += time_to_sec(shader_entries[idxs[i]].time_to_compile);
		}
	}
	if (e.comp_idx != -1 && !shader_entries[e.comp_idx].log.empty()) 
		stats->comp_log = shader_entries[e.comp_idx].log.str;
	else
		stats->comp_log = 0;
}

VkShaderModule shaderGet(const char* pathfile, bool* changed) {
	int idx = getShaderEntry(pathfile);
	checkAllFilesForUpdates();
	*changed = shader_entries[idx].changed_module;
	return shader_entries[idx].module;
}
void shadersDestroy() {
	for (int i = 0; i < shader_entries.size(); ++i) {
		if (shader_entries[i].module != VK_NULL_HANDLE)
			vkDestroyShaderModule(evk.dev, shader_entries[i].module, evk.alloc);
	}
}
bool useProgram(const char* vert, const char* frag, ProgramStats* stats) {
	int prog_idx = getProgEntry(vert, frag, NULL);
	checkAllFilesForUpdates(); // could potentially limit this to just the files "vert" and "frag" care about, but it's probably not worth it right now (if you're changing files, you're probably changing the ones you care about here anyway, and those that aren't changing early out on stat)
	if (prog_entries[prog_idx].handle) {
		programStats(stats, prog_idx);
		/* #PORT
		glUseProgram(prog_entries[prog_idx].handle);
		*/
		return true;
	} else {
		return false;
	}
}
bool useProgram(const char* comp, ProgramStats* stats) {
	int prog_idx = getProgEntry(NULL, NULL, comp);
	checkAllFilesForUpdates(); // could potentially limit this to just the files "vert" and "frag" care about, but it's probably not worth it right now (if you're changing files, you're probably changing the ones you care about here anyway, and those that aren't changing early out on stat)
	programStats(stats, prog_idx);
	if (prog_entries[prog_idx].handle) {
		/* #PORT
		glUseProgram(prog_entries[prog_idx].handle);
		*/
		return true;
	}
	else {
		return false;
	}
}

void dumpFinalShaders(const char* directory) {
	for (int i = 0; i < file_entries.size(); ++i) {
		FileEntry& f = file_entries[i];
		TempStr pathfile = TempStr("%s%.*s", directory, f.file.len, f.file.str);
		if (f.is_procedural) { // dump procedural files
			FILE* file = fopen(pathfile, "wb");
			if (file) {
				fwrite(f.procedural_text, f.procedural_text_len, 1, file);
				fclose(file);
			}
		}
		if (f.shader_idx != -1) { // dump final shader files, and their errors
			ShaderEntry& s = shader_entries[f.shader_idx];
			{
				FILE* file = fopen(pathfile, "wb");
				if (file) {
					fwrite(s.final_text.str, s.final_text.len, 1, file);
					fclose(file);
				}
			}
			if (!s.log.empty()) {
				FILE* file = fopen(TempStr("%s.shader_errors", pathfile.str), "wb");
				if (file) {
					fwrite(s.log.str, s.log.len, 1, file);
					fclose(file);
				}
			}
		}
	}
	for (int i = 0; i < prog_entries.size(); ++i) { // dump program errors
		ProgEntry& p = prog_entries[i];
		if (!p.log.empty()) {
			FILE* file = fopen(TempStr("%s%02d.program_errors", directory, i), "wb");
			if (file) {
				fprintf(file, "Composed from %.*s\n", p.name.len, p.name.str);
				fwrite(p.log.str, p.log.len, 1, file);
				fclose(file);
			}
		}
	}
}

/* GUI Code */

static int file_idx = -1;
static int prog_idx = 0;
static int include_idx = -1;
static void guiSetErroredFile(int idx) { file_idx = idx; }
static void guiSetErroredProg(int idx) { prog_idx = idx; }
void guiIncludeTree(int file_idx) {
        gui::Text("%s", file_entries[file_idx].pathfile.str);
	gui::Indent();
	for (int idx : file_entries[file_idx].user_idxs)
		guiIncludeTree(idx);
	gui::Unindent();
}
void guiShader(bool* open) {
	const vec3 col_red = vec3(0.9, 0.1, 0.1);
	const vec3 col_orange = vec3(1.0, 0.6, 0.1);
	const vec3 col_pink = vec3(0.9, 0.1, 1.0);
	const vec3 col_grey = vec3(0.5);

	bool any_issues = false;
	for (const ShaderEntry& e : shader_entries)
		any_issues |= e.module == VK_NULL_HANDLE;
	for (const ProgEntry& e : prog_entries)
		any_issues |= e.handle == 0;

	if (any_issues) {
		gui::PushStyleColor(ImGuiCol_TitleBg, vec4(col_red*0.8f, 1.f));
		gui::PushStyleColor(ImGuiCol_TitleBgActive, vec4(col_red*0.8, 1.f));
		gui::PushStyleColor(ImGuiCol_TitleBgCollapsed, vec4(col_red*0.9f, 1.f));
	}
	if (gui::Begin("Shaders", open)) {
		gui::Text("Shaders");
		gui::SameLine();
		static char dump_directory[1024] = "./debug_shaders/";
		if (gui::Button("Dump Final Shader Files To Directory")) {
			dumpFinalShaders(dump_directory);
		}
		gui::SameLine();
		gui::InputText("##dump_directory", dump_directory, sizeof(dump_directory));

		gui::Columns(2, "files", true);
		gui::SetColumnWidth(0, 400.f);
		gui::Separator();
		vec3 def_col = vec4(gui::GetStyleColorVec4(ImGuiCol_Text)).xyz();

		for (int i = 0; i < (int)file_entries.size(); ++i) {
			const FileEntry& f = file_entries[i];
			if (f.shader_idx == -1) continue;
			vec3 col = def_col;
			if (f.text == NULL) {
				col = col_grey;
			}
			else {
				const ShaderEntry& s = shader_entries[f.shader_idx];
				if (s.module == VK_NULL_HANDLE) col = col_red;
			}

			gui::PushStyleColor(ImGuiCol_Text, vec4(col, 1.f));
			if (gui::Selectable(file_entries[i].pathfile.str, i == file_idx))
				file_idx = i;
			gui::PopStyleColor(1);
		}
		gui::NextColumn();
		if (file_idx >= 0 && file_idx < (int) file_entries.size()) {
			const FileEntry& f = file_entries[file_idx];
			gui::Text("Compile time: %3.2fms", time_to_msec(shader_entries[f.shader_idx].time_to_compile));
			if (!shader_entries[f.shader_idx].log.empty())
				gui::Text("%s", shader_entries[f.shader_idx].log.str);
			else
				gui::Text("No problem.");

		}
		gui::NextColumn();
		gui::Columns(1);
		gui::Separator();

		gui::Spacing();
		gui::Text("Programs");
		gui::Columns(2, "progs", true);
		gui::SetColumnWidth(0, 400.f);
		gui::Separator();
		for (int i = 0; i < (int)prog_entries.size(); ++i) {
			const ProgEntry& p = prog_entries[i];
			vec3 col = def_col;
			if (p.handle == 0) {
				col = col_red;
			}
			else {
				if (p.stale) {
					col = col_orange;
				}
				else if (!p.valid) {
					col = col_pink;
				}
			}
			gui::PushStyleColor(ImGuiCol_Text, vec4(col, 1.f));
			if (gui::Selectable(p.name.str, i == prog_idx))
				prog_idx = i;
			gui::PopStyleColor(1);
		}
		gui::NextColumn();
		if (prog_entries.size()) {
			const ProgEntry& p = prog_entries[prog_idx];
			gui::Text("Link time: %3.2fms", time_to_msec(p.time_to_link));
			if (p.stale || p.handle == 0) {
				gui::Text("Shaders aren't valid");
				if (p.log.len)
					gui::Text("%s", p.log.str);
			}
			else {
				if (!p.log.empty())
                                        gui::Text("%s", p.log.str);
				else
					gui::Text("No problem.");
			}
		}
		gui::NextColumn();
		gui::Columns(1);
		gui::Separator();

		gui::Spacing();
		gui::Text("Includes");
		gui::Columns(2, "includes", true);
		gui::SetColumnWidth(0, 400.f);
		gui::Separator();
		for (int i = 0; i < (int)file_entries.size(); ++i) {
			const FileEntry& f = file_entries[i];
			if (f.shader_idx != -1) continue;
			vec3 col = def_col;
			if (f.text == NULL) {
				col = col_grey;
			}
			gui::PushStyleColor(ImGuiCol_Text, vec4(col, 1.f));
			if (gui::Selectable(file_entries[i].pathfile.str, i == include_idx))
				include_idx = i;
			gui::PopStyleColor(1);
		}
		gui::NextColumn();
		if (include_idx >= 0 && include_idx < (int) file_entries.size()) {
			gui::Text("Files using this one:");
			guiIncludeTree(include_idx);
		}
		gui::NextColumn();
		gui::Columns(1);
		gui::Separator();

	} gui::End();
	if (any_issues)
		gui::PopStyleColor(3);
}
void requestFinalShaderSource(const char* pathfile, char** source_location) {
	int e = getShaderEntry(pathfile);
	shader_entries[e].source_dump_location = source_location;
}
bool checkForShaderErrors() {
	bool ret = any_errors_since_last_check;
	any_errors_since_last_check = false;
	return ret;
}

const char* GL_type_to_string(GLenum type) {
	/* #PORT
	switch (type) {
	case GL_BOOL: return "bool";
	case GL_INT: return "int";
	case GL_FLOAT: return "float";
	case GL_FLOAT_VEC2: return "vec2";
	case GL_FLOAT_VEC3: return "vec3";
	case GL_FLOAT_VEC4: return "vec4";
	case GL_FLOAT_MAT2: return "mat2";
	case GL_FLOAT_MAT3: return "mat3";
	case GL_FLOAT_MAT4: return "mat4";
	case GL_SAMPLER_2D: return "sampler2D";
	case GL_SAMPLER_3D: return "sampler3D";
	case GL_SAMPLER_CUBE: return "samplerCube";
	case GL_SAMPLER_2D_SHADOW: return "sampler2DShadow";
	default: break;
	}
	*/
	return "other";
}


bool checkForGLErrors() {
	bool first_error = true;
	/* #PORT ?
	while (true) {
		GLenum err = glGetError();
		if (err != GL_NO_ERROR) {
			if (first_error) {
				log("GL Errors:\n");
				first_error = false;
			}
			const char* str = "Unknown Error";

			switch (err) {
			case GL_INVALID_OPERATION:      str = "INVALID_OPERATION";      break;
			case GL_INVALID_ENUM:           str = "INVALID_ENUM";           break;
			case GL_INVALID_VALUE:          str = "INVALID_VALUE";          break;
			case GL_OUT_OF_MEMORY:          str = "OUT_OF_MEMORY";          break;
			case GL_INVALID_FRAMEBUFFER_OPERATION:  str = "INVALID_FRAMEBUFFER_OPERATION";  break;
			}
			log("GL_%s\n", str);
		}
		else {
			break;
		}
	};
	*/
	return !first_error; // if there were no errors, the first error flag would still be set
}
#if 0 // #PORT ?
void setUniformBool(int loc, bool val) {
	glUniform1i(loc, val ? 1 : 0);
	assert(!checkForGLErrors());
}
void setUniformInt(int loc, int val) {
	glUniform1i(loc, val);
	assert(!checkForGLErrors());
}
void setUniformUint(int loc, u32 val) {
	glUniform1ui(loc, val);
	assert(!checkForGLErrors());
}
void setUniformFloat(int loc, float val) {
	glUniform1f(loc, val);
	assert(!checkForGLErrors());
}
void setUniformVec4(int loc, vec4 val) {
	glUniform4fv(loc, 1, (float*)&val);
	assert(!checkForGLErrors());
}
void setUniformVec3(int loc, vec3 val) {
	glUniform3fv(loc, 1, (float*)&val);
	assert(!checkForGLErrors());
}
void setUniformVec2(int loc, vec2 val) {
	glUniform2f(loc, val.x, val.y);
	assert(!checkForGLErrors());
}
void setUniformIvec2(int loc, ivec2 val) {
	glUniform2i(loc, val.x, val.y);
	assert(!checkForGLErrors());
}
/*
void setUniformMat44(int loc, const Pose& p) {
	float mat[16];
	mat4x4_from_pose(mat, p);
	glUniformMatrix4fv(loc, 1, GL_FALSE, mat);
	assert(!checkForGLErrors());
}
*/
void setUniformTexture(int loc, int unit, int target, int tex) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, tex);
	glUniform1i(loc, unit);
	//assert(!checkForGLErrors());
}
void setUniformImage(int loc, int unit, int access, int format, int tex) {
	glBindImageTexture(unit, tex, 0, GL_FALSE, 0, access, format);
	glUniform1i(loc, unit);
	assert(!checkForGLErrors());
}
void unsetUniformTexture(int unit, int target) {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(target, 0);
	//assert(!checkForGLErrors());
}
#endif