#include "data_fields.h"
#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl"
#include "core/string_range.h"
#include "core/log.h" // for tempstr

enum {
	BASIC_TYPE_Unsigned,
	BASIC_TYPE_Int,
	BASIC_TYPE_Unary,
	BASIC_TYPE_Bool,
	BASIC_TYPE_C2D,
	BASIC_TYPE_S2D,
	BASIC_TYPE_count,
};

static inline const char* BASIC_TYPE_NAME(int e) {
	switch (e) {
	case BASIC_TYPE_Unsigned: return "Unsigned";
	case BASIC_TYPE_Int: return "Int";
	case BASIC_TYPE_Unary: return "Unary";
	case BASIC_TYPE_Bool: return "Bool";
	case BASIC_TYPE_C2D: return "C2D";
	case BASIC_TYPE_S2D: return "S2D";
	default: return "Unknown";
	}
};

static const char* GENERIC_OP_NAMES[] = { "get", "set" };
static const char* INTEGER_OP_NAMES[] = { "add", "sub", "mul", "div" };
static const char* INTEGER_OPS[] = { "+", "-", "*", "/" };


void DataField::appendDataFunctions(String& text, StringRange class_name) {
	if (internal) return;
	int minof = 0;
	int maxof = 0;
	const char* suffix = "";
	if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int) {
		minof = type == BASIC_TYPE_Int ? minSignedVal(bitsize) : 0;
		maxof = type == BASIC_TYPE_Int ? maxSignedVal(bitsize) : maxUnsignedVal(bitsize);
		suffix = type == BASIC_TYPE_Unsigned ? "U" : "";
		text.append(TempStr("const %s %.*s_%.*s_minof = %d%s;\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str, minof, suffix));
		text.append(TempStr("const %s %.*s_%.*s_maxof = %d%s;\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str, maxof, suffix));
	}
	// get
	text.append(TempStr("%s %.*s_%.*s_get(SiteNum sn) {\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str));
	if (global_offset != NO_OFFSET) {
		text.append(TempStr("\tAtom A = ew(sn);\n"));
		//if (type == BASIC_TYPE_Bool)
		//	text.append(TempStr("\treturn bitfieldExtract(A[%d], %d, %d) != 0;\n", global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
		//else
		text.append(TempStr("\treturn %s(bitfieldExtract(%s(A[%d]), %d, %d));\n", BASIC_TYPE_NAME(name), BASIC_TYPE_NAME(type), global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
	} else {
		text.append(TempStr("return %s(0);\n", BASIC_TYPE_NAME(type)));
	}
	text.append("}\n");

	text.append(TempStr("%s %.*s_%.*s_get() {\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str));
	if (global_offset != NO_OFFSET) {
		text.append(TempStr("\tAtom A = ew(0);\n"));
		text.append(TempStr("\treturn %s(bitfieldExtract(%s(A[%d]), %d, %d));\n", BASIC_TYPE_NAME(name), BASIC_TYPE_NAME(type), global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
	} else {
		text.append(TempStr("return %s(0);\n", BASIC_TYPE_NAME(type)));
	}
	text.append("}\n");

	//set
	text.append(TempStr("void %.*s_%.*s_set(SiteNum sn, %s v) {\n", class_name.len, class_name.str, name.len, name.str, BASIC_TYPE_NAME(type)));
	if (global_offset != NO_OFFSET) {
		if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int)
			text.append(TempStr("\tv = clamp(v, %d%s, %d%s);\n", minof, suffix, maxof, suffix));
		text.append(TempStr("\tAtom A = ew(sn);\n"));
		text.append(TempStr("\tA[%d] = bitfieldInsert(A[%d], v, %d, %d);\n", global_offset / BITS_PER_COMPONENT, global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
		text.append(TempStr("\tew(sn, A);\n"));
	} else {
		text.append("\treturn;\n");
	}
	text.append("}\n");

	text.append(TempStr("void %.*s_%.*s_set(%s v) {\n", class_name.len, class_name.str, name.len, name.str, BASIC_TYPE_NAME(type)));
	if (global_offset != NO_OFFSET) {
		if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int)
			text.append(TempStr("\tv = clamp(v, %d%s, %d%s);\n", minof, suffix, maxof, suffix));
		text.append(TempStr("\tAtom A = ew(0);\n"));
		text.append(TempStr("\tA[%d] = bitfieldInsert(A[%d], v, %d, %d);\n", global_offset / BITS_PER_COMPONENT, global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
		text.append(TempStr("\tew(0, A);\n"));
	} else {
		text.append("\treturn;\n");
	}
	text.append("}\n");

	// ops
	if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int) {
		for (int o = 0; o < ARRSIZE(INTEGER_OPS); ++o) {
			text.append(TempStr("%s %.*s_%.*s_%s(SiteNum sn, int v) {\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str, INTEGER_OP_NAMES[o]));
			if (global_offset != NO_OFFSET) {
				text.append(TempStr("\tAtom A = ew(sn);\n"));
				text.append(TempStr("\tint a = int(bitfieldExtract(%s(A[%d]), %d, %d));\n", BASIC_TYPE_NAME(type), global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
				text.append(TempStr("\ta = clamp(a %s v, %d, %d) << %d;\n", INTEGER_OPS[o], minof, maxof, global_offset%BITS_PER_COMPONENT));
				text.append(TempStr("\tA[%d] = bitfieldInsert(A[%d], a, %d, %d);\n", global_offset / BITS_PER_COMPONENT, global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
				text.append(TempStr("\tew(sn, A);\n"));
				text.append(TempStr("\treturn %s(a);\n", BASIC_TYPE_NAME(type)));
			} else {
				text.append(TempStr("\treturn %s(%d);\n", BASIC_TYPE_NAME(type), 0));
			}
			text.append("}\n");

			text.append(TempStr("%s %.*s_%.*s_%s(int v) {\n", BASIC_TYPE_NAME(type), class_name.len, class_name.str, name.len, name.str, INTEGER_OP_NAMES[o]));
			if (global_offset != NO_OFFSET) {
				text.append(TempStr("\tAtom A = ew(0);\n"));
				text.append(TempStr("\tint a = int(bitfieldExtract(%s(A[%d]), %d, %d));\n", BASIC_TYPE_NAME(type), global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
				text.append(TempStr("\ta = clamp(a %s v, %d, %d) << %d;\n", INTEGER_OPS[o], minof, maxof, global_offset%BITS_PER_COMPONENT));
				text.append(TempStr("\tA[%d] = bitfieldInsert(A[%d], a, %d, %d);\n", global_offset / BITS_PER_COMPONENT, global_offset / BITS_PER_COMPONENT, global_offset%BITS_PER_COMPONENT, bitsize));
				text.append(TempStr("\tew(0, A);\n"));
				text.append(TempStr("\treturn %s(a);\n", BASIC_TYPE_NAME(type)));
			} else {
				text.append(TempStr("\treturn %s(%d);\n", BASIC_TYPE_NAME(type), 0));
			}
			text.append("}\n");
		}
	}

}
void DataField::appendLocalDataFunctionDefines(String& text, StringRange class_name, bool define) {
	if (internal) return;
	if (define) {
		text.append(TempStr("#define %.*s_minof %.*s_%.*s_minof\n", name.len, name.str, class_name.len, class_name.str, name.len, name.str));
		text.append(TempStr("#define %.*s_maxof %.*s_%.*s_maxof\n", name.len, name.str, class_name.len, class_name.str, name.len, name.str));
	} else {
		text.append(TempStr("#undef %.*s_minof\n", name.len, name.str));
		text.append(TempStr("#undef %.*s_maxof\n", name.len, name.str));
	}
	TempStr full_name = TempStr("%.*s_%.*s", class_name.len, class_name.str, name.len, name.str);
	TempStr local_name = TempStr("%.*s", name.len, name.str);
	if (define) {
		for (int i = 0; i < ARRSIZE(GENERIC_OP_NAMES); ++i)
			text.append(TempStr("#define %s_%s %s_%s\n", local_name.str, GENERIC_OP_NAMES[i], full_name.str, GENERIC_OP_NAMES[i]));
		if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int)
			for (int i = 0; i < ARRSIZE(INTEGER_OP_NAMES); ++i)
				text.append(TempStr("#define %s_%s %s_%s\n", local_name.str, INTEGER_OP_NAMES[i], full_name.str, INTEGER_OP_NAMES[i]));
	} else {
		for (int i = 0; i < ARRSIZE(GENERIC_OP_NAMES); ++i)
			text.append(TempStr("#undef %s_%s\n", local_name.str, GENERIC_OP_NAMES[i]));
		if (type == BASIC_TYPE_Unsigned || type == BASIC_TYPE_Int)
			for (int i = 0; i < ARRSIZE(INTEGER_OP_NAMES); ++i)
				text.append(TempStr("#undef %s_%s\n", local_name.str, INTEGER_OP_NAMES[i]));
	}
}
void DataField::appendTo(Atom A, String& text) const {
	text.append(TempStr("%s(%d) %.*s = ", BASIC_TYPE_NAME(type), bitsize, name.len, name.str));
	switch (type) {
	case BASIC_TYPE_Unsigned: text.append(TempStr("%u", (u32)unpackBits(A, global_offset, bitsize))); return;
	case BASIC_TYPE_Int:      text.append(TempStr("%d", unpackInt(A, global_offset, bitsize))); return;
	default: text.append("?"); return;
	}
}

void dataAddInternalAndPickOffsets(Bunch<DataField>& data) {
	const int UNUSED = -1;
	int bit_owners[ATOM_BITS];
	for (int i = 0; i < ATOM_BITS; ++i)
		bit_owners[i] = UNUSED;

	DataField& ecc = data.push();
	ecc.bitsize = ATOM_ECC_BITSIZE;
	ecc.global_offset = ATOM_ECC_GLOBAL_OFFSET;
	ecc.name = StringRange("ECC");
	ecc.type = BASIC_TYPE_Unsigned;
	ecc.internal = true;
	for (int i = ecc.global_offset; i < (ecc.global_offset+ecc.bitsize); ++i)
		bit_owners[i] = &ecc - data.ptr;
		
	DataField& atype = data.push();
	atype.bitsize = ATOM_TYPE_BITSIZE;
	atype.global_offset = ATOM_TYPE_GLOBAL_OFFSET;
	atype.name = StringRange("AtomType");
	atype.type = BASIC_TYPE_Unsigned;
	atype.internal = true;
	for (int i = atype.global_offset; i < (atype.global_offset+atype.bitsize); ++i)
		bit_owners[i] = &atype - data.ptr;

	for (int i = 0; i < data.count; ++i) {
		DataField& f = data[i];
		if (f.global_offset != NO_OFFSET)
			continue;
			
		log("Trying to allocate %.*s\n", f.name.len, f.name.str);
		int global_offset = 0;
		while (true) {
			if (global_offset >= ATOM_BITS) {
				log("Unable to allocate %.*s\n",  f.name.len, f.name.str);
				break;
			}
			if (bit_owners[global_offset] != UNUSED) {
				global_offset++;
				continue;
			}
			bool usable = true;
			int component = global_offset / BITS_PER_COMPONENT; // the vector component of this spot
			for (int b = global_offset; b < (global_offset + f.bitsize); ++b) {
				if (bit_owners[b] != UNUSED) { // someone else is here already
					usable = false;
					break;
				}
				if ((b / BITS_PER_COMPONENT) != component) { // we changed components
					usable = false;
					break;
				}
			}
			if (usable) {
				// set my bits
				for (int b = global_offset; b < (global_offset + f.bitsize); ++b)
					bit_owners[b] = i;
				f.global_offset = global_offset;
				log("Put %.*s at %d to %d\n", f.name.len, f.name.str, global_offset, global_offset + f.bitsize);
				break;
			} else {
				global_offset += 1;
			}
		}
	}
}