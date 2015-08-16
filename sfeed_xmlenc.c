#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "xml.h"

static XMLParser parser;
static int isxmlpi, tags;

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen)
{
	(void)p;

	/* optimization: try to find processing instruction at start */
	if (tags > 3)
		exit(1);
	isxmlpi = (!strncasecmp(tag, "?xml", taglen)) ? 1 : 0;
	tags++;
}

static void
xmltagend(XMLParser *p, const char *tag, size_t taglen, int isshort)
{
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

	if (isxmlpi && (!strcasecmp(name, "encoding"))) {
		if (*value) {
			/* output lowercase */
			for (; *value; value++)
				putc(tolower((int)*value), stdout);
			putchar('\n');
		}
		exit(0);
	}
}

int
main(void)
{
	parser.xmlattr = xmlattr;
	parser.xmltagend = xmltagend;
	parser.xmltagstart = xmltagstart;

	xml_parse_fd(&parser, 0);

	return 1;
}
