#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "common.c"

void
printutf8padded(const char *s, size_t len) {
	unsigned int n = 0, i = 0;

	for(; s[i] && n < len; i++) {
		if((s[i] & 0xc0) != 0x80) /* start of character */
			n++;
		putchar(s[i]);
	}
	for(; n < len; n++)
		putchar(' ');
}

int
main(void) {
	char *line = NULL, *fields[FieldLast];
	time_t parsedtime, comparetime;
	size_t size = 0;

	tzset();
	comparetime = time(NULL) - (3600 * 24); /* 1 day is old news */
	while(parseline(&line, &size, fields, FieldLast, stdin, FieldSeparator) > 0) {
		parsedtime = (time_t)strtol(fields[FieldUnixTimestamp], NULL, 10);
		printf(" %c  ", (parsedtime >= comparetime) ? 'N' : ' ');
		if(fields[FieldFeedName][0] != '\0')
			printf("%-15.15s  ", fields[FieldFeedName]);
		fputs(fields[FieldTimeFormatted], stdout);
		fputs("  ", stdout);
		printutf8padded(fields[FieldTitle], 70);
		fputs("  ", stdout);
		if(fields[FieldBaseSiteUrl][0] != '\0')
			printlink(fields[FieldLink], fields[FieldBaseSiteUrl]);
		else
			printlink(fields[FieldLink], fields[FieldFeedUrl]);
		putchar('\n');
	}
	free(line);
	return EXIT_SUCCESS;
}
