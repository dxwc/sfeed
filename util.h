#include <stdint.h>
#include <time.h>
#ifdef USE_PLEDGE
#include <unistd.h>
#else
#define pledge(p1,p2) 0
#endif

#undef strlcat
size_t strlcat(char *, const char *, size_t);
#undef strlcpy
size_t strlcpy(char *, const char *, size_t);

#define ISUTF8(c) (((c) & 0xc0) != 0x80)

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
	FieldUnixTimestamp = 0, FieldTitle, FieldLink,
	FieldContent, FieldContentType, FieldId, FieldAuthor, FieldLast
};

int     absuri(char *, size_t, const char *, const char *);
size_t  parseline(char *, char *[FieldLast]);
int     parseuri(const char *, struct uri *, int);
void    printutf8pad(FILE *, const char *, size_t, int);
int     strtotime(const char *, time_t *);
void    xmlencode(const char *, FILE *);
