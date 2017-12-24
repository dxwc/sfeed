#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tree.h"
#include "util.h"

static int firsttime;
static int sleepsecs;
static char *line;
static size_t linesize;
time_t comparetime;

struct line {
	char *id;
	char *link;
	char *title;
	time_t timestamp;
	RB_ENTRY(line) entry;
};

int
linecmp(struct line *e1, struct line *e2)
{
	int r;

	if ((r = strcmp(e1->id, e2->id)))
		return r;
	else if ((r = strcmp(e1->title, e2->title)))
		return r;
	return strcmp(e1->link, e2->link);
}
RB_HEAD(linetree, line) head = RB_INITIALIZER(&head);
RB_GENERATE_STATIC(linetree, line, entry, linecmp)

/* remove old entries from the tree that won't be shown anyway. */
static void
gc(void)
{
	struct line *line, *tmp;

	RB_FOREACH_SAFE(line, linetree, &head, tmp) {
		if (line->timestamp < comparetime) {
/*			printf("DEBUG: gc: removing: %s %s\n",
				line->id, line->title);*/
			free(line->id);
			free(line->link);
			free(line->title);
			RB_REMOVE(linetree, &head, line);
			free(line);
		}
	}
}

static void
printfeed(FILE *fp, const char *feedname)
{
	struct line *add, search;
	char *fields[FieldLast], *s;
	ssize_t linelen;
	time_t parsedtime;
	struct tm *tm;
	int i;

	while ((linelen = getline(&line, &linesize, fp)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';

		if (!parseline(line, fields))
			break;
		parsedtime = 0;
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime))
			continue;
		if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		/* old news: skip */
		if (parsedtime < comparetime)
			continue;

		search.id = fields[FieldId];
		search.link = fields[FieldLink];
		search.title = fields[FieldTitle];
		search.timestamp = parsedtime;
		if (RB_FIND(linetree, &head, &search))
			continue;

/*		printf("DEBUG: new: id: %s, link: %s, title: %s\n",
			fields[FieldId], fields[FieldLink], fields[FieldTitle]);*/

		if (!(add = calloc(1, sizeof(*add))))
			err(1, "calloc");
		if (!(add->id = strdup(fields[FieldId])))
			err(1, "strdup");
		if (!(add->link = strdup(fields[FieldLink])))
			err(1, "strdup");
		if (!(add->title = strdup(fields[FieldTitle])))
			err(1, "strdup");
		add->timestamp = parsedtime;
		RB_INSERT(linetree, &head, add);

		if (firsttime)
			continue;

		/* output parsed line: it may not be the same as the input. */
		for (i = 0; i < FieldLast; i++) {
			if (i)
				putchar('\t');
			fputs(fields[i], stdout);
		}
		putchar('\n');
		fflush(stdout);

#if 0
		if (fields[FieldFeedName][0])
			printf("%-15.15s  ", fields[FieldFeedName]);
		else if (feedname[0])
			printf("%-15.15s  ", feedname);
		printf("%04d-%02d-%02d %02d:%02d  ",
		       tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		       tm->tm_hour, tm->tm_min);
		printutf8pad(stdout, fields[FieldTitle], 70, ' ');
		printf(" %s\n", fields[FieldLink]);
#endif
	}
}

int
main(int argc, char *argv[])
{
	char *name;
	FILE *fp;
	int i, slept = 0;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	setlocale(LC_CTYPE, "");

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

	if (argc == 1)
		sleepsecs = 1;
	else
		sleepsecs = 300;

	for (firsttime = (argc > 1); ; firsttime = 0) {
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
		// DEBUG: TODO: gc first run.
		gc();

		sleep(sleepsecs);
		slept += sleepsecs;

		/* gc once every hour (excluding run-time) */
		if (slept >= 3600) {
			gc();
			slept = 0;
		}
	}
	return 0;
}
