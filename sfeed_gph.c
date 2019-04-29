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
static char *prefixpath;
static char *line;
static size_t linesize;
static time_t comparetime;
static unsigned long totalnew;

/* Escape characters in links in geomyidae .gph format */
void
gphlink(FILE *fp, const char *s, size_t len)
{
	size_t i;

	for (i = 0; *s && i < len; s++, i++) {
		switch (*s) {
		case '\r': /* ignore CR */
		case '\n': /* ignore LF */
			break;
		case '\t':
			fputs("        ", fp);
			break;
		case '|': /* escape separators */
			fputs("\\|", fp);
			break;
		default:
			fputc(*s, fp);
			break;
		}
	}
}

static void
printfeed(FILE *fpitems, FILE *fpin, struct feed *f)
{
	char *fields[FieldLast];
	ssize_t linelen;
	unsigned int isnew;
	struct tm *tm;
	time_t parsedtime;

	if (f->name[0])
		fprintf(fpitems, "t%s\n\n", f->name);

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

		if (fields[FieldLink][0]) {
			fputs("[h|", fpitems);
			fprintf(fpitems, "%04d-%02d-%02d %02d:%02d ",
			        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			        tm->tm_hour, tm->tm_min);
			gphlink(fpitems, fields[FieldTitle], strlen(fields[FieldTitle]));
			fputs("|URL:", fpitems);
			gphlink(fpitems, fields[FieldLink], strlen(fields[FieldLink]));
			fputs("|server|port]\n", fpitems);
		} else {
			fprintf(fpitems, "%04d-%02d-%02d %02d:%02d %s\n",
			        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
			        tm->tm_hour, tm->tm_min, fields[FieldTitle]);
		}
	}
}

int
main(int argc, char *argv[])
{
	FILE *fpitems, *fpindex, *fp;
	char *name, path[PATH_MAX + 1];
	int i;
	struct feed *f;

	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");

	if (!(prefixpath = getenv("SFEED_GPH_PATH")))
		prefixpath = "/";

	if (!(feeds = calloc(argc, sizeof(struct feed *))))
		err(1, "calloc");

	if ((comparetime = time(NULL)) == -1)
		err(1, "time");
	/* 1 day is old news */
	comparetime -= 86400;

	if (argc == 1) {
		if (pledge("stdio", NULL) == -1)
			err(1, "pledge");
		if (!(feeds[0] = calloc(1, sizeof(struct feed))))
			err(1, "calloc");
		feeds[0]->name = "";
		printfeed(stdout, stdin, feeds[0]);
	} else {
		/* write main index page */
		if (!(fpindex = fopen("index.gph", "wb")))
			err(1, "fopen: index.gph");

		for (i = 1; i < argc; i++) {
			if (!(feeds[i - 1] = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			f = feeds[i - 1];
			name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
			f->name = name;

			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);

			snprintf(path, sizeof(path), "%s.gph", f->name);
			if (!(fpitems = fopen(path, "wb")))
				err(1, "fopen");
			printfeed(fpitems, fp, f);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
			fclose(fpitems);

			/* append directory item to index */
			fprintf(fpindex, "[1|");
			gphlink(fpindex, f->name, strlen(f->name));
			fprintf(fpindex, " (%lu/%lu)|%s",
			        f->totalnew, f->total, prefixpath);
			snprintf(path, sizeof(path), "%s.gph", f->name);
			gphlink(fpindex, path, strlen(path));
			fputs("|server|port]\n", fpindex);
		}
		fclose(fpindex);
	}

	return 0;
}
