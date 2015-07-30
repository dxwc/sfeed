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

static void
printfeed(FILE *fp, const char *feedname)
{
	struct tm tm;
	char *fields[FieldLast], timebuf[32], mtimebuf[32];
	char host[HOST_NAME_MAX + 1], *user;
	time_t parsedtime;
	int r;

	if(!(user = getenv("USER")))
		user = "you";
	if (gethostname(host, sizeof(host)) == -1)
		err(1, "gethostname");

	if ((parsedtime = time(NULL)) == -1)
		err(1, "time");
	if (!gmtime_r(&parsedtime, &tm))
		errx(1, "gmtime_r: can't get current time");
	if (!strftime(mtimebuf, sizeof(mtimebuf), "%a %b %d %H:%M:%S %Y", &tm))
		errx(1, "can't format current time");

	while (parseline(&line, &linesize, fields, FieldLast, '\t', fp) > 0) {
		/* mbox header */
		printf("From MAILER-DAEMON  %s\n", mtimebuf);
		printf("From: %s <sfeed@>\n", feedname);
		printf("To: %s <%s@%s>\n", user, user, host);
		printf("Subject: %s\n", fields[FieldTitle]);

		printf("Message-Id: <%s-%s-sfeed>\n",
			fields[FieldUnixTimestamp],
			fields[FieldId]);

		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		if (r != -1) {
			if (gmtime_r(&parsedtime, &tm) &&
			   strftime(timebuf, sizeof(timebuf), "%d %b %y %H:%M +0000", &tm))
				printf("Date: %s\n", timebuf);
		}
		printf("Content-Type: text/%s; charset=UTF-8\n",
			fields[FieldContentType]);
		printf("Content-Transfer-Encoding: binary\n");

		if (*feedname != '\0')
			printf("X-Feedname: %s\n", feedname);
		printf("\nLink: %s", fields[FieldLink]);
		printf("\n\n");

		if (!strcmp(fields[FieldContentType], "html")) {
			printf("<html><body>\n");
			printcontent(fields[FieldContent], stdout);
			printf("</body></html>\n");
		} else {
			printcontent(fields[FieldContent], stdout);
		}
		fputs("\n", stdout);
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
