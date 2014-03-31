#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include "xml.h"

static int isxmlpi = 0, tags = 0;

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen) {
	if(tags > 3) /* optimization: try to find processing instruction at start */
		exit(EXIT_FAILURE);
	isxmlpi = (tag[0] == '?' && (!strncasecmp(tag, "?xml", taglen))) ? 1 : 0;
	tags++;
}

static void
xmltagend(XMLParser *p, const char *tag, size_t taglen, int isshort) {
	isxmlpi = 0;
}

static void
xmlattr(XMLParser *p, const char *tag, size_t taglen, const char *name, size_t namelen, const char *value, size_t valuelen) {
	if(isxmlpi && (!strncasecmp(name, "encoding", namelen))) {
		for(; *value; value++)
			putc(tolower((int)*value), stdout); /* output lowercase */
		exit(EXIT_SUCCESS);
	}
}

int
main(int argc, char **argv) {
	XMLParser x;

	xmlparser_init(&x);
	x.xmltagstart = xmltagstart;
	x.xmltagend = xmltagend;
	x.xmlattr = xmlattr;

	xmlparser_parse(&x);

	return EXIT_FAILURE;
}
