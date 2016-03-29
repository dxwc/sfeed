#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util.h"

static int firsttime;
static char *line;
static size_t linesize;

struct line {
	char *s;
	size_t len;
	struct line *next;
};

/* ofcourse: bigger bucket size uses more memory, but has less collisions. */
#define BUCKET_SIZE 16384
struct bucket {
	struct line cols[BUCKET_SIZE];
};
static struct bucket *buckets;
static struct bucket *bucket;
static const uint32_t seed = 1167266473;

static void
printfeed(FILE *fp, const char *feedname)
{
	struct line *match;
	char *fields[FieldLast];
	uint32_t hash;
	int uniq;
	ssize_t linelen;
	time_t parsedtime;
	struct tm *tm;

	while ((linelen = getline(&line, &linesize, fp)) > 0) {
		if (line[linelen - 1] == '\n')
			line[--linelen] = '\0';
		hash = murmur3_32(line, (size_t)linelen, seed) % BUCKET_SIZE;

		for (uniq = 1, match = &(bucket->cols[hash]);
		     match;
		     match = match->next) {
			/* check for collision, can still be unique. */
			if (match->s && match->len == (size_t)linelen &&
			    !strcmp(line, match->s)) {
				uniq = 0;
				break;
			}
			/* nonexistent or no collision */
			if (!match->next) {
				if (!(match = match->next = calloc(1, sizeof(struct line))))
					err(1, "calloc");
				if (!(match->s = strdup(line)))
					err(1, "strdup");
				match->len = (size_t)linelen;
				break;
			}
		}

		if (!uniq || firsttime)
			continue;
		if (!parseline(line, fields))
			break;

		parsedtime = 0;
		strtotime(fields[FieldUnixTimestamp], &parsedtime);
		if (!(tm = localtime(&parsedtime)))
			err(1, "localtime");

		if (feedname[0])
			printf("%-15.15s  ", feedname);

		printf("%04d-%02d-%02d %02d:%02d  ",
		       tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		       tm->tm_hour, tm->tm_min);
		printutf8pad(stdout, fields[FieldTitle], 70, ' ');
		printf(" %s\n", fields[FieldLink]);
	}
}

int
main(int argc, char *argv[])
{
	char *name;
	FILE *fp;
	int i;

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

	if (!(bucket = buckets = calloc(argc, sizeof(struct bucket))))
		err(1, "calloc");
	for (firsttime = (argc > 1); ; firsttime = 0) {
		if (argc == 1) {
			printfeed(stdin, "");
		} else {
			for (i = 1; i < argc; i++) {
				bucket = &buckets[i - 1];
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
		sleep(60);
	}
	return 0;
}
