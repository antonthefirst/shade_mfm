#include "imgui/imgui.h"
#include "core/string_range.h"
#include "core/log.h"
#include "core/file_stat.h"
#include "core/container.h"
#include "mfm_utils.h"
#include "splat_internal.h"
#include <stdio.h>

/* DODGY
 - String leaks memory!
*/
static StringRange section_strings[] = {
	StringRange("Element"),
	StringRange("Rules"),
	StringRange("Methods"),
	StringRange("Data"),
};
static NodeType section_types[] = {
	Node_element, // #WARNING: this is used as 'starting enum' in some places, so don't change it from 0's position without fixing that
	Node_rules,
	Node_methods,
	Node_data,
};
static StringRange metadata_strings[] = {
	StringRange("symmetries"),
	StringRange("radius"),
	StringRange("symbol"),
	StringRange("color"),
	StringRange("author"),
	StringRange("licence"),
};
static NodeType metadata_types[] = {
	Node_metadata_symmetries,
	Node_metadata_radius,
	Node_metadata_symbol,
	Node_metadata_color,
	Node_metadata_author,
	Node_metadata_licence,
};
static StringRange symmetry_strings[] = {
	StringRange("all"),
	StringRange("none"),
};
static unsigned int symmetry_bitmasks[] = {
	0xff, // 11111111
	0x00,
};

Node* makeNode(NodeType type, Token tok) {
	Node* n = (Node*)malloc(sizeof(Node));
	memset(n, 0, sizeof(Node));
	n->type = type;
	n->tok = tok;
	return n;
}
Node* makeNode(NodeType type, Token tok, int int_val) {
	Node* n = makeNode(type, tok);
	n->int_val = int_val;
	return n;
}
Node* makeNode(NodeType type, Token tok, unsigned int unsigned_val) {
	Node* n = makeNode(type, tok);
	n->unsigned_val = unsigned_val;
	return n;
}
Node* makeNode(NodeType type, Token tok, StringRange str_val) {
	Node* n = makeNode(type, tok);
	n->str_val = str_val;
	return n;
}
Node* makeNodeError(Token tok) {
	return makeNode(Node_error, tok);
}
void freeNode(Node* who) {
	if (who->kid) {
		freeNode(who->kid);
		who->kid = NULL;
	}
	if (who->sib) {
		freeNode(who->sib);
		who->sib = NULL;
	}
	free(who);
}
void addSib(Node* who, Node* sib) {
	while (who->sib) who = who->sib;
	who->sib = sib;
}
void addKid(Node* par, Node* kid) {
	if (!par->kid) par->kid = kid;
	else addSib(par->kid, kid);
}

DiagramPiece parseDiagramPieceAndClearComponent(Parser* p, DiagramImg* comp_img) {
	DiagramPiece piece;
	piece.comp_id = COMP_COUNT_MAX;
	piece.start = ivec2(INT32_MAX);
	piece.end = ivec2(0);
	for (int x = 0; x < comp_img->dims.x; ++x) {
		bool blank_column = true;
		for (int y = 0; y < comp_img->dims.y; ++y) {
			char c = (*comp_img)(x,y);
			if (c != 0) {
				if (piece.comp_id == COMP_COUNT_MAX) {
					piece.comp_id = c;
				}
				blank_column = false;
			}
			if (c == piece.comp_id) {
				(*comp_img)(x,y) = 0; // clear myself out
				piece.start = minvec(piece.start, ivec2(x,y));
				piece.end = maxvec(piece.end, ivec2(x,y));
			}
		}
		if (piece.comp_id != COMP_COUNT_MAX && blank_column)     // if we started taking, and then hit a blank column
			break;
	}
	return piece;
}

