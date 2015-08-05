#include <sys/types.h>

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

static char *line = NULL;
static size_t linesize = 0;

/* Unescape / decode fields printed by string_print_encoded()
 * "\\" to "\", "\t", to TAB, "\n" to newline. Unrecognised escape sequences
 * are ignored: "\z" etc. Mangle "From " in mboxrd style (always prefix >). */
static void
printcontent(const char *s, FILE *fp)
{
	if (!strncmp(s, "From ", 5))
		fputc('>', fp);

	for (; *s; s++) {
read:
		switch (*s) {
		case '\\':
			switch (*(++s)) {
			case '\0': return; /* ignore */
			case '\\': fputc('\\', fp); break;
			case 't':  fputc('\t', fp); break;
			case 'n':
				fputc('\n', fp);
				for (s++; *s && *s == '>'; s++)
					fputc('>', fp);
				/* escape "From ", mboxrd-style. */
				if (!strncmp(s, "From ", 5))
					fputc('>', fp);
				goto read;
			}
			break;
		case '\0': return; /* ignore */
		case '\n':
			fputc((int)*s, fp);
			for (s++; *s && *s == '>'; s++)
				fputc('>', fp);
			/* escape "From ", mboxrd-style. */
			if (!strncmp(s, "From ", 5))
				fputc('>', fp);
			goto read;
		default:
			fputc((int)*s, fp);
		}
	}
}

static void
printfeed(FILE *fp, const char *feedname)
{
	struct tm tm;
	char *fields[FieldLast], timebuf[32], mtimebuf[32];
	char host[HOST_NAME_MAX + 1], *user;
	time_t parsedtime;
	int r;

	if (!(user = getenv("USER")))
		user = "you";
	if (gethostname(host, sizeof(host)) == -1)
		err(1, "gethostname");

	if ((parsedtime = time(NULL)) == -1)
		err(1, "time");
	if (!gmtime_r(&parsedtime, &tm))
		errx(1, "gmtime_r: can't get current time");
	if (!strftime(mtimebuf, sizeof(mtimebuf), "%a %b %d %H:%M:%S %Y", &tm))
		errx(1, "can't format current time");

	while (parseline(&line, &linesize, fields, fp) > 0) {
		if ((r = strtotime(fields[FieldUnixTimestamp], &parsedtime)) == -1 ||
		    !gmtime_r(&parsedtime, &tm) ||
		    !strftime(timebuf, sizeof(timebuf),
		    "%a, %d %b %Y %H:%M +0000", &tm))
			continue; /* invalid date */

		/* mbox + mail header */
		printf("From MAILER-DAEMON %s\n"
			"Date: %s\n"
			"From: %s <sfeed@>\n"
			"To: %s <%s@%s>\n"
			"Subject: %s\n"
			"Message-ID: <%s-%s-sfeed>\n"
			"Content-Type: text/%s; charset=UTF-8\n"
			"Content-Transfer-Encoding: binary\n"
			"X-Feedname: %s\n"
			"\n",
			mtimebuf, timebuf, fields[FieldAuthor],
			user, user, host, fields[FieldTitle],
			fields[FieldUnixTimestamp], fields[FieldId],
			fields[FieldContentType], feedname);

		if (!strcmp(fields[FieldContentType], "html")) {
			fputs("<p>Link: <a href=\"", stdout);
			xmlencode(fields[FieldLink], stdout);
			fputs("\">", stdout);
			fputs(fields[FieldLink], stdout);
			fputs("</a></p>\n\n", stdout);
			printcontent(fields[FieldContent], stdout);
		} else {
			printf("Link: %s\n\n", fields[FieldLink]);
			printcontent(fields[FieldContent], stdout);
		}
		fputs("\n\n", stdout);
	}
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	char *name;
	int i;

	if (argc == 1) {
		printfeed(stdin, "");
	} else {
		for (i = 1; i < argc; i++) {
			fp = fopen(argv[i], "r");
			if (!fp)
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
