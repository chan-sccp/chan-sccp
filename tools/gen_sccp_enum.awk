# gen_sccp_enum.awk
#
# creates "sccp_enum.c" and "sccp_enum.h" from "sccp_enum.in"
# invoked using -->  awk -v out_header_file="sccp_enum.h" -v out_source_file="sccp_enum.c" -f gen_sccp_enum.awk sccp_enum.in
#
# created in 2015 by Diederik de Groot
# tested with gawk, mawk, and nawk.
#

BEGIN {
        out_header_file = "sccp_enum.h"
        out_source_file = "sccp_enum.c"

        #
        # gen enum headerfile
        #
	print "/*" > out_header_file
	print " * Auto-Generated File, do not modify.  Changes will be destroyed." > out_header_file 
        print " * $Date: $" > out_header_file
        print " * $Revision: $" > out_header_file
	print " */" >out_header_file 
	print "#ifndef __SCCP_ENUM_GUARD_H" >out_header_file
	print "#define __SCCP_ENUM_GUARD_H" >out_header_file 
	print "typedef int (*sccp_enum_str2intval_t)(const char *lookup_str);" > out_header_file
	print "typedef char *(*sccp_enum_all_entries_t)(void);" > out_header_file
	
	#
        # gen enum sourcefile
        #
	print "/*" > out_source_file
	print " * Auto-Generated File, do not modify. Changes will be destroyed." > out_source_file
        print " * $Date: $" > out_source_file
        print " * $Revision: $" > out_source_file
	print " */" > out_source_file
	#print "#define ARRAY_LEN(a) (size_t) (sizeof(a) / sizeof(0[a]))" >out_source_file
	print "#include <config.h>" > out_source_file
	print "#include \"common.h\"" > out_source_file
	print "#include \"sccp_enum.h\"" > out_source_file
	print "#include \"sccp_utils.h\"" > out_source_file
	enum_name = ""
	Comment = ""
	comment = 0
        codeSkip = 1
        e = 0
        sparse = 0
}


# when namespace is encountered store it for later use
/^namespace [a-zA-Z]+/	{
	namespace = $2				# code namespace reconstruct
	next
}