Node* parseSpatialForm(Parser* p, Errors* err, Token t) {
	DiagramImg full_img;
	DiagramImg comp_img;

	memset(comp_img.mem, 0, sizeof(DiagramImg)); // replace with clear(0)

	Node* n = makeNode(Node_diagram, t);
	const char* c = t.str;
	ivec2 idx = ivec2(0,0);
	ivec2 dims = t.maxc - t.minc + ivec2(1);
	full_img.dims = dims;
	comp_img.dims = dims;
	char comp_count = 1;
	// max number of keys is 41 + 41 + 2, so this should be plenty
	char comp_map[COMP_COUNT_MAX];
	memset(comp_map, 0, sizeof(comp_map));
	while (c != (t.str + t.len)) {
		if (isEndOfLine(*c)) {
			idx.x  = 0;
			idx.y += 1;
		} else {
			if (*c != ' ' && idx.x <= t.maxc.x) {
				if (idx.x >= DIAGRAM_IMG_W || idx.y >= DIAGRAM_IMG_H) {
					ParserError& e = err->errors.push();
					e.tok = t;
					e.msg.set(TempStr("Diagram is too large to parse:\n%.*s", t.str, t.len));
					goto END;
				} else {
					ivec2 v = idx - t.minc;
					full_img(v) = *c;
					char xc = v.x > 0 ? comp_img(v - ivec2(1,0)) : 0;
					char yc = v.y > 0 ? comp_img(v - ivec2(0,1)) : 0;
					char dc = (v.x > 0 && v.y > 0) ? comp_img(v - ivec2(1,1)) : 0;
					char rc = (v.x < (dims.x-1)  && v.y > 0) ? comp_img(v + ivec2(+1,-1)) : 0;
					if (xc != 0 || yc != 0 || dc != 0 || rc != 0) {	
						// min of non-zero values
						char min_nonzero = min(yc != 0 ? yc : COMP_COUNT_MAX, dc != 0 ? dc : COMP_COUNT_MAX);
						min_nonzero = min(min_nonzero, char(xc != 0 ? xc : COMP_COUNT_MAX)); 
						min_nonzero = min(min_nonzero, char(rc != 0 ? rc : COMP_COUNT_MAX)); 
						comp_img(v) = min_nonzero;
						// update the mapping of the larger values down to the new smaller value
						if (xc != 0) comp_map[xc] = min_nonzero;
						if (yc != 0) comp_map[yc] = min_nonzero;
						if (dc != 0) comp_map[dc] = min_nonzero;
						if (rc != 0) comp_map[rc] = min_nonzero;

					} else {
						if (comp_count < COMP_COUNT_MAX) {
							comp_img(v) = comp_count;
							comp_map[comp_count] = comp_count; // init to identity mapping
							comp_count += 1;
						} else {
							ParserError& e = err->errors.push();
							e.tok = t;
							e.msg.set(TempStr("Diagram has too many disconnected regions to parse:\n%.*s", t.str, t.len));
							goto END;
						}
					}
				}
			}
			idx.x += 1;
		}
		c += 1;
	}
	// collapse mappings
	// #OPT I feel like this only requires 1 iteration really, but I can't prove it so in case there is a pedantic case I'm missing...
	int loops = 0;
	while (true) {
		bool did_a_collapse = false;
		for (char i = 0; i < comp_count; ++i) {
			if (comp_map[i] != i) {
				char eq = comp_map[comp_map[i]];
				if (comp_map[i] != eq) {
					comp_map[i] = comp_map[eq];
					did_a_collapse = true;
				}
			}
		}
		loops += 1;
		if (!did_a_collapse) break; // stop when no remappings occurred
	}
	if (loops > 2) {
		assert(false); // i'm curious what this can be...
	}

	// renumber mappings to numeric order
	char renumber_map[COMP_COUNT_MAX];
	char unique_count = 0;
	memset(renumber_map, COMP_COUNT_MAX, sizeof(renumber_map));
	for (int i = 0; i < comp_count; ++i) {
		if (renumber_map[comp_map[i]] == COMP_COUNT_MAX)
			renumber_map[comp_map[i]] = unique_count++;
	}
	for (int i = 0; i < comp_count; ++i) {
		comp_map[i] = renumber_map[comp_map[i]];
	}

	
	// remap
	for (int y = 0; y < dims.y; ++y) {
		for (int x = 0; x < dims.x; ++x) {
			comp_img(x,y) = comp_map[comp_img(x,y)];
		}
	}

	if (unique_count != 4) { // includes the implicit '0' component for background
		ParserError& e = err->errors.push();
		e.tok = t;
		e.msg.set(TempStr("Too %s parts in diagram - expected Left, Arrow, Right.", unique_count < 4 ? "few" : "many"));
		e.img = full_img;
		e.show_diagram = true;
		for (int y = 0; y < dims.y; ++y) {
			for (int x = 0; x < dims.x; ++x) {
				e.img_color[y][x] = COLOR_COMPONENT(comp_img(x,y));
			}
		}
		goto END;
	}

	// isolate lhs
	DiagramImg comp_working_img = comp_img;
	DiagramPiece lhs = parseDiagramPieceAndClearComponent(p, &comp_working_img);
	DiagramPiece arr = parseDiagramPieceAndClearComponent(p, &comp_working_img);
	DiagramPiece rhs = parseDiagramPieceAndClearComponent(p, &comp_working_img);

	// check that arrow is actually an arrow
	{	
		ParserError e;
		e.tok = t;
		e.img = full_img;
		e.show_diagram = true;
		ivec2 arrow_pos = ivec2(-1);
		bool non_arrow_characters = false;
		bool malformed_arrow = false;
		int char_count = 0;
		for (int y = arr.start.y; y <= arr.end.y; ++y) {
			for (int x = arr.start.x; x <= arr.end.x; ++x) {
				if (comp_img(x,y) == arr.comp_id) {
					char key = full_img(x,y);
					if (key != ' ') {
						e.img_color[y][x] = COLOR_ERROR; // by default, say this is an error
						if (key == '-' || key == '>') {
							if (key == '-') {
								if (arrow_pos == ivec2(-1)) { // check we haven't found something already
									if (x < (dims.x-1) && full_img(x+1,y) == '>') { // if '>' is next to it, we found the arrow
										arrow_pos = ivec2(x,y);
										e.img_color[y][x] = COLOR_TEXT;
									} else {
										malformed_arrow = true;
									}
								} else {
									malformed_arrow = true;
								}
							} else if (key == '>') {
								if ((arrow_pos != ivec2(-1)) && (x == (arrow_pos.x+1))) { // if '-' is not behind it, this is a stray
									e.img_color[y][x] = COLOR_TEXT;
								} else {
									malformed_arrow = true;
								}
							}
						} else {
							non_arrow_characters = true;
						}
					}
				}
			}
		}
		if (arrow_pos == ivec2(-1)) {
			e.msg.set("Arrow '->' not found.");
			err->errors.push(e);
			goto END;
		} else if (non_arrow_characters) {
			e.msg.set("Arrow part contains invalid characters.");
			err->errors.push(e);
			goto END;
		} else if (malformed_arrow) {
			e.msg.set("Arrow is malformed.");
			err->errors.push(e);
			goto END;
		}
	}

	// check that shapes match
	{
		ParserError e;
		e.tok = t;
		e.img = full_img;
		e.show_diagram = true;
		ivec2 pos = ivec2(0, 0);
		ivec2 pos_end = maxvec(lhs.end-lhs.start, rhs.end-rhs.start);
		bool shapes_dont_match = false;
		while (true) {
			ivec2 lhs_pos = lhs.start + pos;
			ivec2 rhs_pos = rhs.start + pos;
			bool lhs_empty = full_img.inImage(lhs_pos) ? full_img(lhs_pos) == ' ' : true;
			bool rhs_empty = full_img.inImage(rhs_pos) ? full_img(rhs_pos) == ' ' : true;
			char lhs_char = full_img.inImage(lhs_pos) ? full_img(lhs_pos) : 0;
			char rhs_char = full_img.inImage(rhs_pos) ? full_img(rhs_pos) : 0;
			if (lhs_empty ^ rhs_empty) { // different!
				e.img_color[lhs_pos.y][lhs_pos.x] = COLOR_ERROR;
				e.img_color[rhs_pos.y][rhs_pos.x] = COLOR_ERROR;
				shapes_dont_match = true;
			}

			if (pos.x == pos_end.x) {
				if (pos.y == pos_end.y) {
					break;
				} else {
					pos.x = 0;
					pos.y += 1;
				}
			} else {
				pos.x += 1;
			}
		}
		if (shapes_dont_match) {
			e.msg.set("Shape of left and right parts is not the same.");
			err->errors.push(e);
			goto END;
		}
	}

	ivec2 lhs_center_pos = ivec2(-1);
	// parse LHS
	{
		ParserError e;
		e.tok = t;
		e.img = full_img;
		e.show_diagram = true;
		bool center_non_unique = false;
		for (int y = lhs.start.y; y <= lhs.end.y; ++y) {
			for (int x = lhs.start.x; x <= lhs.end.x; ++x) {
				if (comp_img(x,y) == lhs.comp_id) {
					char key = full_img(x,y);
					if (key != ' ') {
						e.img_color[y][x] = COLOR_ERROR; // by default, say this is an error
						if (key == '@') {
							if (lhs_center_pos == ivec2(-1)) {
								lhs_center_pos = ivec2(x,y);
								e.img_color[y][x] = COLOR_TEXT;
							} else {
								center_non_unique = true;
							}
						}
					}
				}
			}
		}
		if (lhs_center_pos == ivec2(-1)) {
			e.msg.set("Site marker '@' not found on left part.");
			err->errors.push(e);
			goto END;
		} else if (center_non_unique) {
			e.msg.set("Multiple site markers '@' found on left part.");
			err->errors.push(e);
			goto END;
		}
	}

	// Begin semantic parsing
	// At this stage, the spatial syntax should all be valid
	// - center pos != -1 (@ was found)
	// - lhs and rhs shapes exist, and match in shape
	ivec2 rhs_center_pos = rhs.start + (lhs_center_pos - lhs.start);
	for (int i = 0; i < 41; ++i) {
		ivec2 lhs_pos = lhs_center_pos + getSiteCoord(i);
		n->diag.lhs[i] = full_img.inImage(lhs_pos) && comp_img(lhs_pos) == lhs.comp_id ? full_img(lhs_pos) : ' ';
		ivec2 rhs_pos = rhs_center_pos + getSiteCoord(i);
		n->diag.rhs[i] = full_img.inImage(rhs_pos) && comp_img(rhs_pos) == rhs.comp_id ? full_img(rhs_pos) : ' ';		
	}

END:
	n->full_img = full_img;
	n->comp_img = comp_img;
	return n;
}

