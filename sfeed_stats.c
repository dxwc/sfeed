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
	unsigned int isnew;
	struct feed *f, *fcur = NULL;
	time_t parsedtime, comparetime, timenewest = 0;
	size_t size = 0;

	atexit(cleanup);
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */

	if(!(fcur = calloc(1, sizeof(struct feed))))
		die("can't allocate enough memory");
	feeds = fcur;

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		isnew = (parsedtime >= comparetime);
		/* first of feed section or new feed section. */
		/* TODO: allocate fcur before here, fcur can be NULL */
		if(!totalfeeds || (fcur && strcmp(fcur->name, fields[FieldFeedName]))) {
			if(!(f = calloc(1, sizeof(struct feed))))
				die("can't allocate enough memory");
			if(totalfeeds) { /* end previous one. */
				fcur->next = f;
				fcur = f;
			} else {
				fcur = f;
				feeds = fcur; /* first item. */
			}
			if(parsedtime > timenewest) {
				timenewest = parsedtime;
				strlcpy(timenewestformat, fields[FieldTimeFormatted],
				        sizeof(timenewestformat));
			}
			if(parsedtime > fcur->timenewest) {
				fcur->timenewest = parsedtime;
				strlcpy(fcur->timenewestformat, fields[FieldTimeFormatted],
				        sizeof(fcur->timenewestformat));
			}

			/* TODO: memcpy and make fcur->name static? */
			if(!(fcur->name = strdup(fields[FieldFeedName])))
				die("can't allocate enough memory");

			totalfeeds++;
		}
		totalnew += isnew;
		fcur->totalnew += isnew;
		fcur->total++;
		totalitems++;
	}
	for(fcur = feeds; fcur; fcur = fcur->next) {
		if(!fcur->name || fcur->name[0] == '\0')
			continue;
		fprintf(stdout, "%c %-20.20s [%4lu/%-4lu]",
		        fcur->totalnew > 0 ? 'N' : ' ',
		        fcur->name, fcur->totalnew, fcur->total);
		if(fcur->timenewestformat && fcur->timenewestformat[0])
			fprintf(stdout, " %s", fcur->timenewestformat);
		putchar('\n');
	}
	printf("  ================================\n");
	printf("%c %-20.20s [%4lu/%-4lu] %s\n", totalnew > 0 ? 'N' : ' ', "Total:",
	       totalnew, totalitems, timenewestformat);
	return EXIT_SUCCESS;
}
