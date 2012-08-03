#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include "common.c"

/* Feed info. */
struct feed {
	char *name; /* feed name */
	unsigned long new; /* amount of new items per feed */
	unsigned long total; /* total items */
	struct feed *next; /* linked list */
};

static int showsidebar = 1; /* show sidebar ? */

void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed_html: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

struct feed *
feednew(void) {
	struct feed *f;
	if(!(f = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	return f;
}

void
feedsfree(struct feed *f) {
	struct feed *next;
	while(f) {
		next = f->next;
		free(f->name);
		free(f);
		f = next;
	}
}

/* print feed name for id; spaces and tabs in string as "-" (spaces in anchors are not valid). */
void
printfeednameid(const char *s) {
	for(; *s; s++)
		putchar(isspace(*s) ? '-' : *s);
}

void
printhtmlencoded(const char *s) {
	for(; *s; s++) {
		switch(*s) {
		case '<': fputs("&lt;", stdout); break;
		case '>': fputs("&gt;", stdout); break;
		case '&': fputs("&amp;", stdout); break;
		default:
			putchar(*s);
		}
	}
}

int
main(void) {
	char *line = NULL, *fields[FieldLast];
	unsigned long totalfeeds = 0, totalnew = 0;
	unsigned int islink, isnew;
	struct feed *feedcurrent = NULL, *feeds = NULL; /* start of feeds linked-list. */
	time_t parsedtime, comparetime;
	size_t size = 0;

	tzset();
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	fputs(
		"<!DOCTYPE HTML>\n"
		"<html dir=\"ltr\" lang=\"en\">\n"
		"	<head>\n"
		"		<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		"		<style type=\"text/css\">\n"
		"			body {\n"
		"				font-family: monospace;\n"
		"				font-size: 9pt;\n"
		"				color: #333;\n"
		"				background-color: #fff;\n"
		"				overflow: hidden;\n"
		"			}\n"
		"			#feedcontent td {\n"
		"				white-space: nowrap;\n"
		"			}\n"
		"			#feedcontent h2 {\n"
		"				font-size: 14pt;\n"
		"			}\n"
		"			#feedcontent a {\n"
		"				display: block;\n"
		"			}\n"
		"			#feedcontent ul, #feedcontent li {\n"
		"				list-style: none;\n"
		"				padding: 0;\n"
		"				margin: 0;\n"
		"			}\n"
		"			#feedcontent h2 a, #feedcontent ul li a {\n"
		"				color: inherit;\n"
		"			}\n"
		"			#feedcontent ul li a {\n"
		"				padding: 5px 3px 5px 10px;\n"
		"			}\n"
		"			#feedcontent div#sidebar {\n"
		"				background-color: inherit;\n"
		"				position: fixed;\n"
		"				top: 0;\n"
		"				left: 0;\n"
		"				width: 175px;\n"
		"				height: 100%;\n"
		"				overflow: hidden;\n"
		"				overflow-y: auto;\n"
		"				z-index: 999;\n"
		"			}\n"
		"			#feedcontent div#items {\n"
		"				left: 175px;\n"
		"			}\n"
		"			#feedcontent div#items-nosidebar {\n"
		"				left: 0px;\n"
		"			}\n"
		"			#feedcontent div#items-nosidebar,\n"
		"			#feedcontent div#items {\n"
		"				position: absolute;\n"
		"				height: 100%;\n"
		"				top: 0;\n"
		"				right: 0;\n"
		"				overflow: auto;\n"
		"				padding: 0 15px;\n"
		"			}\n"
		"		</style>\n"
		"	</head>\n"
		"	<body>\n"
		"		<div id=\"feedcontent\">\n",
	stdout);

	while(parseline(&line, &size, fields, FieldLast, stdin, FieldSeparator) > 0) {
		/* first of feed section or new feed section. */
		if(!totalfeeds || strcmp(feedcurrent->name, fields[FieldFeedName])) {
			if(totalfeeds) { /* end previous one. */
				fputs("</table>\n", stdout);
				feedcurrent->next = feednew();
				feedcurrent = feedcurrent->next;
			} else {
				feedcurrent = feednew();
				feeds = feedcurrent; /* first item. */
				fputs("\t\t<div id=\"items", stdout);
				if(fields[FieldFeedName][0] == '\0') {
					fputs("-nosidebar", stdout); /* set other id on div if no sidebar for styling */
					showsidebar = 0;
				}
				fputs("\">\n", stdout);
			}
			if(!(feedcurrent->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");
			if(fields[FieldFeedName][0] != '\0') {
				fputs("<h2 id=\"", stdout);
				printfeednameid(feedcurrent->name);
				fputs("\"><a href=\"#", stdout);
				printfeednameid(feedcurrent->name);
				fputs("\">", stdout);
				fputs(feedcurrent->name, stdout);
				fputs("</a></h2>\n", stdout);
			}
			fputs("<table>", stdout);
			totalfeeds++;
		}
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		isnew = (parsedtime >= comparetime);
		islink = (strlen(fields[FieldLink]) > 0);
		totalnew += isnew;
		feedcurrent->new += isnew;
		feedcurrent->total++;

		fputs("<tr><td>", stdout);
		printtime(parsedtime);
		fputs("</td><td>", stdout);
		if(isnew)
			fputs("<b><u>", stdout);
		if(islink) {
			fputs("<a href=\"", stdout);
			if(fields[FieldBaseSiteUrl][0] != '\0')
				printlink(fields[FieldLink], fields[FieldBaseSiteUrl]);
			else
				printlink(fields[FieldLink], fields[FieldFeedUrl]);
			fputs("\">", stdout);
		}
		printhtmlencoded(fields[FieldTitle]);
		if(islink)
			fputs("</a>", stdout);
		if(isnew)
			fputs("</u></b>", stdout);
		fputs("</td></tr>\n", stdout);
	}
	if(totalfeeds) {
		fputs("</table>\n", stdout);
		fputs("\t\t</div>\n", stdout); /* div items */
	}
	if(showsidebar) {
		fputs("\t\t<div id=\"sidebar\">\n\t\t\t<ul>\n", stdout);
		for(feedcurrent = feeds; feedcurrent; feedcurrent = feedcurrent->next) {
			if(!feedcurrent->name || feedcurrent->name[0] == '\0')
				continue;
			fputs("<li><a href=\"#", stdout);
			printfeednameid(feedcurrent->name);
			fputs("\">", stdout);
			if(feedcurrent->new > 0)
				fputs("<b><u>", stdout);
			fputs(feedcurrent->name, stdout);
			fprintf(stdout, " (%lu)", feedcurrent->new);
			if(feedcurrent->new > 0)
				fputs("</u></b>", stdout);
			fputs("</a></li>\n", stdout);
		}
		fputs("\t\t\t</ul>\n\t\t</div>\n", stdout);
	}
	fputs(
		"		</div>\n"
		"	</body>\n"
		"		<title>Newsfeeds (",
	stdout);
	fprintf(stdout, "%lu", totalnew);
	fputs(")</title>\n</html>", stdout);

	free(line); /* free line */
	feedsfree(feeds); /* free feeds linked-list */

	return EXIT_SUCCESS;
}
