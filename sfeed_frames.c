#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

static struct feed **feeds;
static char *line;
static size_t linesize;
static time_t comparetime;
static unsigned long totalnew;

static void
printfeed(FILE *fpitems, FILE *fpin, struct feed *f)
{
	char *fields[FieldLast];
	ssize_t linelen;
	unsigned int isnew;
	struct tm *tm;
	time_t parsedtime;

	/* menu if not unnamed */
	if (f->name[0]) {
		fputs("<h2 id=\"", fpitems);
		xmlencode(f->name, fpitems);
		fputs("\"><a href=\"#", fpitems);
		xmlencode(f->name, fpitems);
		fputs("\">", fpitems);
		xmlencode(f->name, fpitems);
		fputs("</a></h2>\n", fpitems);
	}

	while ((linelen = getline(&line, &linesize, fpin)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		if (!parseline(line, fields))
			break;

		parsedtime = 0;
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime))
			continue;
		if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		isnew = (parsedtime >= comparetime) ? 1 : 0;
		totalnew += isnew;
		f->totalnew += isnew;
		f->total++;

		fprintf(fpitems, "%04d-%02d-%02d&nbsp;%02d:%02d ",
		        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min);
		if (isnew)
			fputs("<b><u>", fpitems);
		if (fields[FieldLink][0]) {
			fputs("<a href=\"", fpitems);
			xmlencode(fields[FieldLink], fpitems);
			fputs("\">", fpitems);
			xmlencode(fields[FieldTitle], fpitems);
			fputs("</a>", fpitems);
		} else {
			xmlencode(fields[FieldTitle], fpitems);
		}
		if (isnew)
			fputs("</u></b>", fpitems);
		fputs("\n", fpitems);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fpindex, *fpitems, *fpmenu = NULL, *fp;
	char *name;
	int i, showsidebar = (argc > 1);
	struct feed *f;

	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");

	if (!(feeds = calloc(argc, sizeof(struct feed *))))
		err(1, "calloc");

	if ((comparetime = time(NULL)) == -1)
		err(1, "time");
	/* 1 day is old news */
	comparetime -= 86400;

	/* write main index page */
	if (!(fpindex = fopen("index.html", "wb")))
		err(1, "fopen: index.html");
	if (!(fpitems = fopen("items.html", "wb")))
		err(1, "fopen: items.html");
	if (showsidebar && !(fpmenu = fopen("menu.html", "wb")))
		err(1, "fopen: menu.html");

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

	fputs("<!DOCTYPE HTML>\n"
	      "<html>\n"
	      "\t<head>\n"
	      "\t<meta name=\"referrer\" content=\"no-referrer\" />\n"
	      "\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
	      "</head>\n"
	      "<body class=\"frame\"><div id=\"items\"><pre>\n", fpitems);

	if (argc == 1) {
		if (!(feeds[0] = calloc(1, sizeof(struct feed))))
			err(1, "calloc");
		feeds[0]->name = "";
		printfeed(fpitems, stdin, feeds[0]);
	} else {
		for (i = 1; i < argc; i++) {
			if (!(feeds[i - 1] = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
			feeds[i - 1]->name = name;

			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);
			printfeed(fpitems, fp, feeds[i - 1]);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}
	fputs("</pre>\n</div></body>\n</html>\n", fpitems); /* div items */

	if (showsidebar) {
		fputs("<!DOCTYPE HTML>\n"
		      "<html>\n"
		      "<head>\n"
		      "\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
		      "\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
		      "</head>\n"
		      "<body class=\"frame\">\n<div id=\"sidebar\">\n", fpmenu);

		for (i = 1; i < argc; i++) {
			f = feeds[i - 1];
			if (f->totalnew)
				fputs("<a class=\"n\" href=\"items.html#", fpmenu);
			else
				fputs("<a href=\"items.html#", fpmenu);
			xmlencode(f->name, fpmenu);
			fputs("\" target=\"items\">", fpmenu);
			if (f->totalnew > 0)
				fputs("<b><u>", fpmenu);
			xmlencode(f->name, fpmenu);
			fprintf(fpmenu, " (%lu)", f->totalnew);
			if (f->totalnew > 0)
				fputs("</u></b>", fpmenu);
			fputs("</a><br/>\n", fpmenu);
		}
		fputs("</div></body></html>\n", fpmenu);
	}
	fputs("<!DOCTYPE html>\n<html>\n<head>\n"
	      "\t<meta name=\"referrer\" content=\"no-referrer\" />\n"
	      "\t<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n"
	      "\t<title>Newsfeed (", fpindex);
	fprintf(fpindex, "%lu", totalnew);
	fputs(")</title>\n\t<link rel=\"stylesheet\" type=\"text/css\" href=\"style.css\" />\n"
	      "</head>\n", fpindex);
	if (showsidebar) {
		fputs("<frameset framespacing=\"0\" cols=\"250,*\" frameborder=\"1\">\n"
		      "\t<frame name=\"menu\" src=\"menu.html\" target=\"menu\">\n", fpindex);
	} else {
		fputs("<frameset framespacing=\"0\" cols=\"*\" frameborder=\"1\">\n", fpindex);
	}
	fputs(
	      "\t<frame name=\"items\" src=\"items.html\" target=\"items\">\n"
	      "</frameset>\n"
	      "</html>\n", fpindex);

	fclose(fpindex);
	fclose(fpitems);
	if (fpmenu)
		fclose(fpmenu);

	return 0;
}
