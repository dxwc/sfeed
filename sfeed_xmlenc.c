#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>

#include "xml.h"

static int isxmlpi = 0, tags = 0;

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen) {
	(void)p;

	/* optimization: try to find processing instruction at start */
	if(tags > 3)
		exit(EXIT_FAILURE);
	isxmlpi = (!strncasecmp(tag, "?xml", taglen)) ? 1 : 0;
	tags++;
}

static void
xmltagend(XMLParser *p, const char *tag, size_t taglen, int isshort) {
	(void)p;
	(void)tag;
	(void)taglen;
	(void)isshort;

	isxmlpi = 0;
}

static void
xmlattr(XMLParser *p, const char *tag, size_t taglen, const char *name,
	size_t namelen, const char *value, size_t valuelen)
{
	(void)p;
	(void)tag;
	(void)taglen;
	(void)valuelen;

	if(isxmlpi && (!strncasecmp(name, "encoding", namelen))) {
		if(*value) {
			/* output lowercase */
			for(; *value; value++)
				putc(tolower((int)*value), stdout);
			putchar('\n');
		}
		exit(EXIT_SUCCESS);
	}
}

int
main(void) {
	XMLParser x;

	xmlparser_init(&x, stdin);
	x.xmltagstart = xmltagstart;
	x.xmltagend = xmltagend;
	x.xmlattr = xmlattr;

	xmlparser_parse(&x);

	return EXIT_FAILURE;
}
