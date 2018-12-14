#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tree.h"
#include "util.h"

static char *line;
static size_t linesize;
static int changed, firsttime = 1;
static time_t comparetime;

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
	char *fields[FieldLast];
	ssize_t linelen;
	time_t parsedtime;
	struct tm *tm;

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

		changed = 1;

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

		if (feedname[0]) {
			printutf8pad(stdout, feedname, 15, ' ');
			fputs("  ", stdout);
		}

		fprintf(stdout, "%04d-%02d-%02d %02d:%02d  ",
		        tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		        tm->tm_hour, tm->tm_min);
		printutf8pad(stdout, fields[FieldTitle], 70, ' ');
		printf(" %s\n", fields[FieldLink]);
	}
}

int
main(int argc, char *argv[])
{
	struct stat *stfiles, st;
	char *name;
	FILE *fp;
	int i, slept = 0;

	if (pledge("stdio rpath", NULL) == -1)
		err(1, "pledge");

	if (argc <= 1) {
		fprintf(stderr, "usage: %s <file>...\n", argv[0]);
		return 1;
	}

	setlocale(LC_CTYPE, "");

	if (!(stfiles = calloc(argc - 1, sizeof(*stfiles))))
		err(1, "calloc");

	while (1) {
		changed = 0;

		if ((comparetime = time(NULL)) == -1)
			err(1, "time");
		/* 1 day is old news */
		comparetime -= 86400;

		for (i = 1; i < argc; i++) {
			if (!(fp = fopen(argv[i], "r"))) {
				if (firsttime)
					err(1, "fopen: %s", argv[i]);
				/* don't report when the file is missing after the first run */
				continue;
			}
			if (fstat(fileno(fp), &st) == -1) {
				if (firsttime)
					err(1, "fstat: %s", argv[i]);
				warn("fstat: %s", argv[i]);
				fclose(fp);
				continue;
			}

			/* did the file change? by size or modification time */
			if (stfiles[i - 1].st_size != st.st_size ||
			    stfiles[i - 1].st_mtime != st.st_mtime) {
				name = ((name = strrchr(argv[i], '/'))) ? name + 1 : argv[i];
				printfeed(fp, name);
				if (ferror(fp))
					warn("ferror: %s", argv[i]);
				memcpy(&stfiles[i - 1], &st, sizeof(st));
			}

			fclose(fp);
		}

		/* "garbage collect" on a change or every 5 minutes */
		if (changed || slept > 300) {
			gc();
			changed = 0;
			slept = 0;
		}
		sleep(10);
		slept += 10;
		firsttime = 0;
	}
	return 0;
}
