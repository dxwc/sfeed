#if 1
#include <strings.h>
#include <string.h>
#define xstrcasecmp strcasecmp
#define xstrncasecmp strncasecmp
#else
int xstrcasecmp(const char *s1, const char *s2);
int xstrncasecmp(const char *s1, const char *s2, size_t len);
#endif

/* non-ansi */
void * xstrdup(const char *s);

/* for mingw */
#include <sys/stat.h>
#include <sys/types.h>
int xmkdir(const char *path, mode_t mode);