Node* parseSectionHeader(Parser* par, Errors* err, Token tok)  {
	NodeType node_type = Node_unknown;
	const char* str = tok.str;
	const char* str_end = tok.str + tok.len;

	int header_depth = 0;
	while(str != str_end) {
		if (*str == '=') {
			header_depth += 1;
			str += 1;
		} else {
			break;
		}
	}
	assert(header_depth > 0); //lexer gave us an invalid header?
	skipWhitespace(&str, str_end);
	StringRange name;
	name.str = str;
	while (str != str_end && isAlpha(*str)) str += 1;
	name.len = str - name.str;
	for (int i = 0; i < ARRSIZE(section_strings); ++i)
		if (name.compareIgnoreCase(section_strings[i]))
			node_type = section_types[i];
	if (node_type == Node_unknown) {
		err->add(tok, TempStr("Section type '%.*s' not recognized.", name.len, name.str)); 
		return NULL;
	} else {
		Node* nod = makeNode(node_type, tok);
		nod->int_val = header_depth;
		if (node_type == Node_element) {
			skipWhitespace(&str, str_end);
			nod->str_val.str = str;
			skipAlphaNumeric(&str, str_end);
			nod->str_val.len = str - nod->str_val.str;
		}
		return nod;
	}
}

Node* parseSymmetriesMetadata(Parser* par, Errors* err, Token meta_tok) {
	Lexer lex = Lexer(meta_tok.str, meta_tok.str + meta_tok.len);
	lexToken(&lex, err); // eat 'symmetries'
	unsigned int symmetries = 0;
	bool parsing = true;
	while (parsing) {
		Token tok = lexToken(&lex, err);
		switch (tok.type) {
		case Token_end_of_stream: parsing = false; break;
		case Token_comma: continue;
		case Token_number: {
			int n = strtol(tok.str, 0, 0);
			if (n >= 0 && n <= 8) {
				symmetries |= (1 << n);
			} else {
				err->add(meta_tok, TempStr("Symmetry '%.*s' not recognized.", tok.len, tok.str));
				return makeNodeError(tok);
			}
		} break;
		case Token_identifier: {
			bool known = false;
			for (int i = 0; i < ARRSIZE(symmetry_strings); ++i) {
				if (StringRange(tok.str, tok.len).compareIgnoreCase(symmetry_strings[i])) {
					symmetries |= symmetry_bitmasks[i];
					known = true;
				}
			}
			if (!known) {
				err->add(meta_tok, TempStr("Symmetry set '%.*s' not recognized.", tok.len, tok.str));
				return makeNodeError(tok);
			}
		} break;

		default:
			err->add(meta_tok, TempStr("Symmetry metadata '%.*s' not recognized.", tok.len, tok.str));
			return makeNodeError(tok);
		}
	}
	return makeNode(Node_metadata_symmetries, meta_tok, symmetries);
}
Node* parseRadiusMetadata(Parser* par, Errors* err, Token meta_tok) {
	Lexer lex = Lexer(meta_tok.str, meta_tok.str + meta_tok.len);
	lexToken(&lex, err); // eat 'radius'
	Token tok = lexToken(&lex, err);
	if (tok.type == Token_number) {
		if (tok.int_lit >= 0 && tok.int_lit <= 4) {
			return makeNode(Node_metadata_radius, tok, tok.int_lit);
		} else {
			err->add(meta_tok, TempStr("Radius metadata '%.*s' must be between 0 and 4.", tok.len, tok.str));
			return makeNodeError(tok);
		}
	} else {
		err->add(meta_tok, TempStr("Radius metadata '%.*s' must be a number.", tok.len, tok.str));
		return makeNodeError(tok);
	}
}
Node* parseSymbolMetadata(Parser* par, Errors* err, Token meta_tok) {
	Lexer lex = Lexer(meta_tok.str, meta_tok.str + meta_tok.len);
	lexToken(&lex, err); // eat 'symbol'
	Token tok = lexToken(&lex, err);
	if (tok.len > 2) { 
		err->add(meta_tok, TempStr("Symbol metadata '%.*s' is longer than 2 characters.", tok.len, tok.str));
		return makeNodeError(tok);
	} else if (isLowercaseAlpha(tok.str[0])) {
		err->add(meta_tok, TempStr("Symbol metadata '%.*s' must start with a capital letter.", tok.len, tok.str));
		return makeNodeError(tok);
	} else {
		return makeNode(Node_metadata_symbol, tok, StringRange(tok.str, tok.len));
	}
}
Node* parseColorMetadata(Parser* par, Errors* err, Token meta_tok) {
	Lexer lex = Lexer(meta_tok.str, meta_tok.str + meta_tok.len);
	lexHackEnableInjectedMode(&lex); // #HACK normally '#' will be parsed as a comment, unless we're in injected mode which parses it as a #directive, so let's hijack that
	lexToken(&lex, err); // eat 'color'
	Token tok = lexToken(&lex, err);
	const char* dig = tok.str;
	const char* end = tok.str + tok.len;
	while (dig != end) {
		if (*dig == ' ') {
			break;
		}
		if (isHexadecimal(dig[0])) {
			dig += 1;
		} else {
			err->add(tok, TempStr("Color metadata '%.*s' contains non-hex digit '%c'.", tok.len, tok.str, dig[0]));
			return makeNodeError(tok);
		}
	}
	int dig_count = dig - tok.str;
	if (dig_count == 3 || dig_count == 6) {
		int digs[6];
		for (int c = 0; c < dig_count; ++c) {
			int i = dig_count == 6 ? c : c * 2;
			digs[i] = intDigitFromHex(tok.str[c]);
		}
		if (dig_count == 3) // fill in missing digits for 3 inputs
			for (int c = 0; c < 3; ++c)
				digs[c * 2 + 1] = digs[c * 2];
		unsigned int argb = 0xff000000;
		for (int i = 0; i < 6; ++i)
			argb |= digs[6 - 1 - i] << (i * 4);

		return makeNode(Node_metadata_color, tok, argb);
	} else {
		err->add(tok, TempStr("Color metadata '%.*s' should be 3 or 6 digits long.", tok.len, tok.str));
		return makeNodeError(tok);
	}
}
StringRange parseLineString(Parser* par, Errors* err, Token tok) {
	const char* end = tok.str + tok.len;
	skipAlpha(&tok.str, end); // eat 'author'
	skipWhitespace(&tok.str, end);
	const char* str = tok.str;
	skipUntilEndOfLine(&tok.str, end);
	while (tok.str != str) { // walk backwards eating needless whitespace
		if (!isWhitespace(tok.str[0])) {
			tok.str += 1; // keep the non-whitespace
			break;
		} else {
			tok.str -= 1;
		}
	}
	tok.len = tok.str - str;
	tok.str = str;
	return StringRange(tok.str, tok.len);
}
Node* parseAuthorMetadata(Parser* par, Errors* err, Token tok) {
	return makeNode(Node_metadata_author, tok, parseLineString(par, err, tok));
}
Node* parseLicenceMetadata(Parser* par, Errors* err, Token tok) {
	return makeNode(Node_metadata_licence, tok, parseLineString(par, err, tok));
}
typedef Node* (*MetadataParser)(Parser*, Errors*, Token);
static MetadataParser metadata_parsers[ARRSIZE(metadata_types)] = {
	parseSymmetriesMetadata,
	parseRadiusMetadata,
	parseSymbolMetadata,
	parseColorMetadata,
	parseAuthorMetadata,
	parseLicenceMetadata,
};
Node* parseMetadata(Parser* par, Errors* err, Token tok) {
	const char* str = tok.str;
	const char* str_end = tok.str + tok.len;
	StringRange name;
	name.str = str;
	while (str != str_end && isAlpha(*str)) str += 1;
	name.len = str - name.str;
	for (int i = 0; i < ARRSIZE(metadata_strings); ++i)
		if (name.compareIgnoreCase(metadata_strings[i]))
			return metadata_parsers[i](par, err, tok);
	err->add(tok, TempStr("Metadata type '%.*s' not recognized", name.len, name.str));
	return NULL;	
}

