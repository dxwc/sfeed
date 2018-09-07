#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"
#include "xml.h"

static XMLParser parser;
static int isxmlpi, tags;

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen)
{
	/* optimization: try to find processing instruction at start */
	if (tags > 3)
		exit(1);
	isxmlpi = (!strncasecmp(tag, "?xml", taglen)) ? 1 : 0;
	tags++;
}

static void
xmltagend(XMLParser *p, const char *tag, size_t taglen, int isshort)
{
	isxmlpi = 0;
}

static void
xmlattr(XMLParser *p, const char *tag, size_t taglen, const char *name,
	size_t namelen, const char *value, size_t valuelen)
{
	if (isxmlpi && !strcasecmp(name, "encoding")) {
		if (*value) {
			/* output lowercase */
			for (; *value; value++)
				putchar(tolower((unsigned char)*value));
			putchar('\n');
		}
		exit(0);
	}
}

int
main(void)
{
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	parser.xmlattr = xmlattr;
	parser.xmltagend = xmltagend;
	parser.xmltagstart = xmltagstart;

	parser.getnext = getchar;
	xml_parse(&parser);

	return 1;
}
