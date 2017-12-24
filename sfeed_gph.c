#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "util.h"

static time_t comparetime;
static char *line;
static size_t linesize;

/* format `len' columns of characters. If string is shorter pad the rest
 * with characters `pad`. */
int
utf8pad(char *buf, size_t bufsiz, const char *s, size_t len, int pad)
{
	wchar_t w;
	size_t col = 0, i, slen, siz = 0;
	int rl, wc;

	if (!len)
		return -1;

	slen = strlen(s);
	for (i = 0; i < slen && col < len + 1; i += rl) {
		if ((rl = mbtowc(&w, &s[i], slen - i < 4 ? slen - i : 4)) <= 0)
			break;
		if ((wc = wcwidth(w)) == -1)
			wc = 1;
		col += wc;
		if (col >= len && s[i + rl]) {
			if (siz + 4 >= bufsiz)
				return -1;
			memcpy(&buf[siz], "\xe2\x80\xa6", 4);
			return 0;
		}
		if (siz + rl + 1 >= bufsiz)
			return -1;
		memcpy(&buf[siz], &s[i], rl);
		siz += rl;
		buf[siz] = '\0';
	}

	len -= col;
	if (siz + len + 1 >= bufsiz)
		return -1;
	memset(&buf[siz], pad, len);
	siz += len;
	buf[siz] = '\0';

	return 0;
}

/* Escape characters in links in geomyidae .gph format */
void
gphlink(FILE *fp, const char *s, size_t len)
{
	size_t i;

	for (i = 0; *s && i < len; i++) {
		switch (s[i]) {
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
			fputc(s[i], fp);
			break;
		}
	}
}

static void
printfeed(FILE *fp, const char *feedname)
{
	char *fields[FieldLast], buf[1024];
	struct tm *tm;
	time_t parsedtime;
	ssize_t linelen;
	size_t lc;

	for (lc = 0; (linelen = getline(&line, &linesize, fp)) > 0; lc++) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		if (!parseline(line, fields))
			break;

		parsedtime = 0;
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime))
			continue;
	        if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		if (lc == 0 && feedname[0]) {
			fputs("\n  ", stdout);
			utf8pad(buf, sizeof(buf), feedname, 77, ' ');
			gphlink(stdout, buf, strlen(buf));
			fputs("\n  ", stdout);
			memset(buf, '=', 77);
			buf[78] = '\0';
			puts(buf);
		}

		fputs("[h|", stdout);
		if (parsedtime >= comparetime)
			fputs("N ", stdout);
		else
			fputs("  ", stdout);

	        fprintf(stdout, "%04d-%02d-%02d %02d:%02d  ",
		        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min);
		utf8pad(buf, sizeof(buf), fields[FieldTitle], 59, ' ');
		gphlink(stdout, buf, strlen(buf));
		fputs("|URL:", stdout);
		gphlink(stdout, fields[FieldLink], strlen(fields[FieldLink]));
		fputs("|server|port]\n", stdout);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *name;
	int i;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	setlocale(LC_CTYPE, "");

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

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
			name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
			printfeed(fp, name);
			if (ferror(fp))
				err(1, "ferror: %s", argv[i]);
			fclose(fp);
		}
	}
	return 0;
}