bool tokenEquals(Token t, StringRange str) {
	return StringRange(t.str, t.len) == str;
}

Node* parseGroups(Parser* par, Lexer* lex, Errors* err) {
	Token tok = lexToken(lex, err);
	if (tok.type == Token_end_of_stream) {
		return NULL;
	} else if (tok.type == Token_newline || tok.type == Token_splat_comment || tok.type == Token_skip) {
		return parseGroups(par, lex, err);
	} else if (tok.type == Token_semicolon) {
		return makeNode(Node_end_statement, tok);
	} else if (tok.type == Token_open_brace || tok.type == Token_open_paren) {
		NodeType type;
		switch (tok.type) {
		case Token_open_brace: type = Node_braces; break;
		case Token_open_paren: type = Node_parens; break;
		default: assert(false); break;
		}
		Node* n = makeNode(type, tok);
		while (true) {
			Node* k = parseGroups(par, lex, err);
			addKid(n, k);
			if (!k) {
				err->add(tok, TempStr("Unmatched %s.", type == Node_braces ? "brace" : "paren"));
				break;
			}
			if (k->type == (type == Node_braces ? Node_braces_end : Node_parens_end))
				break;
		}
		return n;
	} else if (tok.type == Token_close_brace) {
		return makeNode(Node_braces_end, tok);
	} else if (tok.type == Token_close_paren) {
		return makeNode(Node_parens_end, tok);
	} else if (tok.type == Token_spatial_form) {
		return parseSpatialForm(par, err, tok);
	} else if (tok.type == Token_section_header) {
		return parseSectionHeader(par, err, tok);
	} else if (tok.type == Token_metadata) {
		return parseMetadata(par, err, tok);
	} else if (tok.type == Token_identifier) {
		return makeNode(Node_identifier, tok);
	} else if (tok.type == Token_keyword) {
		return makeNode(Node_keyword, tok);
	} else {
		return makeNode(Node_unknown, tok);
	}
}

