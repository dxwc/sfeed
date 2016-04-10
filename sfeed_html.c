#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

static struct feed **feeds;
static int showsidebar;
static char *line;
static size_t linesize;
static unsigned long totalnew;
static time_t comparetime;

static void
printfeed(FILE *fp, struct feed *f)
{
	char *fields[FieldLast];
	struct tm *tm;
	time_t parsedtime;
	unsigned int islink, isnew;
	ssize_t linelen;

	if (f->name[0]) {
		fputs("<h2 id=\"", stdout);
		xmlencode(f->name, stdout);
		fputs("\"><a href=\"#", stdout);
		xmlencode(f->name, stdout);
		fputs("\">", stdout);
		xmlencode(f->name, stdout);
		fputs("</a></h2>\n", stdout);
	}
	fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", stdout);

	while ((linelen = getline(&line, &linesize, fp)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		if (!parseline(line, fields))
			break;

		parsedtime = 0;
		strtotime(fields[FieldUnixTimestamp], &parsedtime);
	        if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		isnew = (parsedtime >= comparetime) ? 1 : 0;
		islink = fields[FieldLink][0] ? 1 : 0;

		totalnew += isnew;
		f->totalnew += isnew;
		f->total++;

		if (isnew)
			fputs("<tr class=\"n\">", stdout);
		else
			fputs("<tr>", stdout);
		fputs("<td nowrap valign=\"top\">", stdout);
	        fprintf(stdout, "%04d-%02d-%02d&nbsp;%02d:%02d",
		        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min);
		fputs("</td><td nowrap valign=\"top\">", stdout);
		if (isnew)
			fputs("<b><u>", stdout);
		if (islink) {
			fputs("<a href=\"", stdout);
			xmlencode(fields[FieldLink], stdout);
			fputs("\">", stdout);
		}
		xmlencode(fields[FieldTitle], stdout);
		if (islink)
			fputs("</a>", stdout);
		if (isnew)
			fputs("</u></b>", stdout);
		fputs("</td></tr>\n", stdout);
	}
	fputs("</table>\n", stdout);
}

int
main(int argc, char *argv[])
{
	struct feed *f;
	char *name;
	FILE *fp;
	int i;

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

	if (!(feeds = calloc(argc, sizeof(struct feed *))))
		err(1, "calloc");
	if ((comparetime = time(NULL)) == -1)
		err(1, "time");
	/* 1 day old is old news */
	comparetime -= 86400;

	fputs("<!DOCTYPE HTML>\n"
	      "<html>\n"
	      "\t<head>\n"
	      "\t\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "\t\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
	      "\t</head>\n"
	      "\t<body class=\"noframe\">\n", stdout);

	showsidebar = (argc > 1);
	if (showsidebar)
		fputs("\t\t<div id=\"items\">\n", stdout);
	else
		fputs("\t\t<div id=\"items\" class=\"nosidebar\">\n", stdout);

	if (argc == 1) {
		if (!(feeds[0] = calloc(1, sizeof(struct feed))))
			err(1, "calloc");
		feeds[0]->name = "";
		printfeed(stdin, feeds[0]);
		if (ferror(stdin))
			err(1, "ferror: <stdin>:");
	} else {
		for (i = 1; i < argc; i++) {
			if (!(feeds[i - 1] = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
			feeds[i - 1]->name = name;
			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);
			printfeed(fp, feeds[i - 1]);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}
	fputs("</div>\n", stdout); /* div items */

	if (showsidebar) {
		fputs("\t<div id=\"sidebar\">\n\t\t<ul>\n", stdout);

		for (i = 1; i < argc; i++) {
			f = feeds[i - 1];
			if (f->totalnew > 0)
				fputs("<li class=\"n\"><a href=\"#", stdout);
			else
				fputs("<li><a href=\"#", stdout);
			xmlencode(f->name, stdout);
			fputs("\">", stdout);
			if (f->totalnew > 0)
				fputs("<b><u>", stdout);
			xmlencode(f->name, stdout);
			fprintf(stdout, " (%lu)", f->totalnew);
			if (f->totalnew > 0)
				fputs("</u></b>", stdout);
			fputs("</a></li>\n", stdout);
		}
		fputs("\t\t</ul>\n\t</div>\n", stdout);
	}

	fprintf(stdout, "\t</body>\n\t<title>Newsfeed (%lu)</title>\n</html>",
	        totalnew);

	return 0;
}
