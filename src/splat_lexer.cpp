#include "imgui/imgui.h"
#include "core/string_range.h"
#include "splat_internal.h"

static StringRange keyword_strings[] = {
	StringRange("isa"),
	StringRange("given"),
	StringRange("vote"),
	StringRange("check"),
	StringRange("change"),
};
static Keyword keywords[] = {
	Keyword_isa,
	Keyword_given,
	Keyword_vote,
	Keyword_check,
	Keyword_change,
};

void eatAllWhitespace(Lexer* z) {
	while (isWhitespace(z->at[0])) z->at += 1;
}
Token lexIdentifier(Lexer* z) {
	Token t = {};
	t.line_num = z->line_num;
	t.str = z->at;
	while (true) {
		if (isAlpha(z->at[0]) || isNumeric(z->at[0]) || z->at[0] == '_') { // eat the identifier
			z->at += 1;
		} else {
			break;
		}
	}
	t.type = Token_identifier;
	t.len = z->at - t.str;
	return t;
}
Token lexNumber(Lexer* z) {
	Token t = { };
	t.line_num = z->line_num;
	t.str = z->at;
	bool found_hex = false;
	while(true) {
		if (isNumeric(z->at[0])) {
			z->at += 1;
		} else if (z->at[0] == 'u' || z->at[0] == 'U' ||
			       z->at[0] == 'b' || z->at[0] == 'B') {
			if (found_hex) {
				//#ERROR hex is unsigned by default
				break;
			}
			z->at += 1;
			break;
		} else if (z->at[0] == 'x' || z->at[0] == 'X') {
			if (found_hex) {
				//#ERROR two x's in one number
				break;
			}
			z->at += 1;
			found_hex = true;
		} else {
			break;
		}
	}
	t.int_lit = strtol(t.str, 0, 0);
	t.type = Token_number;
	t.len = z->at - t.str;
	return t;
}
void eatSPLATComment(Lexer* z) {
	while (true) {
		if (isEndOfStream(z)) {
			//#ERROR
			break;
		} else if (isEndOfLine(z->at[0])) {
			break;
		} else {
			z->at += 1;
		}
	}
}
Token lexSpatialForm(Lexer* z, Errors* err) {
	Token t = { };
	t.line_num = z->line_num;
	t.str = z->at;
	t.type = Token_spatial_form;
	t.minc = ivec2(INT32_MAX);
	t.maxc = ivec2(0);
	ivec2 idx = ivec2(0,0);
	while(true) {
		// parse one line at a time
		bool is_blank = true;       // is the entire line blank
		bool is_sequential = false; // did we hit a sequential statement?
		while(true) {
			if (isEndOfStream(z)) {
				// eos implies a following blank line so end here.
				// leave eos in Lexer, the non-spatial parser will take care of it.
				is_blank = true; 
				break;
			} else if (isEndOfLine(z->at[0])) {
				z->at += 1;
				z->line_start = z->at;
				z->line_num += 1;
				idx.x = 0;
				idx.y += 1;
				break;
			} else if (z->at[0] == '\t') {
				ParserError& e = err->errors.push();
				e.tok = t;
				e.msg.set("Illegal tab character (\\t).");
				Token err_t = { };
				err_t.type = Token_error;
				return err_t;
			} else if (z->at == z->line_start && z->at[0] != ' ') {
				// hit a non-spatial form
				is_sequential = true;
				break;
			} else {
				if (z->at[0] == ' ') {

				} else {

					is_blank = false;
					t.minc = minvec(t.minc, idx);
					t.maxc = maxvec(t.maxc, idx);
				}
				z->at += 1;
				idx.x += 1;
			}
		}
		if (is_blank || is_sequential) {
			break;
		}
	}
	t.len = z->at - t.str;
	if (t.minc == ivec2(INT32_MAX) && t.maxc == ivec2(0)) // all blank, just skip it.
		t.type = Token_skip;
	return t;
}
Token lexSectionHeader(Lexer* lex, Errors* err) {
	Token tok;
	tok.line_num = lex->line_num;
	tok.str = lex->at;
	while (!isEndOfStream(lex) && !isEndOfLine(lex->at[0])) lex->at += 1;
	tok.len = lex->at - tok.str;
	tok.type = Token_section_header;
	return tok;
}

