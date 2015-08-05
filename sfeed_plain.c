#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "util.h"

static time_t comparetime;
static char *line = NULL;
static size_t size = 0;

/* print `len' columns of characters. If string is shorter pad the rest
 * with characters `pad`. */
static void
printutf8pad(FILE *fp, const char *s, size_t len, int pad)
{
	wchar_t w;
	size_t n = 0, i;
	int r;

	for (i = 0; *s && n < len; i++, s++) {
		if (ISUTF8(*s)) {
			if ((r = mbtowc(&w, s, 4)) == -1)
				break;
			if ((r = wcwidth(w)) == -1)
				r = 1;
			n += (size_t)r;
		}
		putc(*s, fp);
	}
	for (; n < len; n++)
		putc(pad, fp);
}

static void
printfeed(FILE *fp, const char *feedname)
{
	char *fields[FieldLast];
	time_t parsedtime;

	while (parseline(&line, &size, fields, fp) > 0) {
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime) != -1 &&
		   parsedtime >= comparetime)
			fputs("N ", stdout);
		else
			fputs("  ", stdout);

		if (feedname[0])
			printf("%-15.15s ", feedname);
		printf(" %-30.30s  ", fields[FieldTimeFormatted]);
		printutf8pad(stdout, fields[FieldTitle], 70, ' ');
		printf("  %s\n", fields[FieldLink]);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *name;
	int i;

	if ((comparetime = time(NULL)) == -1)
		err(1, "time");
	/* 1 day is old news */
	comparetime -= 86400;

	if (argc == 1) {
		printfeed(stdin, "");
	} else {
		for (i = 1; i < argc; i++) {
			if (!(fp = fopen(argv[i], "r")))
				err(1, "fopen: %s", argv[i]);
			name = xbasename(argv[i]);
			printfeed(fp, name);
			free(name);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}
	return 0;
}
