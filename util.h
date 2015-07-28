#ifdef COMPAT
#include "compat.h"
#endif

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
	char host[255];
	char path[2048];
};

enum { FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle, FieldLink,
       FieldContent, FieldContentType, FieldId, FieldAuthor, FieldFeedType,
       FieldLast };

int    absuri(const char *, const char *, char *, size_t);
int    encodeuri(const char *, char *, size_t);
int    parseline(char **, size_t *, char **, unsigned int, int, FILE *);
int    parseuri(const char *, struct uri *, int);
void   printcontent(const char *, FILE *);
void   printxmlencoded(const char *, FILE *);
void   printutf8pad(FILE *, const char *, size_t, int);
int    strtotime(const char *, time_t *);
char * trimstart(const char *);
char * trimend(const char *);
char * xbasename(const char *);


