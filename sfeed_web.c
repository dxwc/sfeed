#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"
#include "xml.h"

static unsigned int isbase, islink, isfeedlink, found;
static char abslink[4096], feedlink[4096], basehref[4096], feedtype[256];

static void
printfeedtype(const char *s, FILE *fp)
{
	for(; *s; s++) {
		if(!isspace((int)*s))
			fputc(*s, fp);
	}
}

static void
xmltagstart(XMLParser *p, const char *tag, size_t taglen)
{
	(void)p;

	isbase = islink = isfeedlink = 0;
	if(taglen == 4) { /* optimization */
		if(!strncasecmp(tag, "base", taglen))
			isbase = 1;
		else if(!strncasecmp(tag, "link", taglen))
			islink = 1;
	}
}

static void
xmltagstartparsed(XMLParser *p, const char *tag, size_t taglen, int isshort)
{
	(void)p;
	(void)tag;
	(void)taglen;
	(void)isshort;

	if(isfeedlink) {
		if(*feedtype) {
			printfeedtype(feedtype, stdout);
			putchar(' ');
		}
		if(absuri(feedlink, basehref, abslink, sizeof(abslink)) != -1)
			fputs(abslink, stdout);
		putchar('\n');
		found++;
	}
}

static void
xmlattr(XMLParser *p, const char *tag, size_t taglen, const char *name,
        size_t namelen, const char *value, size_t valuelen)
{
	(void)p;
	(void)tag;
	(void)taglen;
	(void)valuelen;

	if(namelen != 4) /* optimization */
		return;
	if(isbase) {
		if(!strncasecmp(name, "href", namelen))
			strlcpy(basehref, value, sizeof(basehref));
	} else if(islink) {
		if(!strncasecmp(name, "type", namelen)) {
			if(!strncasecmp(value, "application/atom", strlen("application/atom")) ||
			   !strncasecmp(value, "application/xml", strlen("application/xml")) ||
			   !strncasecmp(value, "application/rss", strlen("application/rss"))) {
				isfeedlink = 1;
				strlcpy(feedtype, value, sizeof(feedtype));
			}
		} else if(!strncasecmp(name, "href", namelen)) {
			strlcpy(feedlink, value, sizeof(feedlink));
		}
	}
}

int
main(int argc, char *argv[])
{
	XMLParser parser;

	if(argc > 1)
		strlcpy(basehref, argv[1], sizeof(basehref));

	memset(&parser, 0, sizeof(parser));
	parser.xmltagstart = xmltagstart;
	parser.xmlattr = xmlattr;
	parser.xmltagstartparsed = xmltagstartparsed;

	xmlparser_parse_fd(&parser, 0);

	return found > 0 ? 0: 1;
}
