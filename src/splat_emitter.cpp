#include "splat_internal.h"
#include "mfm_utils.h"
#include "core/string_range.h"
#include "core/shader_loader.h" // for C_STYLE_LINE_DIRECTIVES

// #HACK must match data_fields.cpp for now...
enum BasicType {
	BasicType_Unsigned,
	BasicType_Int,
	BasicType_Bool,
	BasicType_Unary,
	BasicType_C2D,
	BasicType_S2D,
	BasicType_unknown,
};

static StringRange basic_type_strings[] = {
	StringRange("Unsigned"),
	StringRange("Int"),
	StringRange("Bool"),
	StringRange("Unary"),
	StringRange("C2D"),
	StringRange("S2D"),
};


//#TODO maybe replace this with a proper stream writer or something?
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
void emitIndent(Emitter* emi) {
	emi->indent += 1;
}
void emitUnindent(Emitter* emi) {
	emi->indent -= 1;
	assert(emi->indent >= 0);
}
void emitLine(Emitter* emi, const char * format, ...) {
	static char line_buff[1024];
	char* buff = line_buff;
	va_list args;
	va_start(args, format);

	for (int i = 0; i < emi->indent; ++i)
		*(buff++) = '\t';
	buff += vsprintf(buff, format, args);
	buff += sprintf(buff, "\n");

	emi->code.append(StringRange(line_buff, buff - line_buff));

	va_end(args);
}
void emitIndentedText(Emitter* emi, const char * format, ...) {
	static char line_buff[1024];
	char* buff = line_buff;
	va_list args;
	va_start(args, format);

	for (int i = 0; i < emi->indent; ++i)
		*(buff++) = '\t';
	buff += vsprintf(buff, format, args);

	emi->code.append(StringRange(line_buff, buff - line_buff));

	va_end(args);
}
void emitText(Emitter* emi, const char * format, ...) {
	static char line_buff[1024];
	char* buff = line_buff;
	va_list args;
	va_start(args, format);

	buff += vsprintf(buff, format, args);

	emi->code.append(StringRange(line_buff, buff - line_buff));

	va_end(args);
}

#define HEADER_WIDTH 80
void emitHeader(Emitter* emi, int size, char border_char, char endcap_char, StringRange text) {
	if (!text.str) return;
	emi->code.append("\n");
	char comm_buff[128];
	comm_buff[0] = comm_buff[1] = '//';
	comm_buff[HEADER_WIDTH] = '\n';
	for (int i = 2; i < HEADER_WIDTH; ++i) comm_buff[i] = border_char;
	emi->code.append(comm_buff, HEADER_WIDTH+1);
	if (size != 1) {
		for (int i = 2; i < HEADER_WIDTH; ++i) comm_buff[i] = i == HEADER_WIDTH -1 ? endcap_char : ' ';
		emi->code.append(comm_buff, HEADER_WIDTH+1);
	}
	for (int i = 2; i < HEADER_WIDTH; ++i) {
		if (i < HEADER_WIDTH-1) {
			int t = i-4;
			if (t >= 0 && t < text.len) {
				comm_buff[i] = text.str[t];
			} else {
				comm_buff[i] = ' ';
			}
		} else {
			comm_buff[i] = endcap_char;
		}
	}
	emi->code.append(comm_buff, HEADER_WIDTH+1);
	if (size != 1) {
		for (int i = 2; i < HEADER_WIDTH; ++i) comm_buff[i] = i == HEADER_WIDTH -1 ? endcap_char : ' ';
		emi->code.append(comm_buff, HEADER_WIDTH+1);
	}
	for (int i = 2; i < HEADER_WIDTH; ++i) comm_buff[i] = border_char;
	emi->code.append(comm_buff, HEADER_WIDTH+1);
	
}
static const char* findBracesEnd(Node* who) {
	while (who) {
		if (who->type == Node_braces_end)
			return who->tok.str;
		who = who->sib;
	}
	assert(false); // we are assuming matching braces at this point
	return 0;
}
void emitDirectiveLine(Emitter* emi, Token tok) {
	int file_idx = findFileIdx(emi->file_ranges, emi->file_count, tok.str);
#ifdef C_STYLE_LINE_DIRECTIVES
	StringRange file_name = emi->file_names[file_idx];
	emitLine(emi, "/* Injected from file %.*s line %d */", file_name.len, file_name.str, tok.line_num);
#ifndef DISABLE_LINE_DIRECTIVES
	emitLine(emi, "#line %d \"%.*s\"", tok.line_num+1, file_name.len, file_name.str);
#endif
#else
	emitLine(emi, "/* Injected from file %d line %d */", file_idx, tok.line_num);
#ifndef DISABLE_LINE_DIRECTIVES
	emitLine(emi, "#line %d %d", tok.line_num+1, file_idx);
#endif
#endif
}
void emitBlock(Emitter* emi, Node* who) {
	if (!who->kid) return; // empty
	emitDirectiveLine(emi, who->tok);
	const char* block_end = findBracesEnd(who->kid);
	const char* block_start = who->tok.str; // start out sitting on the brace, since the kid's start ignore leading whitespace and indentation
	while (block_start[0] == '{' || isEndOfLine(block_start[0])) block_start += 1;
	emi->code.append(StringRange(block_start, block_end - block_start));
	emi->code.append("\n");
}
#define RULEELEMENT_FORMAT "%.*s_behave"
#define RULEELEMENT_FORMAT_ARGS emi->element_name.len, emi->element_name.str
#define RULESET_FORMAT "%.*s_rs%d"
#define RULESET_FORMAT_ARGS emi->element_name.len, emi->element_name.str, emi->ruleset_idx
#define RULENAME_FORMAT "%.*s_rs%d_r%d"
#define RULENAME_FORMAT_ARGS emi->element_name.len, emi->element_name.str, emi->ruleset_idx, emi->rule_idx
#define GIVEN_KEYCODE_FORMAT "_given_keycode%d"
#define VOTE_KEYCODE_FORMAT "_vote_keycode%d"
#define CHECK_KEYCODE_FORMAT "_check_keycode%d"
#define CHANGE_KEYCODE_FORMAT "_change_keycode%d"

void emitGivenKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "bool " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "(in SiteNum _cursn) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitVoteKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "void " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "(in SiteNum _cursn) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitCheckKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "bool " RULENAME_FORMAT CHECK_KEYCODE_FORMAT "() { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitChangeKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "void " RULENAME_FORMAT CHANGE_KEYCODE_FORMAT "(in SiteNum _cursn) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
static void clearBinds(Emitter* emi) {
	memset(emi->given,  0, sizeof(Emitter::given));
	memset(emi->vote,   0, sizeof(Emitter::vote));
	memset(emi->check,  0, sizeof(Emitter::check));
	memset(emi->change, 0, sizeof(Emitter::change));
}

static void emitRule(Emitter* emi, Node* who, Errors* err) {
	memset(emi->nsites, 0, sizeof(Emitter::nsites));
	memset(emi->lhs_used, 0, sizeof(Emitter::lhs_used));
	memset(emi->rhs_used, 0, sizeof(Emitter::rhs_used));
	int lhs_active_sites = 0;
	int rhs_active_sites = 0;
	for (int i = 0; i < 41; ++i) {
		if (who->diag.lhs[i] != ' ')  {
			emi->lhs_used[who->diag.lhs[i]] = true;
			emi->nsites[who->diag.lhs[i]] += 1;
			lhs_active_sites += 1;
		}
		if (who->diag.rhs[i] != ' ') {
			emi->rhs_used[who->diag.rhs[i]] = true;
			rhs_active_sites += 1;
		}
	}

	/* Create slots to keycode map (and inverse) */
	int slot_count = 0;
	int keycode_from_slot[41] = { };
	int slot_from_keycode[128] = { };
	int rhs_slot_count = 0;
	int rhs_keycode_from_slot[41] = { };
	int rhs_slot_from_keycode[128] = { };
	for (int i = 0; i < ARRSIZE(Emitter::given); ++i) {
		if (emi->lhs_used[i]) {
			keycode_from_slot[slot_count] = i;
			slot_from_keycode[i] = slot_count;
			slot_count += 1;
		} else {
			slot_from_keycode[i] = -1;
		}
		if (emi->rhs_used[i]) {
			rhs_keycode_from_slot[rhs_slot_count] = i;
			rhs_slot_from_keycode[i] = rhs_slot_count;
			rhs_slot_count += 1;
		} else {
			rhs_slot_from_keycode[i] = -1;
		}
	}

	/* Given keycode binds */
	for (int i = 0; i < ARRSIZE(Emitter::given); ++i) {
		if (emi->lhs_used[i]) {
			if (emi->given[i].block || emi->given[i].expression) {
				emitLine(emi, "bool " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject(in SiteNum _cursn) {", RULENAME_FORMAT_ARGS, i);
				if (emi->given[i].block)
					emitBlock(emi, emi->given[i].block);
				else
					emitLine(emi, "return EXPRESSION;");
				emitLine(emi, "}");
			}
			emitGivenKeycodeDecl(emi, i);
			emitIndent(emi);
			if (emi->given[i].block || emi->given[i].expression) {
				if (emi->given[i].isa.len)
					emitLine(emi, "return is(ew(_cursn), %.*s) && " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject(_cursn);", emi->given[i].isa.len, emi->given[i].isa.str, RULENAME_FORMAT_ARGS, i);
				else
					emitLine(emi, "return " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject(_cursn);", RULENAME_FORMAT_ARGS, i);
			} else if (emi->given[i].isa.len) {
				emitLine(emi, "return is(ew(_cursn), %.*s);", emi->given[i].isa.len, emi->given[i].isa.str);
			} else if (i == '@') {
				emitLine(emi, "return true;");
			} else if (i == '_') {
				emitLine(emi, "return ew_isEmpty(_cursn);");
			} else if (i == '?') {
				emitLine(emi, "return ew_isLive(_cursn) && !ew_isEmpty(_cursn);"); //#OPT could just be (t != Void && t != Empty)
			} else if (i == '.') {
				emitLine(emi, "return ew_isLive(_cursn);");
			} else {
				//assert(false);
				emitLine(emi, "return ew_isLive(_cursn); // Default");
			}
			emitUnindent(emi);
			emitLine(emi, "}");
		}
	}


	/* Vote keycode binds */
	for (int s = 0; s < slot_count; ++s) {
		int k = keycode_from_slot[s];
		if (emi->vote[k].block || emi->vote[k].expression) {
			emitLine(emi, "int " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "_inject(in SiteNum _cursn) {", RULENAME_FORMAT_ARGS, k);
			if (emi->vote[k].block)
				emitBlock(emi, emi->vote[k].block);
			else
				emitLine(emi, "return EXPRESSION;");
			emitLine(emi, "}");
		}
		emitVoteKeycodeDecl(emi, k);
		emitLine(emi, "#define _nvotes _nvotes_%d", s);
		emitLine(emi, "#define _winsn _winsn_%d", s);
		emitIndent(emi);
		if (emi->vote[k].block) {
			if (emi->vote[k].isa.len)
				emitLine(emi, "int myvotes = is(ew(_cursn), %.*s) ? " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "_inject(_cursn) : 0;", emi->vote[k].isa.len, emi->vote[k].isa.str, RULENAME_FORMAT_ARGS, k);
			else
				emitLine(emi, "int myvotes = " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "_inject(_cursn);", RULENAME_FORMAT_ARGS, k);
		} else if (emi->vote[k].isa.len) {
			emitLine(emi, "int myvotes = is(ew(_cursn), %.*s) ? 1 : 0;", emi->vote[k].isa.len, emi->vote[k].isa.str);
		} else {
			emitLine(emi, "int myvotes = 1;");
		}
		emitLine(emi, "_nvotes += myvotes;");
		emitLine(emi, "if (random_create(int(_nvotes)) < myvotes) { _winsn = _cursn; }"); //#CAREFUL random_create(int) is what does clamping on 0, not random_create(unsigned)!
		emitUnindent(emi);
		emitLine(emi, "#undef _winsn");
		emitLine(emi, "#undef _nvotes");
		emitLine(emi, "}");
	}

	/* Check keycode binds */
	for (int s = 0; s < slot_count; ++s) {
		emitCheckKeycodeDecl(emi, keycode_from_slot[s]);
		emitLine(emi, "#define _nvotes _nvotes_%d", s);
		emitIndent(emi);
		if (emi->check[keycode_from_slot[s]].block) {
			emitLine(emi, "const int _nsites = %d;", emi->nsites[keycode_from_slot[s]]);
			emitBlock(emi, emi->check[keycode_from_slot[s]].block);
		} else {
			emitLine(emi, "return _nvotes > 0;", s); 
		}
		emitUnindent(emi);
		emitLine(emi, "#undef _nvotes");
		emitLine(emi, "}");
	}

	/* Change keycode binds */
	for (int s_rhs = 0; s_rhs < rhs_slot_count; ++s_rhs) {
		int k = rhs_keycode_from_slot[s_rhs];
		int s_lhs = slot_from_keycode[k];
		emitChangeKeycodeDecl(emi, k);
		if (s_lhs != -1) {
			emitLine(emi, "#define _winsn _winsn_%d", s_lhs);
			emitLine(emi, "#define _winatom _winatom_%d", s_lhs);
		} else {
			emitLine(emi, "#define _winsn InvalidSiteNum");
			emitLine(emi, "#define _winatom InvalidAtom");
		}
		emitIndent(emi);
		if (emi->change[k].block) {
			emitBlock(emi, emi->change[k].block);
		} else if (emi->change[k].isa.len) {
			emitLine(emi, "ew(_cursn, new(%.*s));", emi->change[k].isa.len, emi->change[k].isa.str);
		} else if (k == '_') {
			emitLine(emi, "ew(_cursn, new(Empty));");
		} else if (k == '?') {
			emitLine(emi, "/* Nothing to do */"); 
		} else if (k == '.') {
			emitLine(emi, "/* Nothing to do */");
		} else {
			emitLine(emi, "ew(_cursn, _winatom); // Default");
		}
		emitUnindent(emi);
		emitLine(emi, "#undef _winatom");
		emitLine(emi, "#undef _winsn");
		emitLine(emi, "}");
	}

	/* Given */
	emitLine(emi, "bool " RULENAME_FORMAT "_given() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitIndentedText(emi, "const int sitenum_table[%d] = {", lhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.lhs[i] != ' ')
			emitText(emi, " %d,", i);
	emitLine(emi, "};");
	emitIndentedText(emi, "const int dispatch_table[%d] = {", lhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.lhs[i] != ' ')
			emitText(emi, " %d,", slot_from_keycode[who->diag.lhs[i]]);
	emitLine(emi, "};");
	emitLine(emi, "for (int i = 0; i < %d; ++i) {", lhs_active_sites);
	emitIndent(emi);
	emitLine(emi, "switch (dispatch_table[i]) {");
	for (int i = 0; i < slot_count; ++i)
		emitLine(emi, "case %d: if (!" RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "(sitenum_table[i])) return false; break;", i, RULENAME_FORMAT_ARGS, keycode_from_slot[i]);
	emitLine(emi, "default: break;");
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");
	emitLine(emi, "return true;");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Vote */
	emitLine(emi, "void " RULENAME_FORMAT "_vote() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	for (int i = 0; i < slot_count; ++i) {
		emitLine(emi, "_nvotes_%d = 0; /* %c */", i, keycode_from_slot[i]);
		emitLine(emi, "_winsn_%d = InvalidSiteNum;", i);
	}
	emitIndentedText(emi, "const int sitenum_table[%d] = {", lhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.lhs[i] != ' ')
			emitText(emi, " %d,", i);
	emitLine(emi, "};");
	emitIndentedText(emi, "const int dispatch_table[%d] = {", lhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.lhs[i] != ' ')
			emitText(emi, " %d,", slot_from_keycode[who->diag.lhs[i]]);
	emitLine(emi, "};");
	emitLine(emi, "for (int i = 0; i < %d; ++i) {", lhs_active_sites);
	emitIndent(emi);
	emitLine(emi, "switch (dispatch_table[i]) {");
	for (int i = 0; i < slot_count; ++i)
		emitLine(emi, "case %d: " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "(sitenum_table[i]); break;", i, RULENAME_FORMAT_ARGS, keycode_from_slot[i]);
	emitLine(emi, "default: break;");
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");
	for (int i = 0; i < slot_count; ++i)
		emitLine(emi, "_winatom_%d = (_winsn_%d != 63) ? ew(_winsn_%d) : new(Void);", i, i ,i);
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Check */
	emitLine(emi, "bool " RULENAME_FORMAT "_check() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	for (int s = 0; s < slot_count; ++s) {
		char k = keycode_from_slot[s];
		emitLine(emi, "if (!" RULENAME_FORMAT CHECK_KEYCODE_FORMAT "()) return false; /* %c */", RULENAME_FORMAT_ARGS, k, k);
	}
	emitLine(emi, "return true;");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Change */
	emitLine(emi, "void " RULENAME_FORMAT "_change() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitIndentedText(emi, "const int sitenum_table[%d] = {", rhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.rhs[i] != ' ')
			emitText(emi, " %d,", i);
	emitLine(emi, "};");
	emitIndentedText(emi, "const int dispatch_table[%d] = {", rhs_active_sites);
	for (int i = 0; i < 41; ++i)
		if (who->diag.rhs[i] != ' ')
			emitText(emi, " %d,", rhs_slot_from_keycode[who->diag.rhs[i]]);
	emitLine(emi, "};");
	emitLine(emi, "for (int i = 0; i < %d; ++i) {", rhs_active_sites);
	emitIndent(emi);
	emitLine(emi, "switch (dispatch_table[i]) {");
	for (int s = 0; s < rhs_slot_count; ++s) {
		emitLine(emi, "case %d: " RULENAME_FORMAT CHANGE_KEYCODE_FORMAT "(sitenum_table[i]); break; /* %c */", s, RULENAME_FORMAT_ARGS, rhs_keycode_from_slot[s], rhs_keycode_from_slot[s]);
	}
	emitLine(emi, "default: break;");
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Final rule */
	emitLine(emi, "bool " RULENAME_FORMAT "() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitLine(emi, "if (" RULENAME_FORMAT "_given()) {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitLine(emi, RULENAME_FORMAT "_vote();", RULENAME_FORMAT_ARGS);
	emitLine(emi, "if (" RULENAME_FORMAT "_check()) {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitLine(emi, RULENAME_FORMAT "_change();", RULENAME_FORMAT_ARGS);
	emitLine(emi, "return true;");
	emitUnindent(emi);
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");
	emitLine(emi, "return false;");
	emitUnindent(emi);
	emitLine(emi, "}");

	emi->rule_idx += 1;
}
static bool isPhaseNode(Node* who) {
	return who->tok.key == Keyword_given || who->tok.key == Keyword_vote || who->tok.key == Keyword_check || who->tok.key == Keyword_change;
}
static Node* emitPhase(Emitter* emi, Node* who, Errors* err) {
	KeycodeBinding* bind = NULL;
	char keycode = 0;
	StringRange isa;
	Keyword key = who->tok.key;

	switch (key) {
	case Keyword_given:  bind = emi->given;  break;
	case Keyword_vote:   bind = emi->vote;   break;
	case Keyword_check:  bind = emi->check;  break;
	case Keyword_change: bind = emi->change; break;
	};
	assert(bind); // we shouldn't be here unless .key is one of the phases...

	/* Optional vote norm */
	if (key == Keyword_vote) {
		if (!who->sib) { err->add(who->tok, TempStr("Phase '%.*s' expected an option or a keycode to bind.", who->tok.len, who->tok.str)); return NULL; } // could return 'who' here if we want to continue rather than bail...
		if (who->sib->type == Node_identifier && StringRange("max") == StringRange(who->sib->tok.str, who->sib->tok.len)) { //#OPT shouldn't have to construct it here?
			err->add(who->tok, "Sorry vote 'max' is not supported yet."); return NULL;
			who = who->sib;
		}
	}

	/* Keycode */
	if (!who->sib) { err->add(who->tok, TempStr("Phase '%.*s' expected a keycode to bind.", who->tok.len, who->tok.str)); return NULL; } // could return 'who' here if we want to continue rather than bail...
	who = who->sib;
	if (who->type != Node_identifier) { err->add(who->tok, TempStr("Phase keycode '%.*s' is not valid.", who->tok.len, who->tok.str)); return NULL; }
	if (who->tok.len != 1) { err->add(who->tok, TempStr("Phase keycode '%.*s' is more than one character long.", who->tok.len, who->tok.str)); return NULL; }
	if (who->tok.str[0] == '_' || who->tok.str[0] == '.' || who->tok.str[0] == '?') { err->add(who->tok, TempStr("Phase keycode '%c' cannot be defined.", who->tok.str[0], who->tok.str[0])); return NULL; }
	if (who->tok.str[0] != '@' && !isAlpha(who->tok.str[0]) && !isNumeric(who->tok.str[0])) { err->add(who->tok, TempStr("Phase keycode '%c' (ASCII value %d) is not a printable character.", who->tok.str[0], who->tok.str[0])); return NULL; }
	
	bind += who->tok.str[0];

	/* What */
	who = who->sib;
	if (who) {
		/* expression */
		if (who->tok.type == Token_colon) {
			err->add(who->tok, "Sorry expressions via ':' are not supported yet. Can use { return ...; } in the meantime?"); return NULL;
		}

		/* isa */
		if (who->type == Node_keyword && who->tok.key == Keyword_isa) {
			if (key == Keyword_check) { err->add(who->tok, "'isa' cannot be used with check phase."); return NULL; }
			if (!who->sib) { err->add(who->tok, "'isa' requires a typename."); return NULL; }
			who = who->sib;
			if (who->type != Node_identifier) { err->add(who->tok, TempStr("'%.*s' is not a recognized typename", who->tok.len, who->tok.str)); return NULL; }
			bind->isa = StringRange(who->tok.str, who->tok.len);
			who = who->sib;
		} else if (who->type != Node_braces) { err->add(who->tok, TempStr("Phase body '%.*s' is not recognized. (Expected 'isa Type' or { })", who->tok.len, who->tok.str)); return NULL; }
		
		/* braces */
		if (who && who->type == Node_braces) {
			bind->block = who;
			who = who->sib;
		}
	}
	return who;
}
void emitRuleset(Emitter* emi) {
	if (emi->rule_idx > 1) {
		emitLine(emi, "bool " RULESET_FORMAT "() {", RULESET_FORMAT_ARGS);
		emitIndent(emi);
		for (int i = 1; i < emi->rule_idx; ++i) {
			emitLine(emi, "if (" RULENAME_FORMAT "()) return true;", emi->element_name.len, emi->element_name.str, emi->ruleset_idx, i);
		}
		emitLine(emi, "return false;");
		emitUnindent(emi);
		emitLine(emi, "}");
		emi->ruleset_idx += 1;
	}
	
	emi->rule_idx = 0;
}
void emitElement(Emitter* emi, Node* who) {
	/* Rulesets */
	if (emi->ruleset_idx > 1) {
		emitLine(emi, "bool " RULEELEMENT_FORMAT "() {", RULEELEMENT_FORMAT_ARGS);
		emitIndent(emi);
		for (int i = 1; i < emi->ruleset_idx; ++i) {
			emitLine(emi, "if (" RULESET_FORMAT "()) return true;", emi->element_name.len, emi->element_name.str, i);
		}
		if (emi->super_name.len)
			emitLine(emi, "if (%.*s_behave()) return true;", emi->super_name.len, emi->super_name.str);
		emitLine(emi, "return false;");
		emitUnindent(emi);
		emitLine(emi, "}");
	} else {
		if (emi->super_name.len)
			emitLine(emi, "bool " RULEELEMENT_FORMAT "() { return %.*s_behave(); }", RULEELEMENT_FORMAT_ARGS, emi->super_name.len, emi->super_name.str);
		else
			emitLine(emi, "bool " RULEELEMENT_FORMAT "() { return false; }", RULEELEMENT_FORMAT_ARGS); // #TODO #WRONG should fault here
	}
	emi->ruleset_idx = 0;

	/* Default getColor */
	u32 color = 0xff00ffff;
	{
		Node* kid = who->kid;
		while (kid) {
			if (kid->type == Node_metadata_color) {
				color = kid->unsigned_val;
				break;
			}
			kid = kid->sib;
		}
	}
	emitLine(emi, "ARGB %.*s_getColor(Unsigned selector) {", emi->element_name.len, emi->element_name.str);
	emitIndent(emi);
	emitLine(emi, "return cu_color(%d,%d,%d);", (color>>16)&0xff, (color>>8)&0xff, (color>>0)&0xff);
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Event start */
	u32 symmetries = 0;
	{
		Node* kid = who->kid;
		while (kid) {
			if (kid->type == Node_metadata_symmetries) {
				symmetries = kid->unsigned_val;
				break;
			}
			kid = kid->sib;
		}
	}
	emitLine(emi, "void %.*s_EVENT_START() {", emi->element_name.len, emi->element_name.str);
	emitIndent(emi);
	if (symmetries == 0) {
		emitLine(emi, "_SYMMETRY = 0;");
	} else if (symmetries == 0xff) {
		emitLine(emi, "_SYMMETRY = random_create(8);");
	} else {
		int symmetries_count = 0;
		for (int i = 0; i < 8; ++i)
			if (symmetries & (1 << i))
				symmetries_count += 1;
		emitLine(emi, "switch(random_create(%d)) {", symmetries_count);
		emitIndent(emi);
		int case_i = 0;
		for (int sym_i = 0; sym_i < 8; ++sym_i) {
			if (symmetries & (1 << sym_i)) {
				emitLine(emi, "case %d: _SYMMETRY = %d; break;", case_i++, sym_i);
			}
		}
		emitUnindent(emi);
		emitLine(emi, "}");
	}
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Undef the accessors */
	emitLine(emi, "");
	for (int i = 0; i < emi->data.count; ++i) {
		DataField& dat = emi->data[i]; 
		dat.appendLocalDataFunctionDefines(emi->code, emi->element_name, false);
	}
}
static Node* collectDataMember(Emitter* emi, Node* who, Errors* err, ProgramInfo* info) {
	BasicType type = BasicType_unknown;
	int bitsize = 16; //#TODO this should be 32, but need to make sure 32 actually works correctly (some code may be using things like (1 << bitsize) which would overflow on 32)
	StringRange name;

	/* Parse type */
	if (who->type != Node_identifier) { err->add(who->tok, TempStr("Data member type '%.*s' is not valid.", who->tok.len, who->tok.str)); return NULL; }
	for (int i = 0; i < ARRSIZE(basic_type_strings); ++i) {
		if (basic_type_strings[i] == StringRange(who->tok.str, who->tok.len)) {
			type = BasicType(i);
			break;
		}
	}
	if (type == BasicType_unknown) { err->add(who->tok, TempStr("Data member type '%.*s' is not recognized.", who->tok.len, who->tok.str)); return NULL; }
	if (!(type == BasicType_Unsigned || type == BasicType_Int)) { err->add(who->tok, TempStr("Data member type '%.*s' is not supported yet :(", who->tok.len, who->tok.str)); return NULL; }
	if (!who->sib) { err->add(who->tok, "Data member type must be followed by bitsize or name."); return NULL; }
	who = who->sib;

	/* Parse optional bitsize */
	if (who->type == Node_parens) {
		Node* lit = who->kid;
		if (!lit || lit->type == Node_parens_end) { err->add(who->tok, "Bitsize of data member is missing."); return NULL; }
		if (lit->type != Node_integer_literal) { err->add(lit->tok, TempStr("Bitsize '%.*s' must be an integer literal.", lit->tok.len, lit->tok.str)); return NULL; }
		bitsize = lit->tok.int_lit;
		if (!who->sib) { err->add(who->tok, "Data member must have a name following type and bitsize."); return NULL; }
		who = who->sib;
	}

	/* Parse name */
	if (who->type != Node_identifier) { err->add(who->tok, TempStr("Data member name '%.*s' is not valid.", who->tok.len, who->tok.str)); return NULL; }
	name = StringRange(who->tok.str, who->tok.len);

	DataField& mem = emi->data.push();
	mem.name = name;
	mem.type = type;
	mem.bitsize = bitsize;
	mem.global_offset = NO_OFFSET;
	mem.internal = false;
	return who->sib;
}
static void elementStart(Emitter* emi_decl, Emitter* emi, Node* who, Errors* err, ProgramInfo* info)  {
	emi->element_name = who->str_val;
	emi->super_name = { };
	emi->ruleset_idx = 1;
	emi->data.clear();
	
	/* Collect Data Members and check for Super */
	Node* who_kid = who->kid;
	while (who_kid) {
		if (who_kid->type == Node_data) {
			Node* member = who_kid->kid;
			while (member)
				member = collectDataMember(emi, member, err, info);
		} else if (who_kid->type == Node_super) {
			emi->super_name = who_kid->str_val;
		}
		who_kid = who_kid->sib;
	}

	/* Pick offsets, avoiding straddling component boundaries */
	dataAddInternalAndPickOffsets(emi->data);

	/* Write header */
	emitHeader(emi, 2, '=', '|', StringRange(who->tok.str, who->tok.len)); 
	emitLine(emi, "");

	emitDirectiveLine(emi, who->tok);

	/* Write out the accessors */
	if (emi->data.count > NUM_INTERNAL_DATA_MEMBERS) {
		emitHeader(emi_decl, 1, '+', '+', StringRange(TempStr("Data Member Accessors for %.*s", emi->element_name.len, emi->element_name.str)));  
		emitLine(emi_decl, "");
	}
	for (int i = 0; i < emi->data.count; ++i) {
		DataField& dat = emi->data[i]; 
		dat.appendDataFunctions(emi_decl->code, emi->element_name);
		dat.appendLocalDataFunctionDefines(emi->code, emi->element_name, true);
	}

	/* Copy data layout for external use */
	for (int i = 0; i < info->elems.count; ++i) {
		if (info->elems[i].name == emi->element_name) {
			info->elems[i].data = emi->data;
			break;
		}
	}
}
static void rulesetStart(Emitter* emi, Node* who) {
	emi->rule_idx = 1;
	clearBinds(emi);
	emitHeader(emi, 1, '-', '|', StringRange(who->tok.str, who->tok.len)); 
}
static void emitNode(Emitter* emi_decl, Emitter* emi, Node* who, Errors* err, ProgramInfo* info) {
	Node* next = who->sib;

	// Re-think this logic. Seems like going forward we will need to know what section we're in to deliver good errors.
	if (who->type == Node_element) {
		elementStart(emi_decl, emi, who, err, info);
	} else if (who->type == Node_rules) {
		rulesetStart(emi, who);
	} else if (who->type == Node_diagram) {
		emitLine(emi, "/* Rule %d:\n................................................................................\n", emi->rule_idx);
		emitLine(emi, "%.*s", who->tok.len, who->tok.str);
		emitLine(emi, "\n................................................................................\n*/\n");
		emitRule(emi, who, err);
	} else if (who->type == Node_keyword) {
		if (isPhaseNode(who)) {
			next = emitPhase(emi, who, err);
		} else {
			err->add(who->tok, TempStr("Keyword '%.*s' is out of place.", who->tok.len, who->tok.str));
			who = NULL;
		}
	} else if (emi->rule_idx > 0) { // inside Rules block
		err->add(who->tok, TempStr("Sentential form '%.*s' is out of place. (Missing ' ' in first column of spatial form?)", who->tok.len, who->tok.str));
		who = NULL;
	}

	if (who) {
		if (who->kid) emitNode(emi_decl, emi, who->kid, err, info);
		if (who->type == Node_rules) emitRuleset(emi);
		if (who->type == Node_element) emitElement(emi, who);
		if (next) emitNode(emi_decl, emi, next, err, info);
	}
}
static void emitElementTypeDecls(Emitter* emi, Node* who, Errors* err, ProgramInfo* info) {
	{
		emitLine(emi, "#define Void 0");
		ElementInfo& einfo = info->elems.push();
		einfo.name = "Void";
		einfo.symbol = "  ";
		einfo.color = 0xffff00ff;
	}
	{
		emitLine(emi, "#define Empty 1");
		ElementInfo& einfo = info->elems.push();
		einfo.name = "Empty";
		einfo.symbol = "Em";
		einfo.color = 0xff000000;
	}
	int t = 2;
	while (who) {
		if (who->type == Node_element) {
			emitLine(emi, "#define %.*s %d", who->str_val.len, who->str_val.str, t++);
			ElementInfo& einfo = info->elems.push();
			einfo.name = who->str_val;
			einfo.color = 0xffffff00;
			einfo.symbol = "??";
			Node* kid = who->kid;
			while (kid) {
				if (kid->type == Node_metadata_color) {
					einfo.color = 0; // ABGR from ARGB
					einfo.color |= ((kid->unsigned_val >>  0) & 0xff) << 16; // B
					einfo.color |= ((kid->unsigned_val >>  8) & 0xff) <<  8; // G
					einfo.color |= ((kid->unsigned_val >> 16) & 0xff) <<  0; // R
					einfo.color |= ((kid->unsigned_val >> 24) & 0xff) << 24; // A
				} else if (kid->type == Node_metadata_symbol) {
					einfo.symbol = kid->str_val;
				}
				kid = kid->sib;
			}
		}
		who = who->sib;
	}
	emitLine(emi, "#define TYPE_COUNT %d", t);
}
static void emitInheritanceDecls(Emitter* emi, Node* who, Errors* err, ProgramInfo* info) {
	while (who) {
		if (who->type == Node_element)
			emitLine(emi, "bool %.*s_behave();", who->str_val.len, who->str_val.str);
		who = who->sib;
	}
}
static void emitElementDispatches(Emitter* emi, Node* who_first, Errors* err) {
	/* Behavior */
	emitLine(emi, "void _BEHAVE_DISPATCH(uint type) {");
	emitIndent(emi);
	emitLine(emi, "switch(type) {");
	emitIndent(emi);
	{
		Node* who = who_first;
		while (who) {
			if (who->type == Node_element)
				emitLine(emi, "case %.*s: %.*s_EVENT_START(); %.*s_behave(); break;", who->str_val.len, who->str_val.str, who->str_val.len, who->str_val.str, who->str_val.len, who->str_val.str);
			who = who->sib;
		}
	}
	emitLine(emi, "default: break;"); // needed for Empty and Void too
	emitUnindent(emi);
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Color */
	emitLine(emi, "ARGB _COLOR_DISPATCH(uint type) {");
	emitIndent(emi);
	emitLine(emi, "switch(type) {");
	emitIndent(emi);
	emitLine(emi, "case Void: return uvec4(255,255,0,255);");
	emitLine(emi, "case Empty: return uvec4(255,0,0,0);");
	{
		Node* who = who_first;
		while (who) {
			if (who->type == Node_element)
				emitLine(emi, "case %.*s: return %.*s_getColor(0);", who->str_val.len, who->str_val.str, who->str_val.len, who->str_val.str);
			who = who->sib;
		}
	}
	emitLine(emi, "default: return uvec4(255,0,255,255);");
	emitUnindent(emi);
	emitLine(emi, "}");
	emitUnindent(emi);
	emitLine(emi, "}");
}


void emitForwardDeclarationsAndTypes(Emitter* emi, Node* who, Errors* err, ProgramInfo* info) {
	emi->code.clear();

	/* Type defines */
	emitHeader(emi, 1, '+', '+', StringRange("Type Defines"));  
	emitLine(emi, "");
	emitElementTypeDecls(emi, who->kid, err, info);

	/* Forward decls needed by data member functions */
	emitHeader(emi, 1, '+', '+', StringRange("Forward Declarations for Data Members"));  
	emitLine(emi, "");
	emitLine(emi, "Atom ew(SiteNum i);");
	emitLine(emi, "void ew(SiteNum i, Atom S);");

	/* Forward decls needed by inheritance calls */
	emitHeader(emi, 1, '+', '+', StringRange("Forward Declarations for Inheritance")); 
	emitLine(emi, "");
	emitInheritanceDecls(emi, who->kid, err, info);
}
void emitElements(Emitter* emi_decl, Emitter* emi_elem, Node* who, Errors* err, ProgramInfo* info) {
	emi_elem->code.clear();

	/* Core code */
	emitNode(emi_decl, emi_elem, who->kid, err, info);

	/* Dispatch methods */
	emitHeader(emi_elem, 1, '+', '+', StringRange("Dispatches"));  
	emitLine(emi_elem, "\n");
	emitElementDispatches(emi_elem, who->kid, err);
}
