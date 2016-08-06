#include <sys/types.h>

#include <err.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

static char *line;
static size_t linesize;
static char host[256], *user, mtimebuf[32];
static const uint32_t seed = 1167266473;

/* Unescape / decode fields printed by string_print_encoded()
 * "\\" to "\", "\t", to TAB, "\n" to newline. Unrecognised escape sequences
 * are ignored: "\z" etc. Mangle "From " in mboxrd style (always prefix >). */
static void
printcontent(const char *s, FILE *fp)
{
	if (!strncmp(s, "From ", 5))
		fputc('>', fp);

read:
	for (; *s; s++) {
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
	char *fields[FieldLast], timebuf[32];
	time_t parsedtime;
	ssize_t linelen;

	while ((linelen = getline(&line, &linesize, fp)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		if (!parseline(line, fields))
			break;
		parsedtime = 0;
		strtotime(fields[FieldUnixTimestamp], &parsedtime);
		/* can't convert: default to formatted time for time_t 0. */
		if (!gmtime_r(&parsedtime, &tm) ||
		    !strftime(timebuf, sizeof(timebuf),
		    "%a, %d %b %Y %H:%M +0000", &tm))
			strlcpy(timebuf, "Thu, 01 Jan 1970 00:00 +0000",
		                sizeof(timebuf));

		/* mbox + mail header */
		printf("From MAILER-DAEMON %s\n"
			"Date: %s\n"
			"From: %s <sfeed@>\n"
			"To: %s <%s@%s>\n"
			"Subject: %s\n"
			"Message-ID: <%s%s%"PRIu32"@%s>\n"
			"Content-Type: text/%s; charset=UTF-8\n"
			"Content-Transfer-Encoding: binary\n"
			"X-Feedname: %s\n"
			"\n",
			mtimebuf, timebuf,
			fields[FieldAuthor][0] ? fields[FieldAuthor] : "anonymous",
			user, user, host, fields[FieldTitle],
			fields[FieldUnixTimestamp],
			fields[FieldUnixTimestamp][0] ? "." : "",
			murmur3_32(line, (size_t)linelen, seed),
			feedname[0] ? feedname : "unnamed",
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
	struct tm tm;
	time_t t;
	FILE *fp;
	char *name;
	int i;

#ifdef USE_PLEDGE
	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");
#endif

	if (!(user = getenv("USER")))
		user = "you";
	if (gethostname(host, sizeof(host)) == -1)
		err(1, "gethostname");
	if ((t = time(NULL)) == -1)
		err(1, "time");
	if (!gmtime_r(&t, &tm))
		errx(1, "gmtime_r: can't get current time");
	if (!strftime(mtimebuf, sizeof(mtimebuf), "%a %b %d %H:%M:%S %Y", &tm))
		errx(1, "strftime: can't format current time");

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
