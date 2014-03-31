#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "util.h"

void
printutf8padded(const char *s, size_t len, FILE *fp, int pad) {
	size_t n = 0, i;

	for(i = 0; s[i] && n < len; i++) {
		if((s[i] & 0xc0) != 0x80) /* start of character */
			n++;
		putc(s[i], fp);
	}
	for(; n < len; n++)
		putc(pad, fp);
}

int
main(void) {
	char *line = NULL, *fields[FieldLast];
	time_t parsedtime, comparetime;
	unsigned int isnew;
	size_t size = 0;

	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	while(parseline(&line, &size, fields, FieldLast, '\t', stdin) > 0) {
		isnew = (parsedtime >= comparetime);
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		printf(" %c  ", isnew ? 'N' : ' ');
		if(fields[FieldFeedName][0] != '\0')
			printf("%-15.15s  ", fields[FieldFeedName]);
		printf("%-30.30s", fields[FieldTimeFormatted]);
		fputs("  ", stdout);
		printutf8padded(fields[FieldTitle], 70, stdout, ' ');
		fputs("  ", stdout);
		if(fields[FieldBaseSiteUrl][0] != '\0')
			printlink(fields[FieldLink], fields[FieldBaseSiteUrl], stdout);
		else
			printlink(fields[FieldLink], fields[FieldFeedUrl], stdout);
		putchar('\n');
	}
	free(line);
	return EXIT_SUCCESS;
}
