#ifdef COMPAT
#include "compat.h"
#endif

#include <time.h>

#define ISUTF8(c) (((c) & 0xc0) != 0x80)

/* feed info */
struct feed {
	char *name; /* feed name */
	unsigned long totalnew; /* amount of new items per feed */
	unsigned long total; /* total items */
	time_t timenewest;
	char timenewestformat[64];
	struct feed *next; /* linked list */
};

enum { FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle, FieldLink,
       FieldContent, FieldContentType, FieldId, FieldAuthor, FieldFeedType,
       FieldFeedName, FieldFeedUrl, FieldBaseSiteUrl, FieldLast };

void feedsfree(struct feed *f);
unsigned int parseline(char **line, size_t *size, char **fields,
                       unsigned int maxfields, int separator, FILE *fp);
void printfeednameid(const char *s, FILE *fp);
void printhtmlencoded(const char *s, FILE *fp);
void printlink(const char *link, const char *baseurl, FILE *fp);
void printutf8pad(FILE *fp, const char *s, size_t len, int pad);
