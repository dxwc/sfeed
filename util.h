#include "compat.h"

#define ISUTF8(c) (((c) & 0xc0) != 0x80)
#define LEN(x) (sizeof (x) / sizeof *(x))

/* feed info */
struct feed {
	char *        name;     /* feed name */
	unsigned long totalnew; /* amount of new items per feed */
	unsigned long total;    /* total items */
	time_t        timenewest;
	char          timenewestformat[64];
};

/* uri */
struct uri {
	char proto[48];
	char host[HOST_NAME_MAX];
	char path[2048];
	char port[6];     /* numeric port */
};

enum { FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle, FieldLink,
       FieldContent, FieldContentType, FieldId, FieldAuthor, FieldFeedType,
       FieldLast };

int     absuri(const char *, const char *, char *, size_t);
int     encodeuri(const char *, char *, size_t);
ssize_t parseline(char **, size_t *, char *[FieldLast], FILE *);
int     parseuri(const char *, struct uri *, int);
int     strtotime(const char *, time_t *);
char *  xbasename(const char *);
void    xmlencode(const char *, FILE *);
