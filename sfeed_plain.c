#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

int
main(void)
{
	char *line = NULL, *fields[FieldLast];
	time_t parsedtime, comparetime;
	size_t size = 0;
	int r;

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		r = strtotime(fields[FieldUnixTimestamp], &parsedtime);
		if(r != -1 && parsedtime >= comparetime)
			fputs(" N  ", stdout);
		else
			fputs("    ", stdout);
		if(fields[FieldFeedName][0] != '\0')
			printf("%-15.15s  ", fields[FieldFeedName]);
		printf("%-30.30s  ", fields[FieldTimeFormatted]);
		printutf8pad(stdout, fields[FieldTitle], 70, ' ');
		fputs("  ", stdout);
		if(fields[FieldBaseSiteUrl][0] != '\0')
			printlink(fields[FieldLink], fields[FieldBaseSiteUrl], stdout);
		else
			printlink(fields[FieldLink], fields[FieldFeedUrl], stdout);
		putchar('\n');
	}
	return 0;
}
