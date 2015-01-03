#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

static struct feed *feeds = NULL; /* start of feeds linked-list. */
static char *line = NULL;

int
main(void)
{
	char *fields[FieldLast], timenewestformat[64] = "";
	unsigned long totalfeeds = 0, totalnew = 0, totalitems = 0;
	unsigned int isnew;
	struct feed *f, *fcur = NULL;
	time_t parsedtime, comparetime, timenewest = 0;
	size_t size = 0;
	int r;

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */

	if(!(fcur = calloc(1, sizeof(struct feed))))
		err(1, "calloc");
	feeds = fcur;

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		/* first of feed section or new feed section. */
		if(!totalfeeds || (fcur && strcmp(fcur->name, fields[FieldFeedName]))) {
			if(!(f = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			if(totalfeeds) { /* end previous one. */
				fcur->next = f;
				fcur = f;
			} else {
				fcur = f;
				feeds = fcur; /* first item. */
			}
			if(r != -1 && parsedtime > timenewest) {
				timenewest = parsedtime;
				strlcpy(timenewestformat, fields[FieldTimeFormatted],
				        sizeof(timenewestformat));
			}
			if(r != -1 && parsedtime > fcur->timenewest) {
				fcur->timenewest = parsedtime;
				strlcpy(fcur->timenewestformat, fields[FieldTimeFormatted],
				        sizeof(fcur->timenewestformat));
			}

			/* TODO: memcpy and make fcur->name static? */
			if(!(fcur->name = strdup(fields[FieldFeedName])))
				err(1, "strdup");

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
	return 0;
}
