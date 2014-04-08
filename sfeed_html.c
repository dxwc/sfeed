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
	struct feed *f, *feedcurrent = NULL;
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

	if(!(feedcurrent = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	feeds = feedcurrent;

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		isnew = (parsedtime >= comparetime);
		islink = (fields[FieldLink][0] != '\0');
		/* first of feed section or new feed section. */
		/* TODO: allocate feedcurrent before here, feedcurrent can be NULL */
		if(!totalfeeds || (feedcurrent && strcmp(feedcurrent->name, fields[FieldFeedName]))) {
			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");
			/*f->next = NULL;*/
			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", stdout);
				feedcurrent->next = f;
				feedcurrent = f;
			} else {
				feedcurrent = f;
				feeds = feedcurrent; /* first item. */
				if(fields[FieldFeedName][0] == '\0' || !showsidebar) {
					/* set nosidebar class on div for styling */
					fputs("\t\t<div id=\"items\" class=\"nosidebar\">\n", stdout);
					showsidebar = 0;
				} else
					fputs("\t\t<div id=\"items\">\n", stdout);
			}

			/* TODO: memcpy and make feedcurrent->name static? */
			if(!(feedcurrent->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");


			/*
			feedcurrent->totalnew = 0;
			feedcurrent->total = 0;
			feedcurrent->next = NULL;*/

			if(fields[FieldFeedName][0] != '\0') {
				fputs("<h2 id=\"", stdout);
				printfeednameid(feedcurrent->name, stdout);
				fputs("\"><a href=\"#", stdout);
				printfeednameid(feedcurrent->name, stdout);
				fputs("\">", stdout);
				fputs(feedcurrent->name, stdout);
				fputs("</a></h2>\n", stdout);
			}
			fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", stdout);
			totalfeeds++;
		}
		totalnew += isnew;
		feedcurrent->totalnew += isnew;
		feedcurrent->total++;
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
		for(feedcurrent = feeds; feedcurrent; feedcurrent = feedcurrent->next) {
			if(!feedcurrent->name || feedcurrent->name[0] == '\0')
				continue;
			if(feedcurrent->totalnew)
				fputs("<li class=\"n\"><a href=\"#", stdout);
			else
				fputs("<li><a href=\"#", stdout);
			printfeednameid(feedcurrent->name, stdout);
			fputs("\">", stdout);
			if(feedcurrent->totalnew > 0)
				fputs("<b><u>", stdout);
			fputs(feedcurrent->name, stdout);
			fprintf(stdout, " (%lu)", feedcurrent->totalnew);
			if(feedcurrent->totalnew > 0)
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
