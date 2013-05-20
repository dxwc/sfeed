/* convert an opml file to sfeedrc file */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "xml.h"
#include "compat.h"

XMLParser parser; /* XML parser state */
char feedurl[2048], feedname[2048], basesiteurl[2048];

int
istag(const char *s1, const char *s2) {
	return !xstrcasecmp(s1, s2);
}

int
isattr(const char *s1, const char *s2) {
	return !xstrcasecmp(s1, s2);
}

void
xml_handler_start_element(XMLParser *p, const char *tag, size_t taglen) {
	if(istag(tag, "outline")) {
		feedurl[0] = '\0';
		feedname[0] = '\0';
		basesiteurl[0] = '\0';
	}
}

void
xml_handler_end_element(XMLParser *p, const char *tag, size_t taglen, int isshort) {
	if(istag(tag, "outline")) {
		printf("\tfeed \"%s\" \"%s\" \"%s\"\n", feedname[0] ? feedname : "unnamed",
		       feedurl[0] ? feedurl : "", basesiteurl[0] ? basesiteurl : "");
	}
}

void
xml_handler_attr(XMLParser *p, const char *tag, size_t taglen, const char *name, size_t namelen, const char *value, size_t valuelen) {
	if(istag(tag, "outline")) {
		if(isattr(name, "text") || isattr(name, "title"))
			strncpy(feedname, value, sizeof(feedname) - 1);
		else if(isattr(name, "htmlurl"))
			strncpy(basesiteurl, value, sizeof(basesiteurl) - 1);
		else if(isattr(name, "xmlurl"))
			strncpy(feedurl, value, sizeof(feedurl) - 1);
	}
}

int
main(void) {
	xmlparser_init(&parser);

	parser.xmltagstart = xml_handler_start_element;
	parser.xmltagend = xml_handler_end_element;
	parser.xmlattr = xml_handler_attr;
	parser.fp = stdin;

	fputs(
		"# paths\n"
		"# NOTE: make sure to uncomment all these if you change it.\n"
		"#sfeedpath=\"$HOME/.sfeed\"\n"
		"#sfeedfile=\"$sfeedpath/feeds\"\n"
		"#sfeedfilenew=\"$sfeedfile.new\"\n"
		"\n"
		"# list of feeds to fetch:\n"
		"feeds() {\n"
		"	# feed <name> <url> [encoding]\n", stdout);
	xmlparser_parse(&parser);
	fputs("}\n", stdout);
	return EXIT_SUCCESS;
}
