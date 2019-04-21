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

#define ROT32(x, y) ((x << y) | (x >> (32 - y)))

static uint32_t
murmur3_32(const char *key, uint32_t len, uint32_t seed)
{
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m = 5;
	static const uint32_t n = 0xe6546b64;
	uint32_t hash = seed;
	const int nblocks = len / 4;
	const uint32_t *blocks = (const uint32_t *) key;
	int i;
	uint32_t k, k1;
	const uint8_t *tail;

	for (i = 0; i < nblocks; i++) {
		k = blocks[i];
		k *= c1;
		k = ROT32(k, r1);
		k *= c2;

		hash ^= k;
		hash = ROT32(hash, r2) * m + n;
	}
	tail = (const uint8_t *) (key + nblocks * 4);

	k1 = 0;
	switch (len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];

		k1 *= c1;
		k1 = ROT32(k1, r1);
		k1 *= c2;
		hash ^= k1;
	}

	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);

	return hash;
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
		if (strtotime(fields[FieldUnixTimestamp], &parsedtime))
			continue;

		/* mbox + mail header */
		printf("From MAILER-DAEMON %s\n", mtimebuf);
		/* can't convert: default to formatted time for time_t 0. */
		if (gmtime_r(&parsedtime, &tm) &&
		    strftime(timebuf, sizeof(timebuf),
		             "%a, %d %b %Y %H:%M:%S +0000", &tm))
			printf("Date: %s\n", timebuf);
		else
			printf("Date: Thu, 01 Jan 1970 00:00:00 +0000\n");

		printf("From: %s <sfeed@>\n", fields[FieldAuthor][0] ? fields[FieldAuthor] : "unknown");
		printf("To: %s <%s@%s>\n", user, user, host);
		if (feedname[0])
			printf("Subject: [%s] %s\n", feedname, fields[FieldTitle]);
		else
			printf("Subject: %s\n", fields[FieldTitle]);
		printf("Message-ID: <%s%s%"PRIu32"@%s>\n",
		       fields[FieldUnixTimestamp],
		       fields[FieldUnixTimestamp][0] ? "." : "",
		       murmur3_32(line, (size_t)linelen, seed),
		       feedname);
		printf("Content-Type: text/plain; charset=\"utf-8\"\n");
		printf("Content-Transfer-Encoding: binary\n");
		printf("X-Feedname: %s\n\n", feedname);

		printf("%s\n", fields[FieldLink]);
		if (fields[FieldEnclosure][0])
			printf("\nEnclosure:\n%s\n", fields[FieldEnclosure]);
		fputs("\n", stdout);
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

	if (pledge(argc == 1 ? "stdio" : "stdio rpath", NULL) == -1)
		err(1, "pledge");

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
