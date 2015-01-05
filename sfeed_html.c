#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "util.h"

static int showsidebar = 1; /* show sidebar ? */
static SLIST_HEAD(feedshead, feed) fhead = SLIST_HEAD_INITIALIZER(fhead);
static char *line = NULL;

int
main(void)
{
	char *fields[FieldLast];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int islink, isnew;
	struct feed *f, *fcur = NULL;
	time_t parsedtime, comparetime;
	size_t size = 0;
	int r;

	/* 1 day old is old news */
	comparetime = time(NULL) - 86400;

	fputs("<!DOCTYPE HTML>\n"
	      "<html>\n"
	      "\t<head>\n"
	      "\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
	      "\t</head>\n"
	      "\t<body class=\"noframe\">\n", stdout);

	if(!(fcur = calloc(1, sizeof(struct feed))))
		err(1, "calloc");
	SLIST_INSERT_HEAD(&fhead, fcur, entry);

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		islink = (fields[FieldLink][0] != '\0') ? 1 : 0;
		/* first of feed section or new feed section (differs from
		 * previous one). */
		if(!totalfeeds || strcmp(fcur->name, fields[FieldFeedName])) {
			if(!(f = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			if(!(f->name = strdup(fields[FieldFeedName])))
				err(1, "strdup");

			SLIST_INSERT_AFTER(fcur, f, entry);
			fcur = f;

			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", stdout);
			} else {
				if(fields[FieldFeedName][0] == '\0' || !showsidebar) {
					/* set nosidebar class on div for styling */
					fputs("\t\t<div id=\"items\" class=\"nosidebar\">\n", stdout);
					showsidebar = 0;
				} else {
					fputs("\t\t<div id=\"items\">\n", stdout);
				}
			}
			if(fields[FieldFeedName][0] != '\0') {
				fputs("<h2 id=\"", stdout);
				printfeednameid(fcur->name, stdout);
				fputs("\"><a href=\"#", stdout);
				printfeednameid(fcur->name, stdout);
				fputs("\">", stdout);
				fputs(fcur->name, stdout);
				fputs("</a></h2>\n", stdout);
			}
			fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", stdout);
			totalfeeds++;
		}
		totalnew += isnew;
		fcur->totalnew += isnew;
		fcur->total++;

		if(isnew)
			fputs("<tr class=\"n\">", stdout);
		else
			fputs("<tr>", stdout);
		fputs("<td nowrap valign=\"top\">", stdout);
		fputs(fields[FieldTimeFormatted], stdout);
		fputs("</td><td nowrap valign=\"top\">", stdout);
		if(isnew)
			fputs("<b><u>", stdout);
		if(islink) {
			fputs("<a href=\"", stdout);
			if(fields[FieldBaseSiteUrl][0] != '\0')
				printlink(fields[FieldLink],
				          fields[FieldBaseSiteUrl], stdout);
			else
				printlink(fields[FieldLink],
				          fields[FieldFeedUrl], stdout);
			fputs("\">", stdout);
		}
		printhtmlencoded(fields[FieldTitle], stdout);
		if(islink)
			fputs("</a>", stdout);
		if(isnew)
			fputs("</u></b>", stdout);
		fputs("</td></tr>\n", stdout);
	}
	if(totalfeeds)
		fputs("</table>\n\t\t</div>\n", stdout); /* div items */
	if(showsidebar) {
		fputs("\t<div id=\"sidebar\">\n\t\t<ul>\n", stdout);

		SLIST_FOREACH(f, &fhead, entry) {
			if(!f->name || f->name[0] == '\0')
				continue;
			if(f->totalnew > 0)
				fputs("<li class=\"n\"><a href=\"#", stdout);
			else
				fputs("<li><a href=\"#", stdout);
			printfeednameid(f->name, stdout);
			fputs("\">", stdout);
			if(f->totalnew > 0)
				fputs("<b><u>", stdout);
			fprintf(stdout, "%s (%lu)", f->name, f->totalnew);
			if(f->totalnew > 0)
				fputs("</u></b>", stdout);
			fputs("</a></li>\n", stdout);
		}
		fputs("\t\t</ul>\n\t</div>\n", stdout);
	}
	fprintf(stdout, "\t</body>\n\t<title>Newsfeed (%lu)</title>\n</html>",
	        totalnew);

	return 0;
}
