#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "util.h"
#include "xml.h"

static XMLParser parser; /* XML parser state */
static char url[2048], text[256], title[256];

static void
printsafe(const char *s)
{
	for (; *s; s++)
		if (!iscntrl((int)*s) && *s != '\'' && *s != '\\')
			putchar((int)*s);
}

static void
xml_handler_end_element(XMLParser *p, const char *tag, size_t taglen,
	int isshort)
{
	(void)p;
	(void)taglen;
	(void)isshort;

	if (strcasecmp(tag, "outline"))
		return;

	if (url[0]) {
		fputs("\tfeed '", stdout);
		if (title[0])
			printsafe(title);
		else if (text[0])
			printsafe(text);
		else
			fputs("unnamed", stdout);
		fputs("' '", stdout);
		printsafe(url);
		fputs("'\n", stdout);
	}
	url[0] = text[0] = title[0] = '\0';
}

static void
xml_handler_attr(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen, const char *value, size_t valuelen)
{
	(void)p;
	(void)taglen;
	(void)namelen;
	(void)valuelen;

	if (strcasecmp(tag, "outline"))
		return;

	if (!strcasecmp(name, "title"))
		strlcat(title, value, sizeof(title));
	else if (!strcasecmp(name, "text"))
		strlcat(text, value, sizeof(text));
	else if (!strcasecmp(name, "xmlurl"))
		strlcat(url, value, sizeof(url));
}

static void
xml_handler_attrentity(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen, const char *value, size_t valuelen)
{
	char buf[16];
	ssize_t len;

	if ((len = xml_entitytostr(value, buf, sizeof(buf))) < 0)
		return;
	if (len > 0)
		xml_handler_attr(p, tag, taglen, name, namelen, buf, len);
	else
		xml_handler_attr(p, tag, taglen, name, namelen, value, valuelen);
}

int
main(void)
{
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	parser.xmlattr = xml_handler_attr;
	parser.xmlattrentity = xml_handler_attrentity;
	parser.xmltagend = xml_handler_end_element;

	fputs(
	    "#sfeedpath=\"$HOME/.sfeed/feeds\"\n"
	    "\n"
	    "# list of feeds to fetch:\n"
	    "feeds() {\n"
	    "	# feed <name> <feedurl> [basesiteurl] [encoding]\n", stdout);
	parser.getnext = getchar;
	xml_parse(&parser);
	fputs("}\n", stdout);

	return 0;
}
