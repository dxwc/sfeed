#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>

int
xstrcasecmp(const char *_l, const char *_r) {
	const unsigned char *l = (void *)_l, *r = (void *)_r;
	for(; *l && *r && (*l == *r || tolower(*l) == tolower(*r)); l++, r++);
	return tolower(*l) - tolower(*r);
}

int
xstrncasecmp(const char *_l, const char *_r, size_t n) {
	const unsigned char *l=(void *)_l, *r=(void *)_r;
	if(!n--)
		return 0;
	for(; *l && *r && n && (*l == *r || tolower(*l) == tolower(*r)); l++, r++, n--);
	return tolower(*l) - tolower(*r);
}

void *
xstrdup(const char *s) {
	size_t len = strlen(s) + 1;
	void *p = malloc(len);
	if(p)
		memcpy(p, s, len);
	return p;
}

int
xmkdir(const char *path, mode_t mode) {
/* TODO: fix for mingw */
#if MINGW
	return mkdir(path);
#else
	return mkdir(path, mode);
#endif
}
