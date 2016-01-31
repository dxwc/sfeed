#include <ctype.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "util.h"

static int firsttime;
static char *line;
static size_t linesize;

struct line {
	char *timestamp;
	char *id;
	struct line *next;
};

/* ofcourse: bigger bucket size uses more memory, but has less collisions. */
#define BUCKET_SIZE 65535
struct bucket {
	struct line cols[BUCKET_SIZE];
};
static struct bucket *buckets;
static struct bucket *bucket;

static char *
estrdup(const char *s)
{
	char *p;

	if (!(p = strdup(s)))
		err(1, "strdup");
	return p;
}

static void *
ecalloc(size_t nmemb, size_t size)
{
	void *p;

	if (!(p = calloc(nmemb, size)))
		err(1, "calloc");
	return p;
}

/* jenkins one-at-a-time hash */
static uint32_t
jenkins1(const char *s)
{
	uint32_t hash = 0;

	for (; *s; s++) {
		hash += (int)*s;
		hash += (hash << 10);
		hash ^= (hash >> 6);
	}
	hash += (hash << 3);
	hash ^= (hash >> 11);

	return hash + (hash << 15);
}

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
	struct line *match;
	char *fields[FieldLast];
	uint32_t hash;
	int uniq;

	while (parseline(&line, &linesize, fields, fp) > 0) {
		hash = (jenkins1(fields[FieldUnixTimestamp]) +
		       jenkins1(fields[FieldId])) % BUCKET_SIZE;
		for (uniq = 1, match = &(bucket->cols[hash]);
		     match;
		     match = match->next) {
			/* check for collision, can still be unique. */
			if (match->id && !strcmp(match->id, fields[FieldId]) &&
			    match->timestamp && !strcmp(match->timestamp, fields[FieldUnixTimestamp])) {
				uniq = 0;
				break;
			}
			/* nonexistent or no collision */
			if (!match->next) {
				match = match->next = ecalloc(1, sizeof(struct line));
				match->id = estrdup(fields[FieldId]);
				match->timestamp = estrdup(fields[FieldUnixTimestamp]);
					break;
			}
		}
		if (!uniq || firsttime)
			continue;
		if (feedname[0])
			printf("%-15.15s %-30.30s",
			       feedname, fields[FieldTimeFormatted]);
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

	bucket = buckets = ecalloc(argc, sizeof(struct bucket));
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
