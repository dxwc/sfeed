#undef strlcat
size_t strlcat(char *, const char *, size_t);
#undef strlcpy
size_t strlcpy(char *, const char *, size_t);

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
	char host[256];
	char path[2048];
	char port[6];     /* numeric port */
};

enum {
	FieldUnixTimestamp = 0, FieldTimeFormatted, FieldTitle,
	FieldLink, FieldContent, FieldContentType, FieldId, FieldAuthor,
	FieldFeedType, FieldLast
};

int     absuri(const char *, const char *, char *, size_t);
int     encodeuri(const char *, char *, size_t);
size_t  parseline(char *, char *[FieldLast]);
int     parseuri(const char *, struct uri *, int);
void    printutf8pad(FILE *, const char *, size_t, int);
int     strtotime(const char *, time_t *);
char *  xbasename(const char *);
void    xmlencode(const char *, FILE *);

#ifdef USE_PLEDGE
#include <unistd.h>
#else
int     pledge(const char *, const char *[]);
#endif

#define ROT32(x, y) ((x << y) | (x >> (32 - y)))
uint32_t murmur3_32(const char *, uint32_t, uint32_t);
