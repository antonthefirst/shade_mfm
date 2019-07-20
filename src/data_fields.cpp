#include "data_fields.h"
#include "shaders/cpu_gpu_shared.inl"
#include "shaders/defines.inl"
#include "core/string_range.h"
#include "core/log.h" // for tempstr

static const char* GENERIC_OP_NAMES[] = { "get", "set" };
static const char* INTEGER_OP_NAMES[] = { "add", "sub", "mul", "div" };
static const char* INTEGER_OPS[] = { "+", "-", "*", "/" };

void DataField::appendDataFunctions(String& text, StringRange class_name) {
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