inline bool isSectionTerminator(Node* for_who, Node* what) {
	// element sections terminate only on other element sections
	// other header sections terminate on any other
	if (for_who->type == Node_element) {
		return what->type == Node_element; 
	} else {
		return isSectionHeaderNode(what);
	}
}
inline bool isSectionDepthValid(Node* who) {
	if (who->type == Node_element) {
		return who->int_val == 1;
	} else {
		return who->int_val == 2;
	}
}
void transformAST(Node* par, Node* who, Errors* err) {
	if (isSectionHeaderNode(who)) {
		if (!isSectionDepthValid(who)) {
			ParserError& e = err->errors.push();
			e.tok = who->tok;
			e.msg.set(TempStr("%.*s section is at invalid '=' depth", section_strings[who->type - Node_element].len, section_strings[who->type - Node_element].str));
			return;
		}
		// find the last node of this section by looking for the section node or end of tree
		Node* last_element_sib = who;
		while (last_element_sib) {
			if (last_element_sib->sib && isSectionTerminator(who, last_element_sib->sib)) {
				break;
			} else {
				last_element_sib = last_element_sib->sib;
			}
		}
		if (last_element_sib == who) { // if we have no siblings before next element, nothing to do
			assert(false);
		} else {
			addKid(who, who->sib);                 // convert our first sibling to a kid
			if (last_element_sib) {                // if there is any more elements after this one...
				who->sib = last_element_sib->sib;  // our next sibling becomes the node after our last
				last_element_sib->sib = NULL;      // sever link from our last node to it's old sibling
			} else {
				who->sib = NULL;                   // otherwise we no longer have siblings
			}
		}
	}
	if (who->kid) transformAST(who, who->kid, err);
	if (who->sib) transformAST(par, who->sib, err);
}



#if 0

void parseCStyleComment(Lexer* z) {
	while (true) {
		if (isEndOfStream(z)) {
			//#ERROR
			break;
		} else if (z->at[0] == '*' && z->at[1] == '/') {
			z->at += 2;
			break;
		} else {
			z->at += 1;
		}
	}
}
void parseCPPStyleComment(Lexer* z) {
	while (true) {
		if (isEndOfStream(z->at[0])) {
			//#ERROR
			break;
		} else if (isEndOfLine(z->at[0])) {
			z->at += 1; // skip the eol, double eols like \r\n will get skipped by whitespace
			break;
		} else {
			z->at += 1;
		}
	}
}
void eatAllWhitespaceAndComments(Lexer* z) {
	while (true) {
		if (isWhitespace(z->at[0])) {
			z->at += 1;
		} else if (z->at[0] == '/' && z->at[1] == '/') {
			z->at += 2;
			parseCPPStyleComment(z);
		} else if (z->at[0] == '/' && z->at[1] == '*') {
			z->at += 2;
			parseCStyleComment(z);
		} else {
			break;
		}
	}
}


#endif