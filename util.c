#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <wchar.h>

#include "util.h"

/* print link; if link is relative use baseurl to make it absolute */
void
printlink(const char *link, const char *baseurl, FILE *fp)
{
	const char *ebaseproto, *ebasedomain, *p;
	int isrelative;

	/* protocol part */
	for(p = link; *p && (isalpha((int)*p) || isdigit((int)*p) ||
		                *p == '+' || *p == '-' || *p == '.'); p++);
	/* relative link (baseurl is used). */
	isrelative = strncmp(p, "://", strlen("://"));
	if(isrelative) {
		if((ebaseproto = strstr(baseurl, "://"))) {
			ebaseproto += strlen("://");
			fwrite(baseurl, 1, ebaseproto - baseurl, fp);
		} else {
			ebaseproto = baseurl;
			if(*baseurl || (link[0] == '/' && link[1] == '/'))
				fputs("http://", fp);
		}
		if(link[0] == '/') { /* relative to baseurl domain (not path).  */
			if(link[1] == '/') /* absolute url but with protocol from baseurl. */
				link += 2;
			else if((ebasedomain = strchr(ebaseproto, '/')))
				/* relative to baseurl and baseurl path. */
				fwrite(ebaseproto, 1, ebasedomain - ebaseproto, fp);
			else
				fputs(ebaseproto, stdout);
		} else if((ebasedomain = strrchr(ebaseproto, '/'))) {
			/* relative to baseurl and baseurl path. */
			fwrite(ebaseproto, 1, ebasedomain - ebaseproto + 1, fp);
		} else {
			fputs(ebaseproto, fp);
			if(*baseurl && *link)
				fputc('/', fp);
		}
	}
	fputs(link, fp);
}

/* read a field-separated line from 'fp',
 * separated by a character 'separator',
 * 'fields' is a list of pointer with a maximum size of 'maxfields'.
 * 'line' buffer is allocated using malloc, 'size' will contain the
 * allocated buffer size.
 * returns: amount of fields read. */
unsigned int
parseline(char **line, size_t *size, char **fields,
          unsigned int maxfields, int separator, FILE *fp)
{
	unsigned int i = 0;
	char *prev, *s;

	if(getline(line, size, fp) > 0) {
		for(prev = *line; (s = strchr(prev, separator)) && i <= maxfields; i++) {
			*s = '\0';
			fields[i] = prev;
			prev = s + 1;
		}
		fields[i] = prev;
		for(i++; i < maxfields; i++) /* make non-parsed fields empty. */
			fields[i] = "";
	}
	return i;
}

/* print feed name for id; spaces and tabs in string as "-"
 * (spaces in anchors are not valid). */
void
printfeednameid(const char *s, FILE *fp)
{
	for(; *s; s++)
		fputc(isspace((int)*s) ? '-' : tolower((int)*s), fp);
}

void
printhtmlencoded(const char *s, FILE *fp) {
	for(; *s; s++) {
		switch(*s) {
		case '<': fputs("&lt;", fp); break;
		case '>': fputs("&gt;", fp); break;
/*		case '&': fputs("&amp;", fp); break;*/
		default:
			fputc(*s, fp);
		}
	}
}

void
printutf8pad(FILE *fp, const char *s, size_t len, int pad)
{
	wchar_t w;
    size_t n = 0, i;
	int r;

    for(i = 0; *s && n < len; i++, s++) {
		if(ISUTF8(*s)) {
			if((r = mbtowc(&w, s, 4)) == -1)
				break;
			if((r = wcwidth(w)) == -1)
				r = 1;
			n += (size_t)r;
		}
        putc(*s, fp);
    }
    for(; n < len; n++)
        putc(pad, fp);
}
