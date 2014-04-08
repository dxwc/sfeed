#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

#include "util.h"

static struct feed *feeds = NULL; /* start of feeds linked-list. */
static char *line = NULL;

static void
cleanup(void) {
	free(line); /* free line */
	feedsfree(feeds); /* free feeds linked-list */
}

static void /* print error message to stderr */
die(const char *s) {
	fputs("sfeed_stats: ", stderr);
	fputs(s, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

int
main(void) {
	char *fields[FieldLast], timenewestformat[64] = "";
	unsigned long totalfeeds = 0, totalnew = 0, totalitems = 0;
	unsigned int islink, isnew;
	struct feed *f, *feedcurrent = NULL;
	time_t parsedtime, comparetime, timenewest = 0;
	size_t size = 0;

	atexit(cleanup);
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */

	if(!(feedcurrent = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	feeds = feedcurrent;

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		isnew = (parsedtime >= comparetime);
		islink = (fields[FieldLink][0] != '\0');
		/* first of feed section or new feed section. */
		/* TODO: allocate feedcurrent before here, feedcurrent can be NULL */
		if(!totalfeeds || (feedcurrent && strcmp(feedcurrent->name, fields[FieldFeedName]))) {
			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");
			if(totalfeeds) { /* end previous one. */
				feedcurrent->next = f;
				feedcurrent = f;
			} else {
				feedcurrent = f;
				feeds = feedcurrent; /* first item. */
			}
			if(parsedtime > timenewest) {
				timenewest = parsedtime;
				strlcpy(timenewestformat, fields[FieldTimeFormatted],
				        sizeof(timenewestformat));
			}
			if(parsedtime > feedcurrent->timenewest) {
				feedcurrent->timenewest = parsedtime;
				strlcpy(feedcurrent->timenewestformat, fields[FieldTimeFormatted],
				        sizeof(feedcurrent->timenewestformat));
			}

			/* TODO: memcpy and make feedcurrent->name static? */
			if(!(feedcurrent->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");

			totalfeeds++;
		}
		totalnew += isnew;
		feedcurrent->totalnew += isnew;
		feedcurrent->total++;
		totalitems++;
	}
	for(feedcurrent = feeds; feedcurrent; feedcurrent = feedcurrent->next) {
		if(!feedcurrent->name || feedcurrent->name[0] == '\0')
			continue;
		fprintf(stdout, "%c %-20.20s [%4lu/%-4lu]",
		        feedcurrent->totalnew > 0 ? 'N' : ' ',
		        feedcurrent->name, feedcurrent->totalnew, feedcurrent->total);
		if(feedcurrent->timenewestformat && feedcurrent->timenewestformat[0])
			fprintf(stdout, " %s", feedcurrent->timenewestformat);
		putchar('\n');
	}
	printf("  ================================\n");
	printf("%c %-20.20s [%4lu/%-4lu] %s\n", totalnew > 0 ? 'N' : ' ', "Total:",
	       totalnew, totalitems, timenewestformat);
	return EXIT_SUCCESS;
}
