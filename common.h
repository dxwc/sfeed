/* Feed info. */
struct feed {
	char *name; /* feed name */
	unsigned long totalnew; /* amount of new items per feed */
	unsigned long total; /* total items */
	struct feed *next; /* linked list */
};

enum { FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle, FieldLink,
       FieldContent, FieldContentType, FieldId, FieldAuthor, FieldFeedType,
       FieldFeedName, FieldFeedUrl, FieldBaseSiteUrl, FieldLast };

char * afgets(char **p, size_t *size, FILE *fp);
void feedsfree(struct feed *f);
unsigned int parseline(char **line, size_t *size, char **fields, unsigned int maxfields, int separator, FILE *fp);
void printfeednameid(const char *s, FILE *fp);
void printhtmlencoded(const char *s, FILE *fp);
void printlink(const char *link, const char *baseurl, FILE *fp);
