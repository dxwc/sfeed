#include <stdio.h>
#include <time.h>

#ifdef COMPAT
#include "compat.h"
#endif

#include "queue.h"

#define ISUTF8(c) (((c) & 0xc0) != 0x80)

/* feed info */
struct feed {
	char *         name;     /* feed name */
	unsigned long  totalnew; /* amount of new items per feed */
	unsigned long  total;    /* total items */
	time_t         timenewest;
	char           timenewestformat[64];
	SLIST_ENTRY(feed) entry;
};

enum { FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle, FieldLink,
       FieldContent, FieldContentType, FieldId, FieldAuthor, FieldFeedType,
       FieldFeedName, FieldFeedUrl, FieldBaseSiteUrl, FieldLast };

unsigned int parseline(char **, size_t *, char **, unsigned int, int, FILE *);
void         printfeednameid(const char *, FILE *);
void         printhtmlencoded(const char *, FILE *);
void         printlink(const char *, const char *, FILE *);
void         printutf8pad(FILE *, const char *, size_t, int);
int          strtotime(const char *, time_t *);
const char * trimstart(const char *);
const char * trimend(const char *);
