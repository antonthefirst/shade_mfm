#pragma once
#include "core/log.h"  // for ARRSIZE :(
#include "core/vec4.h" // for colors
#include "core/string_range.h"
#include "core/container.h"

#include "splat_compiler.h"
#include "data_fields.h" //#TODO clean up, shouldn't have to include so much

////////////////////////////////////////////////
// Shared Datatypes and Defines
////////////////////////////////////////////////

#define COMP_COUNT_MAX 96

#define DIAGRAM_IMG_W 32
#define DIAGRAM_IMG_H 16
struct DiagramImg {
	char mem[DIAGRAM_IMG_H][DIAGRAM_IMG_W];
	ivec2 dims;
	DiagramImg() { clear(); }
	void clear() { memset(this, ' ', sizeof(DiagramImg)); }
	char& operator()(int x, int y) {
		assert(x >= 0 && x < DIAGRAM_IMG_W && y >= 0 && y < DIAGRAM_IMG_H);
		return mem[y][x];
	}
	const char& operator()(int x, int y) const {
		assert(x >= 0 && x < DIAGRAM_IMG_W && y >= 0 && y < DIAGRAM_IMG_H);
		return mem[y][x];
	}
	char& operator()(ivec2 c) { return operator()(c.x, c.y); }
	const char& operator()(ivec2 c) const { return operator()(c.x, c.y); }
	bool inImage(int x, int y) const { return x >= 0 && x < DIAGRAM_IMG_W && y >= 0 && y < DIAGRAM_IMG_H; }
	bool inImage(ivec2 c) const { return inImage(c.x, c.y); }
};

struct Lexer;
struct Node;
struct Parser;
struct Errors;
struct ProgramInfo;

////////////////////////////////////////////////
// Lexer
////////////////////////////////////////////////

enum Keyword {
	Keyword_isa,
	Keyword_given,
	Keyword_vote,
	Keyword_check,
	Keyword_change,
};

enum TokType { // windows headers steal TokenType
	Token_blank_line,
	Token_section_header,
	Token_spatial_form,
	Token_newline,
	Token_identifier,
	Token_number,
	Token_open_paren,
	Token_close_paren,
	Token_open_bracket,
	Token_close_bracket,
	Token_open_brace,
	Token_close_brace,
	Token_string,
	Token_semicolon,
	Token_colon,
	Token_dot,
	Token_comma,
	Token_assign,

	// operators
	Token_equals,

	// keywords
	Token_keyword,

	Token_metadata,
	Token_splat_comment,
	Token_hash,
	Token_end_of_stream,
	Token_unknown,
	Token_error, //for handling errors
	Token_skip,  //for skipping past complex whitespace
	Token_count
};

inline const char* toStr(TokType t) {
	switch (t) {
	case Token_blank_line: return "blank line";
	case Token_section_header: return "section header";
	case Token_spatial_form: return "spatial form";
	case Token_newline: return "newline";
	case Token_identifier: return "identifier";
	case Token_number: return "number";
	case Token_open_paren: return "open paren";
	case Token_close_paren: return "close paren";
	case Token_open_bracket: return "open bracket";
	case Token_close_bracket: return "close bracket";
	case Token_open_brace: return "open brace";
	case Token_close_brace: return "close brace";
	case Token_string: return "string";
	case Token_semicolon: return "semicolon";
	case Token_colon: return "colon";
	case Token_dot: return "dot";
	case Token_comma: return "comma";
	case Token_assign: return "assign";
	case Token_equals: return "equals";
	case Token_keyword: return "keyword";
	case Token_metadata: return "metadata";
	case Token_splat_comment: return "splat comment";
	case Token_hash: return "hash";
	case Token_end_of_stream: return "end of stream";
	case Token_error: return "error";
	case Token_skip: return "skip";
	case Token_unknown: return "unknown";
	default: return "ERROR TOKEN";
	}
}

struct Token {
	TokType type;
	const char*  str;
	size_t len;
	int line_num;
	ivec2 minc; // min 2D corner of spatial diagram
	ivec2 maxc; // max
	Keyword key;    // keyword

	// literal data
	int int_lit;
};

struct Lexer {
	const char* at;
	const char* end;
	const char* line_start;
	int line_num;
	int brace_depth;
	Lexer(const char* str_start, const char* str_end) {
		at = line_start = str_start;
		end = str_end;
		line_num = 1;
		brace_depth = 0;
	}
};

