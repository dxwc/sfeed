#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "util.h"

static int showsidebar = 1; /* show sidebar ? */

static struct feed *feeds = NULL; /* start of feeds linked-list. */
static char *line = NULL;

static void
cleanup(void) {
	free(line); /* free line */
	line = NULL;
	feedsfree(feeds); /* free feeds linked-list */
}

static void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed_html: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

int
main(void) {
	char *fields[FieldLast];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int islink, isnew;
	struct feed *f, *fcur = NULL;
	time_t parsedtime, comparetime;
	size_t size = 0;

	atexit(cleanup);
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	fputs(
		"<!DOCTYPE HTML>\n"
		"<html dir=\"ltr\" lang=\"en\">\n"
		"\t<head>\n"
		"\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
		"\t</head>\n"
		"\t<body class=\"noframe\">\n",
	stdout);

	if(!(fcur = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	feeds = fcur;

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		isnew = (parsedtime >= comparetime);
		islink = (fields[FieldLink][0] != '\0');
		/* first of feed section or new feed section. */
		/* TODO: allocate fcur before here, fcur can be NULL */
		if(!totalfeeds || (fcur && strcmp(fcur->name, fields[FieldFeedName]))) {
			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");
			/*f->next = NULL;*/
			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", stdout);
				fcur->next = f;
				fcur = f;
			} else {
				fcur = f;
				feeds = fcur; /* first item. */
				if(fields[FieldFeedName][0] == '\0' || !showsidebar) {
					/* set nosidebar class on div for styling */
					fputs("\t\t<div id=\"items\" class=\"nosidebar\">\n", stdout);
					showsidebar = 0;
				} else
					fputs("\t\t<div id=\"items\">\n", stdout);
			}

			/* TODO: memcpy and make fcur->name static? */
			if(!(fcur->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");


			/*
			fcur->totalnew = 0;
			fcur->total = 0;
			fcur->next = NULL;*/

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
				printlink(fields[FieldLink], fields[FieldBaseSiteUrl], stdout);
			else
				printlink(fields[FieldLink], fields[FieldFeedUrl], stdout);
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
		for(fcur = feeds; fcur; fcur = fcur->next) {
			if(!fcur->name || fcur->name[0] == '\0')
				continue;
			if(fcur->totalnew)
				fputs("<li class=\"n\"><a href=\"#", stdout);
			else
				fputs("<li><a href=\"#", stdout);
			printfeednameid(fcur->name, stdout);
			fputs("\">", stdout);
			if(fcur->totalnew > 0)
				fputs("<b><u>", stdout);
			fputs(fcur->name, stdout);
			fprintf(stdout, " (%lu)", fcur->totalnew);
			if(fcur->totalnew > 0)
				fputs("</u></b>", stdout);
			fputs("</a></li>\n", stdout);
		}
		fputs("\t\t</ul>\n\t</div>\n", stdout);
	}
	fputs(
		"\t</body>\n"
		"\t<title>Newsfeed (",
	stdout);
	fprintf(stdout, "%lu", totalnew);
	fputs(")</title>\n</html>", stdout);

	return EXIT_SUCCESS;
}
