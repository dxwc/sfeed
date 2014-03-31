#include <time.h>

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

#undef strlcpy
size_t strlcpy(char *, const char *, size_t);

char * afgets(char **p, size_t *size, FILE *fp);
void feedsfree(struct feed *f);
unsigned int parseline(char **line, size_t *size, char **fields,
                       unsigned int maxfields, int separator, FILE *fp);
void printfeednameid(const char *s, FILE *fp);
void printhtmlencoded(const char *s, FILE *fp);
void printlink(const char *link, const char *baseurl, FILE *fp);