# Look for start of enum / comment blocks, skip the rest
/^enum[ \t]*[a-zA-Z]/		{ codeSkip = 0; bitfield = 0; sparse=0; enum_name=$2; next }
/^\/\*/	 			{ Comment = $0; comment = 1; next}					# comment start
comment == 1 			{ Comment = Comment "\n" $0; if ($0 ~ /^ \*\//) {comment = 0} }		# comment continuation
codeSkip == 1			{ next }

/^#ifdef[ \t]*[a-zA-Z]/		{ Entry_ifdef[e++] = $2; next }
/^#endif/			{ Entry_ifdef[e] = ""; next }

# match enum entry line
/^[ \t]*[A-Z]/ {
	split($0, entries, ",")
	id = entries[1]
	bitval = ""
	gsub(/^[ \t]+/, "", id);		# trim left
	gsub(/[ \t]+$/, "", id);		# trim right

	val = entries[2]
	gsub(/[ \t=]+/, "", val);		# remove all space

	text = entries[3]
	gsub(/^[ \t"]+/, "", text);		# trim left
	gsub(/[ \t"]+$/, "", text);		# trim right

	# detect sparse / non-sparse enum
	if (match(val, /1<<[0-9]*/) != 0) {
		# print "bitfield"		# skip bitfield
		bitfield = 1
	} else if (e > 0 && val && (strtonum(prev_val) + 1 != strtonum(val))) {
		sparse = 1
	}
	
	#print id "_" val "_" text
	Entry_id[e] = id
	Entry_val[e] = val
	Entry_text[e] = text
	Entry_ifdef[e] = ""
	#string_size = string_size + length(text)
	e++
	prev_val = val;
	next
}

# end of definition -> generate code
/^}/ {		
        codeSkip = 1
        bitval = ""
        
        #
        # gen enum header
        #

        if (sparse == 1) {
		headerfooter = sprintf("====================================================================================== %30s === */\n", "sparse " namespace "_" enum_name);
	} else {
		headerfooter = sprintf("====================================================================================== %30s === */\n", namespace "_" enum_name);
	}
        print "\n/* = Begin =" headerfooter >out_header_file
        
        print "/*" >out_header_file
        if (sparse == 1) {
	        print " * \\brief sparse enum " namespace "_" enum_name >out_header_file
        } else {
        	print " * \\brief enum " namespace "_" enum_name >out_header_file
        }
        print " */" >out_header_file
        
        # typedef enum sccp_channelstate {
	print "typedef enum " namespace "_" enum_name " {" >out_header_file
	for ( i = 0; i < e; ++i) {
		if (Entry_ifdef[i] != "") {
			print "#ifdef " Entry_ifdef[i] > out_header_file
			ifdef = 1
		} else {
			if (Entry_val[i] != "") print "\t" Entry_id[i] "=" Entry_val[i] "," > out_header_file
			else print "\t" Entry_id[i] "," > out_header_file
		}
		if (ifdef && Entry_ifdef[i] == "") {
			print "#endif" > out_header_file
			ifdef = 0
		}
		if (bitfield) {
			bitval = Entry_val[i]
			gsub(/1<</, "", bitval);
		}
	}
	if (bitfield) {
		print "\t" toupper(namespace) "_" toupper(enum_name) "_SENTINEL = 1<<" bitval + 1 > out_header_file
	} else {
		print "\t" toupper(namespace) "_" toupper(enum_name) "_SENTINEL" > out_header_file
	}
	print "} " namespace "_" enum_name "_t;" >out_header_file

	print "int " namespace "_" enum_name "_exists(int " namespace "_" enum_name "_int_value);" > out_header_file
	if (bitfield == 0) {
		print "const char * " namespace "_" enum_name "2str(" namespace "_" enum_name "_t enum_value);" > out_header_file
	} else {
		print "const char * " namespace "_" enum_name "2str(int " namespace "_" enum_name "_int_value);" > out_header_file
	}
	print namespace "_" enum_name "_t " namespace "_" enum_name "_str2val(const char *lookup_str);" > out_header_file
	print "int " namespace "_" enum_name "_str2intval(const char *lookup_str);" > out_header_file
	print "char *" namespace "_" enum_name "_all_entries(void);" > out_header_file

        print "/* = End ===" headerfooter >out_header_file

	#
	# gen enum source
	#

        print "\n/* = Begin =" headerfooter >out_source_file
	# print Comment if available
	if (Comment) {
		print Comment > out_source_file
	} else {
		print "\n/*" >out_source_file
		if (sparse == 1) {
			print " * \\brief sparse enum " namespace "_" enum_name >out_source_file
		} else {
			print " * \\brief enum " namespace "_" enum_name >out_source_file
		}
		print " */" >out_source_file
	}

        # static const char *sccp_channelstate_map[] = {
        if (sparse == 0) {
		print "static const char *" namespace "_" enum_name "_map[] = {" > out_source_file
		for ( i = 0; i < e; ++i) {
			if (Entry_ifdef[i] != "") {
				print "#ifdef " Entry_ifdef[i] > out_source_file
				ifdef = 1
			} else {
				if (bitfield == 0) {
					print "\t[" Entry_id[i] "] = \"" Entry_text[i] "\"," > out_source_file
				} else {
					print "\t\"" Entry_text[i] "\"," > out_source_file
				}
			}
			if (ifdef && Entry_ifdef[i] == "") {
				print "#endif" > out_source_file
				ifdef = 0
			}
		}
		if (bitfield == 0) {
			print "\t[" toupper(namespace) "_" toupper(enum_name) "_SENTINEL] = \"LOOKUPERROR\"" > out_source_file
		} else {
			print "\t\"LOOKUPERROR\"" > out_source_file
		}
		print "};\n" > out_source_file
        } else {
		printf "static const char *" namespace "_" enum_name "_map[] = {" > out_source_file
		for ( i = 0; i < e; ++i) {
			if (Entry_ifdef[i] != "") {
				print "#ifdef " Entry_ifdef[i] > out_source_file
				ifdef = 1
			} else {
				print"\"" Entry_text[i] "\"," > out_source_file
			}
			if (ifdef && Entry_ifdef[i] == "") {
				print "#endif" > out_source_file
				ifdef = 0
			}
		}
		print "};\n" > out_source_file
        }


	# int sccp_does_channelstate_exist[int int_enum_value) {
	print "int " namespace "_" enum_name "_exists(int " namespace "_" enum_name "_int_value) {" > out_source_file
	if (sparse == 0) {
		if (bitfield == 0) {
			print "\tif ((" Entry_id[1] " <=" namespace "_" enum_name "_int_value) && (" namespace "_" enum_name "_int_value < " toupper(namespace) "_" toupper(enum_name) "_SENTINEL )) {" >out_source_file
			print "\t\treturn 1;" > out_source_file
			print "\t}" > out_source_file
			print "\treturn 0;" > out_source_file
		} else {
			print "\tint res = 0, i;" > out_source_file
			print "\tfor (i = 0; i < " toupper(namespace) "_" toupper(enum_name) "_SENTINEL; i++) {" >out_source_file
			print "\t\tif ((" namespace "_" enum_name "_int_value & 1 << i) == 1 << i) {" > out_source_file
			print "\t\t\tres |= 1;" > out_source_file
			print "\t\t}" > out_source_file
			print "\t}" > out_source_file
			print "\treturn res;" > out_source_file
		}
	} else {
		printf "\tstatic const int " namespace "_" enum_name "s[] = {" > out_source_file
		for ( i = 0; i < e; ++i) {
			if (Entry_ifdef[i - 1] == "") {
				printf "" Entry_id[i] "," > out_source_file
			}
		}
		print "};" >out_source_file
		print "\tuint32_t idx;" >out_source_file
		print "\tfor (idx=0; idx < ARRAY_LEN(" namespace "_" enum_name "s); idx++) {" > out_source_file 
		print "\t\tif ("namespace "_" enum_name "s[idx]==" namespace "_" enum_name "_int_value) {" > out_source_file
		print "\t\t\treturn 1;" > out_source_file
		print "\t\t}"> out_source_file
		print "\t}"> out_source_file
		print "\treturn 0;" > out_source_file
	}
	print "}\n" > out_source_file

	# const char * sccp_channelstate2str(sccp_channelstate_t enum_value) {
	if (sparse == 0) {
		if (bitfield == 0) {
			print "const char * " namespace "_" enum_name "2str(" namespace "_" enum_name "_t enum_value) {" > out_source_file
			print "\tif ((" Entry_id[0] " <= enum_value) && (enum_value <= " toupper(namespace) "_" toupper(enum_name) "_SENTINEL)) {" >out_source_file
			print "\t\treturn " namespace "_" enum_name "_map[enum_value];" >out_source_file
			print "\t}" >out_source_file
			print "\tpbx_log(LOG_ERROR, \"SCCP: Error during lookup of '%d' in " namespace "_" enum_name "2str\\n\", enum_value);" > out_source_file
			print "\treturn \"SCCP: OutOfBounds Error during lookup of " namespace "_" enum_name "2str\\n\";" > out_source_file
		} else {
			for ( i = 0; i < e; i++) {
				totlen += length(Entry_text[e]) + 1
			}
			print "const char * " namespace "_" enum_name "2str(int " namespace "_" enum_name "_int_value) {" > out_source_file
			print "\tstatic char res[" totlen "] = \"\";" >out_source_file
			print "\tuint32_t i;" >out_source_file
			print "\tint pos = 0;" >out_source_file
			print "\tfor (i = 0; i < ARRAY_LEN(" namespace "_" enum_name "_map) - 1; i++) {" >out_source_file
			print "\t\tif (("namespace "_" enum_name "_int_value & 1 << i) == 1 << i) {" >out_source_file
			print "\t\t\tpos += snprintf(res + pos, " totlen ", \"%s%s\", pos ? \",\" : \"\", " namespace "_" enum_name "_map[i]);" >out_source_file
			print "\t\t}" >out_source_file
			print "\t}" >out_source_file
			print "\tif (!strlen(res)) {" >out_source_file
			print "\t\tpbx_log(LOG_ERROR, \"SCCP: Error during lookup of '%d' in " namespace "_" enum_name "2str\\n\", " namespace "_" enum_name "_int_value);" > out_source_file
			print "\t\treturn \"SCCP: OutOfBounds Error during lookup of sparse " namespace "_" enum_name "2str\\n\";" > out_source_file
			print "\t}" >out_source_file
			print "\treturn res;" >out_source_file
		}
	} else {
		print "const char * " namespace "_" enum_name "2str(" namespace "_" enum_name "_t enum_value) {" > out_source_file
		print "\tswitch(enum_value) {" > out_source_file
		for ( i = 0; i < e; ++i) {
			print "\t\tcase " Entry_id[i] ": return " namespace "_" enum_name "_map[" i "];" > out_source_file
		}
		print "\t\tdefault:" > out_source_file
		print "\t\t\tpbx_log(LOG_ERROR, \"SCCP: Error during lookup of '%d' in " namespace "_" enum_name "2str\\n\", enum_value);" > out_source_file
		print "\t\t\treturn \"SCCP: OutOfBounds Error during lookup of sparse " namespace "_" enum_name "2str\\n\";" > out_source_file
		print "\t}" >out_source_file
	}
	print "}\n" > out_source_file
	
	# sccp_channelstate_t sccp_channelstate_str2val(const char *lookup_str) {
	print namespace "_" enum_name "_t " namespace "_" enum_name "_str2val(const char *lookup_str) {" > out_source_file
 	if (sparse == 0) {
		print "\tuint32_t idx;" > out_source_file
		print "\tfor (idx = 0; idx < ARRAY_LEN(" namespace "_" enum_name "_map); idx++) {" > out_source_file
		print "\t\tif (sccp_strcaseequals(" namespace "_" enum_name "_map[idx], lookup_str)) {" > out_source_file
		if (bitfield == 0) {
			print "\t\t\treturn idx;" > out_source_file
		} else {
			print "\t\t\treturn 1 << idx;" > out_source_file
		}
		print "\t\t}" > out_source_file
		print "\t}" > out_source_file
	} else {
		for ( i = 0; i < e; ++i) {
			if (i == 0) {
				print "\tif        (sccp_strcaseequals(" namespace "_" enum_name "_map[" i "], lookup_str)) {" > out_source_file
			} else {
				print "\t} else if (sccp_strcaseequals(" namespace "_" enum_name "_map[" i "], lookup_str)) {" > out_source_file
			}
			print "\t\treturn " Entry_id[i] ";" > out_source_file
		}
		print "\t}" > out_source_file
	}
	print "\tpbx_log(LOG_ERROR, \"SCCP: LOOKUP ERROR, " namespace "_" enum_name "_str2val(%s) not found\\n\", lookup_str);" > out_source_file
	print "\treturn "toupper(namespace) "_" toupper(enum_name) "_SENTINEL;" > out_source_file
	print "}\n" > out_source_file

	# sccp_channelstate_t sccp_channelstate_str2intval(const char *lookup_str) {
	print "int " namespace "_" enum_name "_str2intval(const char *lookup_str) {" > out_source_file
	print "\tint res = "namespace "_" enum_name "_str2val(lookup_str);" > out_source_file
	print "\treturn (int)res != " toupper(namespace) "_" toupper(enum_name) "_SENTINEL ? res : -1;"> out_source_file
	print "}\n" > out_source_file

	# const char *sccp_channelstate_all_entries(char *buf, size_t buf_len, const char *separator) {
	print "char *" namespace "_" enum_name "_all_entries(void) {" > out_source_file
	long_str = ""
	for ( i = 0; i < e; ++i) {
		if (i < e-1) {
			long_str = long_str Entry_text[i] ","
		} else {
			long_str = long_str Entry_text[i]
		}
		Entry_bitval[e]=""
 	}
 	#gsub(/,$/,"",long_str
	print "\tstatic char res[] = \"" long_str "\";" > out_source_file
	print "\treturn res;" > out_source_file
	print "}" > out_source_file
	
        print "/* = End ===" headerfooter >out_source_file

        # reset values
        Comment = ""
        e = 0
        bitfield = 0
        #string_size = 0
	next
}

END {
	# add guard
	print "#endif /* __SCCP_ENUM_GUARD_H */" > out_header_file
}
