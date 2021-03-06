#!/bin/bash
function generate () {
	echo "/*"
	echo " *Auto-Generated by configure (Do not change)"
	echo " */"
	echo ""
	echo "#pragma once"
	echo "#if defined(CS_EXPERIMENTAL_XML) && defined(HAVE_LIBEXSLT_EXSLT_H)"
	echo "static struct fallback_stylesheet {"
	echo "	const char *name;"
	echo "	const char *xsl;"
	echo "} fallback_stylesheets[] = {"
	for stylesheet in *2cxml.xsl; do
		name=`basename "${stylesheet}" 2cxml.xsl`
		echo -n "	{\"${name}\", \""
		cat "${stylesheet}" |tr -d '\010\011\012\013'|sed 's/^ *//;s/ *$//;s/ \{1,\}/ /g;s/"/\\"/g'
		echo "\"},"
	done
	echo "};"
	echo "#endif"
}
if test -z $1; then
	generate >../../src/sccp_xml_embedded.h
else
	generate >$1
fi