inline bool isEndOfLine(char c) {
	return c == '\n';
}
inline bool isWhitespace(char c) {
	return c == ' ';
}
inline bool isEndOfStream(Lexer* lex) {
	return (lex->at == lex->end) || (lex->at[0] == '\0');
}
inline bool isLowercaseAlpha(char c) {
	return c >= 'a' && c <= 'z';
}
inline bool isUppercaseAlpha(char c) {
	return c >= 'A' && c <= 'Z';
}
inline bool isAlpha(char c) {
	return isLowercaseAlpha(c) || isUppercaseAlpha(c);
}
inline bool isNumeric(char c) {
	return c >= '0' && c <= '9';
}
inline bool isHexadecimal(char c) {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
inline void skipWhitespace(const char** str, const char* str_end) {
	while(*str != str_end && isWhitespace(**str)) *str += 1;
}
inline void skipAlpha(const char** str, const char* str_end) {
	while (*str != str_end && isAlpha(**str)) *str += 1;
}
inline void skipAlphaNumeric(const char** str, const char* str_end) {
	while (*str != str_end && (isAlpha(**str) || isNumeric(**str))) *str += 1;
}
inline void skipUntilEndOfLine(const char** str, const char* str_end) {
	while (*str != str_end && !isEndOfLine(**str)) *str += 1;
}
inline int intDigitFromHex(char c) {
	if (c >= '0' && c <= '9') return c - '0';
	else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	else if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	else return 0;
}

Token lexToken(Lexer* z, Errors* err);
inline void lexHackEnableInjectedMode(Lexer* lex) { lex->brace_depth = -1; }

////////////////////////////////////////////////
// Errors
////////////////////////////////////////////////

inline vec4 COLOR_COMPONENT(int c) {
	vec3 comp_cols[] = { vec3(0.2f), vec3(1.0f,0.3f,0.3f), vec3(0.3f,1.0f,0.3f), vec3(0.3f,0.3f,1.0f), vec3(1.0f,1.0f,0.3f), vec3(0.3f,1.0f,1.0f), vec3(1.0f,0.3f,1.0f) };
	int i = c == 0 ? 0 : ((c - 1) % (ARRSIZE(comp_cols) - 1) + 1);
	return vec4(comp_cols[i], 1.0f);
}

struct ParserError {
	String msg;
	Token tok;
	bool show_diagram;
	DiagramImg img;
	vec4 img_color[DIAGRAM_IMG_H][DIAGRAM_IMG_W];
	ParserError() { memset(this, 0, sizeof(ParserError)); }
};

struct Errors {
	Bunch<ParserError> errors;
	void add(Token tok, const char* msg);
};

void printNode(Node* who, int depth = 0);
void printErrors(Errors* err);
void printCode(StringRange code);


////////////////////////////////////////////////
// Parser
////////////////////////////////////////////////

enum NodeType {
	Node_unknown,
	Node_diagram,
	Node_braces,
	Node_braces_end,
	Node_parens,
	Node_parens_end,
	Node_end_statement,
	Node_identifier,
	Node_keyword,
	Node_integer_literal,
	Node_element,
	Node_rules,
	Node_methods,
	Node_data,
	Node_metadata_symmetries,
	Node_metadata_radius,
	Node_metadata_symbol,
	Node_metadata_color,
	Node_metadata_author,
	Node_metadata_licence,
	Node_error,
};

inline const char* toStr(NodeType t) {
	switch (t) {
	case Node_unknown: return "unknown";
	case Node_diagram: return "diagram";
	case Node_braces: return "braces";
	case Node_braces_end: return "braces end";
	case Node_parens: return "parens";
	case Node_parens_end: return "parens end";
	case Node_end_statement: return "end statement";
	case Node_identifier: return "identifier";
	case Node_keyword: return "keyword";
	case Node_integer_literal: return "integer literal";
	case Node_element: return "element";
	case Node_rules: return "rules";
	case Node_methods: return "methods";
	case Node_data: return "data";
	case Node_metadata_symmetries: return "symmetries metadata";
	case Node_metadata_radius: return "radius metadata";
	case Node_metadata_symbol: return "symbol metadata";
	case Node_metadata_color: return "color metadata";
	case Node_metadata_author: return "author metadata";
	case Node_metadata_licence: return "licence metadata";
	case Node_error: return "error";
	default: return "INVALID NODE";
	}
}

struct DiagramPiece {
	char comp_id;
	ivec2 start, end;
};
struct Diagram {
	char lhs[41];
	char rhs[41];
};

struct Node {
	NodeType type;
	Node* kid;
	Node* sib;
	Token tok;

	// various possible non-literal data that were parsed from one or more tokens
	Diagram diag; // #OPT maybe change this to a pointer and store separately
	int int_val;
	StringRange str_val;
	unsigned int unsigned_val;

	// debug data, for now
	DiagramImg full_img;
	DiagramImg lhs_img;
	DiagramImg rhs_img;
	DiagramImg comp_img;
};

struct Parser {

};

inline bool isSectionHeaderNode(Node* who) {
	return who->type == Node_element || who->type == Node_rules || who->type == Node_methods || who->type == Node_data;
}

Node* makeNode(NodeType, Token);
void freeNode(Node* who);
void addKid(Node* par, Node* kid);
void transformAST(Node* par, Node* who, Errors* err);

Node* parseGroups(Parser* par, Lexer* lex, Errors* err);


////////////////////////////////////////////////
// Emit
////////////////////////////////////////////////

struct KeycodeBinding {
	StringRange isa;
	Node* block;
	Node* expression;
};

struct Emitter {
	// running state
	Bunch<DataField> data;
	KeycodeBinding  given[128]; // binds, 7 bit ASCII
	KeycodeBinding   vote[128];
	KeycodeBinding  check[128]; 
	KeycodeBinding change[128]; 
	int            nsites[128];
	bool         lhs_used[128];
	bool         rhs_used[128];
	StringRange element_name = { };
	int ruleset_idx = 0;
	int rule_idx = 0;
	int indent = 0;

	// global state
	int vote_sum_count_max = 0;

	int element_uid = 0;
	String code;
};

void emitForwardDeclarationsAndTypes(Emitter* emi, Node* who, Errors* err, ProgramInfo* info);
void emitElements(Emitter* emi, Node* who, Errors* err, ProgramInfo* info);

void compile(const char* code_start, const char* code_end, Emitter* emi, Errors* err, ProgramInfo* info);