bool isInjecting(Lexer* z) {
	return z->brace_depth != 0;
}

Token lexToken(Lexer* z, Errors* err) {
	if (z->at == z->end) {
		Token t = {};
		t.str = "EOS";
		t.len = 3;
		t.type = Token_end_of_stream;
		return t;
	}
	if (!isInjecting(z) && z->line_start == z->at && z->at[0] == ' ') {
		return lexSpatialForm(z, err);
	}
	eatAllWhitespace(z);
	Token t = {};
	t.line_num = z->line_num;
	t.str = z->at;
	t.len = 1;
	t.type = Token_unknown;
	switch (z->at[0]) {
	case '\0': t.type = Token_end_of_stream; assert(false); break; // expecting to end on 'end' pointer, not a null terminator (no harm though)
	case '(':  t.type = Token_open_paren;    z->at += 1; break;
	case ')':  t.type = Token_close_paren;   z->at += 1; break;
	case '[':  t.type = Token_open_bracket;  z->at += 1; break;
	case ']':  t.type = Token_close_bracket; z->at += 1; break;
	case ';':  t.type = Token_semicolon;     z->at += 1; break;
	case ':':  t.type = Token_colon;         z->at += 1; break;
	case ',':  t.type = Token_comma;         z->at += 1; break;
	case '{':  t.type = Token_open_brace;    z->at += 1; z->brace_depth += 1; break;
	case '}':  t.type = Token_close_brace;   z->at += 1; z->brace_depth -= 1; break;
	case '\t': 
	{
		if (!isInjecting(z))
			err->add(t, "Illegal tab character (\\t).");
		z->at += 1;
	} break;
	case '\n':
	{
		t.type = Token_newline;
		z->at += 1;
		z->line_start = z->at;
		z->line_num += 1;
	} break;

	case '#':  
	{
		// need to allow through #defines for gpulam
		z->at += 1;
		t.str = z->at; // remove the '#' from the token
		if (!isEndOfStream(z) && isInjecting(z)) {
			while(!isEndOfStream(z) && !isEndOfLine(z->at[0]) && !isWhitespace(z->at[0])) z->at += 1;
			t.type = Token_hash;
			t.len = z->at - t.str;
		} else {
			t.type = Token_splat_comment;
			eatSPLATComment(z);
			t.len = z->at - t.str;
		}
	} break;

	case '=':
	{
		if (z->line_start == z->at && !isInjecting(z)) {
			t = lexSectionHeader(z, err);
		} else {
			z->at += 1;
			if (!isEndOfStream(z) && z->at[0] == '=') {
				z->at += 1;
				t.type = Token_equals;
			} else {
				t.type = Token_assign;
			}
			t.len = z->at - t.str;
		}
	} break;

	case '"':
	{
		//#TODO should probably error if i didn't hit a " to close the string
		z->at += 1; // skip over quote
		t.str = z->at;
		while (!isEndOfStream(z) && !z->at[0] == '"') {
			if (z->at[0] == '\\') {
				z->at += 1;                  // advance past escape
				if (isEndOfStream(z)) // sudden termination?
					break;
			}
			z->at += 1; // advance past character
		}
		t.type = Token_string;
		t.len = z->at - t.str;
		if (z->at[0] == '"')
			z->at += 1; // skip over closing quote, if that's what stopped us
	} break;

	case '\\':
	{
		if (!isInjecting(z)) { // only parse as metadata if we're not injecting
			z->at += 1;
			t.str = z->at;
			while (!isEndOfStream(z) && !isEndOfLine(z->at[0])) {
				z->at += 1;
			}
			t.len = z->at - t.str;
			t.type = Token_metadata;
			break;
		}
	} 

	default:
	{
		if (isAlpha(z->at[0])) {
			t = lexIdentifier(z);
			StringRange r = StringRange(t.str, t.len);
			for (int i =  0; i < ARRSIZE(keyword_strings); ++i) {
				if (r == keyword_strings[i]) {
					t.type = Token_keyword;
					t.key = keywords[i];
					break;
				}
			}
		}
		else if (isNumeric(z->at[0])) {
			t = lexNumber(z);
		}
		else {
			//log("Skipping unknown char '%c'\n", z->at[0]);
			z->at += 1; // skip unknown character.
		}
	} break;
	};
	return t;
}
