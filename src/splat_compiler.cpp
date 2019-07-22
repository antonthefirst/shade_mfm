#include "splat_internal.h"
#include "core/file_stat.h"
#include "core/log.h"
#include "imgui/imgui.h"
#include <stdlib.h> // for malloc/free
#include <stdio.h> // for FILE

#include "core/shader_loader.h" // maybe better to move this out..

void compile(const char* code_start, const char* code_end, Emitter* emi_decl, Emitter* emi_elem, Errors* err) {
	Lexer lex = Lexer(code_start, code_end);

	Token tok;
	tok.str = "root";
	tok.len = strlen(tok.str);
	tok.type = Token_unknown;

	Parser par;
	Node* root = makeNode(Node_braces, tok);
	while (Node* block = parseGroups(&par, &lex, err))
		addKid(root, block);
	
	if (root->kid)
		transformAST(root, root->kid, err);
	printNode(root);

	emitForwardDeclarationsAndTypes(emi_decl, root, err);
	emitElements(emi_elem, root, err);
	freeNode(root);
}


void checkForSplatProgramChanges(bool* file_change, bool* project_change) {
	bool do_shader_recompile = false;
	static time_t last_modified = 0;

	static char* code_mem = 0;
	static size_t code_mem_bytesize = 0;

	const char* pathfile = "test.splat";
	struct FileStats buf;
	int res = fileStat(pathfile, &buf);
	if (res == 0) {
		if (buf.fs_mtime != last_modified) {
			FILE* f = fopen(pathfile, "rb");
			if (f) {
				fseek(f, 0, SEEK_END);
				code_mem_bytesize = ftell(f);
				fseek(f, 0, SEEK_SET);
				free(code_mem);
				code_mem = (char*)malloc(code_mem_bytesize + 1);
				fread(code_mem, code_mem_bytesize, 1, f);
				code_mem[code_mem_bytesize] = 0; // null term
				fclose(f);
				last_modified = buf.fs_mtime;
				do_shader_recompile = true;
			}
		}
	} else {
		log("failed loading\n");
	}

	// remove \r's
	char* cleaned = (char*)malloc(code_mem_bytesize + 1);
	char* w = cleaned;
	for (const char* r = code_mem; r != (code_mem + code_mem_bytesize); ++r) {
		if (*r != '\r') {
			*w++ = *r;
		}
	}
	code_mem_bytesize = w - cleaned;
	cleaned[code_mem_bytesize] = 0;
	free(code_mem);
	code_mem = cleaned;

	gui::PushStyleColor(ImGuiCol_WindowBg, vec4(vec3(0.0f), 1.0f));
	if (gui::Begin("raw code")) {
		if (code_mem)
			gui::TextUnformatted(code_mem, code_mem + code_mem_bytesize);
	} gui::End();
	gui::PopStyleColor();

	Errors err;
	Emitter emi_decl;
	Emitter emi_elem;
	gui::PushStyleColor(ImGuiCol_WindowBg, vec4(vec3(0.0f), 1.0f));
	if (gui::Begin("Compiler")) {
		if (code_mem) {
			// although we take start, end, the string MUST be null terminated!
			// this is because strtol is used internally, and that uses null term to do it's thing
			compile(code_mem, code_mem + code_mem_bytesize, &emi_decl, &emi_elem, &err);
		}
	} gui::End();
	gui::PopStyleColor();

	gui::PushStyleColor(ImGuiCol_WindowBg, vec4(vec3(0.0f), 1.0f));
	if (gui::Begin("Code")) {
		if (gui::Button("dump compiled code")) {
			FILE* f = fopen("code.txt", "wb");
			if (f) {
				fwrite(emi_decl.code.str, emi_decl.code.len, 1, f);
				fwrite(emi_elem.code.str, emi_elem.code.len, 1, f);
				fclose(f);
			}
		}
		if (emi_elem.code.len > 0)
			printCode(emi_elem.code.range());
	} gui::End();
	gui::PopStyleColor();

	gui::PushStyleColor(ImGuiCol_WindowBg, vec4(vec3(0.0f), 1.0f));
	if (gui::Begin("Compiler Output")) {
		printErrors(&err);
	} gui::End();
	gui::PopStyleColor();
	
	if (do_shader_recompile) {
		// hack in init function at the end, for now
		size_t init_size = 0;
		char* init = fileReadCStringIntoMem("init.gpulam", &init_size);
		emi_elem.code.append(init, init_size);
		free(init);

		injectProceduralFile("shaders/atom_decls.inl", emi_decl.code.str, emi_decl.code.len);
		injectProceduralFile("shaders/atoms.inl", emi_elem.code.str, emi_elem.code.len);
		*file_change = true;
	}

	emi_decl.code.free();
	emi_elem.code.free();
}
