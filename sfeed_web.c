#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "util.h"
#include "xml.h"

/* string and size */
#define STRP(s) s,sizeof(s)-1

static XMLParser parser;
static unsigned int isbase, islink, isfeedlink, found;
static char abslink[4096], feedlink[4096], basehref[4096], feedtype[256];

static void
printfeedtype(const char *s, FILE *fp)
{
	for (; *s; s++) {
		if (!isspace((int)*s))
			fputc(*s, fp);
	}
}

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen)
{
	(void)p;

	isbase = islink = isfeedlink = 0;
	if (taglen != 4) /* optimization */
		return;

	if (!strcasecmp(tag, "base"))
		isbase = 1;
	else if (!strcasecmp(tag, "link"))
		islink = 1;
}

static void
xmltagstartparsed(XMLParser *p, const char *tag, size_t taglen, int isshort)
{
	(void)p;
	(void)tag;
	(void)taglen;
	(void)isshort;

	if (!isfeedlink)
		return;

	if (absuri(feedlink, basehref, abslink, sizeof(abslink)) != -1)
		fputs(abslink, stdout);
	fputc('\t', stdout);
	printfeedtype(feedtype, stdout);
	putchar('\n');
	found++;
}

static void
xmlattr(XMLParser *p, const char *tag, size_t taglen, const char *name,
        size_t namelen, const char *value, size_t valuelen)
{
	(void)p;
	(void)tag;
	(void)taglen;
	(void)valuelen;

	if (namelen != 4) /* optimization */
		return;
	if (isbase) {
		if (!strcasecmp(name, "href"))
			strlcpy(basehref, value, sizeof(basehref));
	} else if (islink) {
		if (!strcasecmp(name, "type")) {
			if (!strncasecmp(value, STRP("application/atom")) ||
			   !strncasecmp(value, STRP("application/xml")) ||
			   !strncasecmp(value, STRP("application/rss"))) {
				isfeedlink = 1;
				strlcpy(feedtype, value, sizeof(feedtype));
			}
		} else if (!strncasecmp(name, "href", namelen)) {
			strlcpy(feedlink, value, sizeof(feedlink));
		}
	}
}

int
main(int argc, char *argv[])
{
	if (argc > 1)
		strlcpy(basehref, argv[1], sizeof(basehref));

	parser.xmlattr = xmlattr;
	parser.xmltagstart = xmltagstart;
	parser.xmltagstartparsed = xmltagstartparsed;

	xml_parse_fd(&parser, 0);

	return found > 0 ? 0: 1;
}
