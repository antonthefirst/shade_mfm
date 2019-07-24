#include "splat_internal.h"
#include "core/string_range.h"


// #TODO need to replace this with a proper thing i think...
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
static const char* findNodeEnd(Node* who, const char* end = 0) {
	end = max(end, who->tok.str + who->tok.len);
	if (who->kid) end = findNodeEnd(who->kid, end);
	if (who->sib) end = findNodeEnd(who->sib, end);
	return end;
}
void emitBlock(Emitter* emi, Node* who) {
	if (!who->kid) return; // empty
	emitLine(emi, "/* Injected from line %d */", who->tok.line_num);
	const char* block_end = findNodeEnd(who->kid);
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
	emitLine(emi, "bool " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "(SiteNum _cursn) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitVoteKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "void " RULENAME_FORMAT VOTE_KEYCODE_FORMAT "(SiteNum _cursn, inout int _nvotes, inout SiteNum _winsn) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitCheckKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "bool " RULENAME_FORMAT CHECK_KEYCODE_FORMAT "(int _nvotes) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}
void emitChangeKeycodeDecl(Emitter* emi, int keycode) {
	emitLine(emi, "void " RULENAME_FORMAT CHANGE_KEYCODE_FORMAT "(SiteNum _cursn, SiteNum _winsn, Atom _winatom) { /* %c */", RULENAME_FORMAT_ARGS, keycode, keycode);
}

static void clearBinds(Emitter* emi) {
	memset(emi->given,  0, sizeof(Emitter::given));
	memset(emi->vote,   0, sizeof(Emitter::vote));
	memset(emi->check,  0, sizeof(Emitter::check));
	memset(emi->change, 0, sizeof(Emitter::change));
}
static void elementStart(Emitter* emi, StringRange name)  {
	emi->element_name = name;
	emi->ruleset_idx = 1;
}
static void rulesetStart(Emitter* emi) {
	emi->rule_idx = 1;
	clearBinds(emi);
}

static void emitRule(Emitter* emi, Node* who, Errors* err) {
	memset(emi->nsites, 0, sizeof(Emitter::nsites));
	memset(emi->lhs_used, 0, sizeof(Emitter::lhs_used));
	memset(emi->rhs_used, 0, sizeof(Emitter::rhs_used));
	for (int i = 0; i < 41; ++i) {
		if (who->diag.lhs[i] != ' ')  {
			emi->lhs_used[who->diag.lhs[i]] = true;
			emi->nsites[who->diag.lhs[i]] += 1;
		}
		if (who->diag.rhs[i] != ' ')
			emi->rhs_used[who->diag.rhs[i]] = true;
	}

	/* Given keycode binds */
	for (int i = 0; i < ARRSIZE(Emitter::given); ++i) {
		if (emi->lhs_used[i]) {
			if (emi->given[i].block || emi->given[i].expression) {
				emitLine(emi, RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject() {", RULENAME_FORMAT_ARGS, i);
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
					emitLine(emi, "return is(ew(_cursn), %.*s) && " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject();", emi->given[i].isa.len, emi->given[i].isa.str, RULENAME_FORMAT_ARGS, i);
				else
					emitLine(emi, "return " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject();", RULENAME_FORMAT_ARGS, i);
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
	int nvotes_count = 0;
	int keycode_from_nvote[41] = { };
	for (int i = 0; i < ARRSIZE(Emitter::vote); ++i) {
		if (emi->lhs_used[i]) {
			if (emi->vote[i].block || emi->vote[i].expression) {
				emitLine(emi, RULENAME_FORMAT VOTE_KEYCODE_FORMAT "_inject() {", RULENAME_FORMAT_ARGS, i);
				if (emi->vote[i].block)
					emitBlock(emi, emi->vote[i].block);
				else
					emitLine(emi, "return EXPRESSION;");
				emitLine(emi, "}");
			}
			emitVoteKeycodeDecl(emi, i);
			emitIndent(emi);
			if (emi->vote[i].block) {
				if (emi->vote[i].isa.len)
					emitLine(emi, "int myvotes = is(ew(_cursn), %.*s) ? " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject() : 0;", emi->vote[i].isa.len, emi->vote[i].isa.str, RULENAME_FORMAT_ARGS, i);
				else
					emitLine(emi, "int myvotes = " RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "_inject();", RULENAME_FORMAT_ARGS, i);
			} else if (emi->vote[i].isa.len) {
				emitLine(emi, "int myvotes = is(ew(_cursn), %.*s) ? 1 : 0;", emi->vote[i].isa.len, emi->vote[i].isa.str);
			} else {
				emitLine(emi, "int myvotes = 1;");
			}
			emitLine(emi, "_nvotes += myvotes;");
			emitLine(emi, "if (random_create(int(_nvotes)) < myvotes) { _winsn = _cursn; }"); //#CAREFUL random_create(int) is what does clamping on 0, not random_create(unsigned)!
			emitUnindent(emi);
			emitLine(emi, "}");
			keycode_from_nvote[nvotes_count] = i;
			nvotes_count += 1;
		}
	}
	emi->vote_sum_count_max = max(emi->vote_sum_count_max, nvotes_count);

	/* Check keycode binds */
	for (int i = 0; i < nvotes_count; ++i) {
		emitCheckKeycodeDecl(emi, keycode_from_nvote[i]);
		emitIndent(emi);
		if (emi->check[keycode_from_nvote[i]].block) {
			emitLine(emi, "const int _nsites = %d;", emi->nsites[keycode_from_nvote[i]]);
			emitBlock(emi, emi->check[keycode_from_nvote[i]].block);
		} else {
			emitLine(emi, "return _nvotes > 0;", i); 
		}
		emitUnindent(emi);
		emitLine(emi, "}");
	}

	/* Change keycode binds */
	for (int i = 0; i < ARRSIZE(Emitter::change); ++i) {
		if (emi->rhs_used[i]) {
			emitChangeKeycodeDecl(emi, i);
			emitIndent(emi);
			if (emi->change[i].block) {
				emitBlock(emi, emi->change[i].block);
			} else if (emi->change[i].isa.len) {
				emitLine(emi, "ew(_cursn, new(%.*s));", emi->change[i].isa.len, emi->change[i].isa.str);
			//} else if (i == '@') {
			//	emitLine(emi, "ew(_cursn, ew(0));");
			} else if (i == '_') {
				emitLine(emi, "ew(_cursn, new(Empty));");
			} else if (i == '?') {
				emitLine(emi, "/* Nothing to do */"); 
			} else if (i == '.') {
				emitLine(emi, "/* Nothing to do */");
			} else {
				emitLine(emi, "ew(_cursn, _winatom); // Default");
			}
			emitUnindent(emi);
			emitLine(emi, "}");
		}
	}

	/* Given */
	emitLine(emi, "bool " RULENAME_FORMAT "_given() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	for (int i = 0; i < 41; ++i) {
		if (who->diag.lhs[i] != ' ') {
			emitLine(emi, "if (!" RULENAME_FORMAT GIVEN_KEYCODE_FORMAT "(%d)) return false;", RULENAME_FORMAT_ARGS, who->diag.lhs[i], i);
		}
	}
	emitLine(emi, "return true;");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Vote */
	emitText(emi, "void " RULENAME_FORMAT "_vote(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "inout int _nvotes_%d, inout SiteNum _winsn_%d, inout Atom _winatom_%d%s", i, i, i, i < (nvotes_count - 1) ? ", " : "");
	emitLine(emi, ") {");
	emitIndent(emi);
	for (int i = 0; i < nvotes_count; ++i) {
		emitLine(emi, "_nvotes_%d = 0; /* %c */", i, keycode_from_nvote[i]);
		emitLine(emi, "_winsn_%d = 63;", i);
	}
	for (int i = 0; i < 41; ++i) {
		if (who->diag.lhs[i] != ' ') { 
			int vote_idx = 0;
			for (int v = 0; v < ARRSIZE(Emitter::vote); ++v) {
				if (who->diag.lhs[i] == v)
					break;
				if (emi->lhs_used[v]) {
					vote_idx += 1;
				}
			}
			emitLine(emi, RULENAME_FORMAT VOTE_KEYCODE_FORMAT "(%d, _nvotes_%d, _winsn_%d); /* %c */", RULENAME_FORMAT_ARGS, who->diag.lhs[i], i, vote_idx, vote_idx, who->diag.lhs[i]);
		}
	}
	for (int i = 0; i < nvotes_count; ++i) {
		//emitLine(emi, "_winatom_%d = new(Res);", i);
		emitLine(emi, "_winatom_%d = (_winsn_%d != 63) ? ew(_winsn_%d) : new(Void);", i, i ,i);
	}
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Check */
	emitText(emi, "bool " RULENAME_FORMAT "_check(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "int _nvotes_%d%s", i,  i < (nvotes_count - 1) ? ", " : "");
	emitLine(emi, ") {");
	emitIndent(emi);
	for (int i = 0; i < nvotes_count; ++i) {
		char keycode = keycode_from_nvote[i];
		emitLine(emi, "if (!" RULENAME_FORMAT CHECK_KEYCODE_FORMAT "(_nvotes_%d)) return false; /* %c */", RULENAME_FORMAT_ARGS, keycode, i, keycode);
	}
	emitLine(emi, "return true;");
	emitUnindent(emi);
	emitLine(emi, "}");

	/* Change */
	emitText(emi, "void " RULENAME_FORMAT "_change(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "SiteNum _winsn_%d, Atom _winatom_%d%s", i, i,  i < (nvotes_count - 1) ? ", " : "");
	emitLine(emi, ") {");
	emitIndent(emi);
	for (int i = 0; i < 41; ++i) {
		if (who->diag.lhs[i] != ' ') {
			int vote_idx = 0;
			for (int v = 0; v < ARRSIZE(Emitter::vote); ++v) {
				if (who->diag.rhs[i] == v)
					break;
				if (emi->lhs_used[v]) {
					vote_idx += 1;
				}
			}
			emitLine(emi, RULENAME_FORMAT CHANGE_KEYCODE_FORMAT "(%d, _winsn_%d, _winatom_%d); /*  %c -> %c  */", RULENAME_FORMAT_ARGS, who->diag.rhs[i], i, vote_idx, vote_idx, who->diag.lhs[i], who->diag.rhs[i]);
		}
	}
	emitUnindent(emi);
	emitLine(emi, "}");


	/* Final rule */
	emitLine(emi, "bool " RULENAME_FORMAT "() {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	emitLine(emi, "if (" RULENAME_FORMAT "_given()) {", RULENAME_FORMAT_ARGS);
	emitIndent(emi);
	for (int i = 0; i < nvotes_count; ++i) {
		emitLine(emi, "int _nvotes_%d; /* %c */", i, keycode_from_nvote[i]);
		emitLine(emi, "SiteNum _winsn_%d;", i);
		emitLine(emi, "Atom _winatom_%d;", i);
	}
	emitIndentedText(emi, RULENAME_FORMAT "_vote(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "_nvotes_%d, _winsn_%d, _winatom_%d%s", i, i, i, i < (nvotes_count - 1) ? ", " : "");
	emitText(emi, ");\n");
	emitIndentedText(emi, "if (" RULENAME_FORMAT "_check(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "_nvotes_%d%s", i, i < (nvotes_count - 1) ? ", " : "");
	emitText(emi, ")) {\n");
	emitIndent(emi);
	emitIndentedText(emi, RULENAME_FORMAT "_change(", RULENAME_FORMAT_ARGS);
	for (int i = 0; i < nvotes_count; ++i)
		emitText(emi, "_winsn_%d, _winatom_%d%s", i, i, i < (nvotes_count - 1) ? ", " : "");
	emitText(emi, ");\n");
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
	if (!isAlpha(who->tok.str[0]) && !isNumeric(who->tok.str[0])) { err->add(who->tok, TempStr("Phase keycode '%c' (ASCII value %d) is not a printable character.", who->tok.str[0], who->tok.str[0])); return NULL; }
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
	emitLine(emi, "bool " RULESET_FORMAT "() {", RULESET_FORMAT_ARGS);
	emitIndent(emi);
	for (int i = 1; i < emi->rule_idx; ++i) {
		emitLine(emi, "if (" RULENAME_FORMAT "()) return true;", emi->element_name.len, emi->element_name.str, emi->ruleset_idx, i);
	}
	emitLine(emi, "return false;");
	emitUnindent(emi);
	emitLine(emi, "}");

	emi->ruleset_idx += 1;
	emi->rule_idx = 0;
}
void emitElement(Emitter* emi, Node* who) {
	/* Rulesets */
	emitLine(emi, "void " RULEELEMENT_FORMAT "() {", RULEELEMENT_FORMAT_ARGS);
	emitIndent(emi);
	for (int i = 1; i < emi->ruleset_idx; ++i) {
		emitLine(emi, "if (" RULESET_FORMAT "()) return;", emi->element_name.len, emi->element_name.str, i);
	}
	emitUnindent(emi);
	emitLine(emi, "}");
	emi->ruleset_idx = 0;

	/* Default getColor */
	u32 color = 0xffffffff;
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
}

static void emitNode(Emitter* emi, Node* who, Errors* err) {
	Node* next = who->sib;

	if (who->type == Node_element) {
		elementStart(emi, who->str_val);
		emitHeader(emi, 2, '=', '|', StringRange(who->tok.str, who->tok.len)); 
	} else if (who->type == Node_rules) {
		rulesetStart(emi);
		emitHeader(emi, 1, '-', '|', StringRange(who->tok.str, who->tok.len));  
	} else if (who->type == Node_diagram) {
		emitLine(emi, "/* Rule %d:\n................................................................................\n", emi->rule_idx);
		//emitLine(emi, "\n/*******************************************************************************\n");
		emitLine(emi, "%.*s", who->tok.len, who->tok.str);
		emitLine(emi, "\n................................................................................\n*/\n");
		//emitLine(emi, " *******************************************************************************/\n");
		emitRule(emi, who, err);
	} else if (who->type == Node_keyword) {
		if (isPhaseNode(who)) {
			next = emitPhase(emi, who, err);
		}
	}
	if (who) {
		if (who->kid) emitNode(emi, who->kid, err);
		if (who->type == Node_rules && emi->rule_idx > 1) emitRuleset(emi);
		if (who->type == Node_element && emi->ruleset_idx > 1) emitElement(emi, who);
		if (next) emitNode(emi, next, err);
	}
}
static void emitElementTypeDecls(Emitter* emi, Node* who, Errors* err) {
	emitLine(emi, "#define Void 0");
	emitLine(emi, "#define Empty 1");
	int t = 2;
	while (who) {
		if (who->type == Node_element)
			emitLine(emi, "#define %.*s %d", who->str_val.len, who->str_val.str, t++);
		who = who->sib;
	}
	emitLine(emi, "#define TYPE_COUNT %d", t);
	emitLine(emi, "");
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


void emitForwardDeclarationsAndTypes(Emitter* emi, Node* who, Errors* err) {
	emi->code.clear();

	/* Type defines */
	emitHeader(emi, 1, '+', '+', StringRange("Type Defines"));  
	emitLine(emi, "\n");
	emitElementTypeDecls(emi, who->kid, err);
}
void emitElements(Emitter* emi, Node* who, Errors* err) {
	emi->code.clear();

	/* Core code */
	emitNode(emi, who, err);

	/* Dispatch methods */
	emitHeader(emi, 1, '+', '+', StringRange("Dispatches"));  
	emitLine(emi, "\n");
	emitElementDispatches(emi, who->kid, err);
}
