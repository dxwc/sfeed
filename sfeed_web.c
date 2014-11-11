#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "util.h"
#include "xml.h"

static unsigned int isbase = 0, islink = 0, isfeedlink = 0, found = 0;
static char feedlink[4096] = "", basehref[4096] = "", feedtype[256] = "";

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
			fputs(feedtype, stdout);
			putchar(' ');
		}
		printlink(feedlink, basehref, stdout);
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
		} else if(!strncasecmp(name, "href", namelen))
			strlcpy(feedlink, value, sizeof(feedlink));
	}
}

int
main(int argc, char *argv[])
{
	XMLParser x;

	/* base href */
	if(argc > 1)
		strlcpy(basehref, argv[1], sizeof(basehref));

	xmlparser_init(&x, stdin);
	x.xmltagstart = xmltagstart;
	x.xmlattr = xmlattr;
	x.xmltagstartparsed = xmltagstartparsed;

	xmlparser_parse(&x);

	return found > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
