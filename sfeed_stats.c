#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "queue.h"
#include "util.h"

static SLIST_HEAD(fhead, feed) fhead = SLIST_HEAD_INITIALIZER(fhead);
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

	/* 1 day is old news */
	comparetime = time(NULL) - 86400;

	if(!(fcur = calloc(1, sizeof(struct feed))))
		err(1, "calloc");
	SLIST_INSERT_HEAD(&fhead, fcur, entry);

	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		isnew = (r != -1 && parsedtime >= comparetime) ? 1 : 0;
		/* first of feed section or new feed section. */
		if(!totalfeeds || strcmp(fcur->name, fields[FieldFeedName])) {
			if(!(f = calloc(1, sizeof(struct feed))))
				err(1, "calloc");
			if(!(f->name = strdup(fields[FieldFeedName])))
				err(1, "strdup");

			SLIST_INSERT_AFTER(fcur, f, entry);
			fcur = f;

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
			totalfeeds++;
		}
		totalnew += isnew;
		fcur->totalnew += isnew;
		fcur->total++;
		totalitems++;
	}
	SLIST_FOREACH(f, &fhead, entry) {
		if(!f->name || f->name[0] == '\0')
			continue;
		fprintf(stdout, "%c %-20.20s [%4lu/%-4lu]",
		        f->totalnew > 0 ? 'N' : ' ',
		        f->name, f->totalnew, f->total);
		if(f->timenewestformat && f->timenewestformat[0])
			fprintf(stdout, " %s", f->timenewestformat);
		putchar('\n');
	}
	printf("  ================================\n");
	printf("%c %-20.20s [%4lu/%-4lu] %s\n", totalnew > 0 ? 'N' : ' ', "Total:",
	       totalnew, totalitems, timenewestformat);
	return 0;
}
