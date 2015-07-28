#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

static struct feed **feeds = NULL;
static int showsidebar = 0; /* show sidebar ? */
static char *line = NULL;
static size_t linesize = 0;
static unsigned long totalnew = 0;
static time_t comparetime;

static void
printfeed(FILE *fp, struct feed *f)
{
	char *fields[FieldLast];
	time_t parsedtime;
	unsigned int islink, isnew;
	int r;

	if (f->name[0] != '\0') {
		fputs("<h2 id=\"", stdout);
		printxmlencoded(f->name, stdout);
		fputs("\"><a href=\"#", stdout);
		printxmlencoded(f->name, stdout);
		fputs("\">", stdout);
		printxmlencoded(f->name, stdout);
		fputs("</a></h2>\n", stdout);
	}
	fputs("<table cellpadding=\"0\" cellspacing=\"0\">\n", stdout);

	while (parseline(&line, &linesize, fields, FieldLast, '\t', fp) > 0) {
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		islink = (fields[FieldLink][0] != '\0') ? 1 : 0;

		totalnew += isnew;
		f->totalnew += isnew;
		f->total++;

		if (isnew)
			fputs("<tr class=\"n\">", stdout);
		else
			fputs("<tr>", stdout);
		fputs("<td nowrap valign=\"top\">", stdout);
		fputs(fields[FieldTimeFormatted], stdout);
		fputs("</td><td nowrap valign=\"top\">", stdout);
		if (isnew)
			fputs("<b><u>", stdout);
		if (islink) {
			fputs("<a href=\"", stdout);
			printxmlencoded(fields[FieldLink], stdout);
			fputs("\">", stdout);
		}
		printxmlencoded(fields[FieldTitle], stdout);
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
	FILE *fp;
	int i;

	if (!(feeds = calloc(argc, sizeof(struct feed *))))
		err(1, "calloc");

	/* 1 day old is old news */
	comparetime = time(NULL) - 86400;

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
			feeds[i - 1]->name = xbasename(argv[i]);

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
			printxmlencoded(f->name, stdout);
			fputs("\">", stdout);
			if (f->totalnew > 0)
				fputs("<b><u>", stdout);
			printxmlencoded(f->name, stdout);
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
