#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <wchar.h>

#include "util.h"

void
printurlencoded(const char *s, size_t len, FILE *fp)
{
	size_t i;

	for(i = 0; i < len && s[i]; i++) {
		if((int)s[i] == ' ')
			fputs("%20", fp);
		else if((unsigned char)s[i] > 127 || iscntrl((int)s[i]))
			fprintf(fp, "%%%02X", (unsigned char)s[i]);
		else
			fputc(s[i], fp);
	}
}

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
			printurlencoded(baseurl, ebaseproto - baseurl, fp);
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
				printurlencoded(ebaseproto, ebasedomain - ebaseproto, fp);
			else
				printurlencoded(ebaseproto, strlen(ebaseproto), fp);
		} else if((ebasedomain = strrchr(ebaseproto, '/'))) {
			/* relative to baseurl and baseurl path. */
			printurlencoded(ebaseproto, ebasedomain - ebaseproto + 1, fp);
		} else {
			printurlencoded(ebaseproto, strlen(ebaseproto), fp);
			if(*baseurl && *link)
				fputc('/', fp);
		}
	}
	printurlencoded(link, strlen(link), fp);
}

/* read a field-separated line from 'fp',
 * separated by a character 'separator',
 * 'fields' is a list of pointer with a maximum size of 'maxfields'.
 * 'maxfields' must be > 0.
 * 'line' buffer is allocated using malloc, 'size' will contain the
 * allocated buffer size.
 * returns: amount of fields read or -1 on error. */
int
parseline(char **line, size_t *size, char **fields,
          unsigned int maxfields, int separator, FILE *fp)
{
	char *prev, *s;
	unsigned int i;

	if(getline(line, size, fp) <= 0)
		return -1;

	for(prev = *line, i = 0;
	    (s = strchr(prev, separator)) && i < maxfields - 1;
	    i++) {
		*s = '\0';
		fields[i] = prev;
		prev = s + 1;
	}
	fields[i++] = prev;
	/* make non-parsed fields empty. */
	for(; i < maxfields; i++)
		fields[i] = "";

	return (int)i;
}

char *
trimend(const char *s)
{
	size_t len = strlen(s);

	for(; len > 0 && isspace((int)s[len - 1]); len--)
		;
	return (char*)&s[len];
}

char *
trimstart(const char *s)
{
	for(; *s && isspace((int)*s); s++)
		;
	return (char *)s;
}

void
printxmlencoded(const char *s, FILE *fp)
{
	for(; *s; s++) {
		switch(*s) {
		case '<':  fputs("&lt;", fp);   break;
		case '>':  fputs("&gt;", fp);   break;
		case '\'': fputs("&apos;", fp); break;
		case '&':  fputs("&amp;", fp);  break;
		case '"':  fputs("&quot;", fp); break;
		default:
			fputc((int)*s, fp);
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

int
strtotime(const char *s, time_t *t)
{
	long l;

	errno = 0;
	l = strtol(s, NULL, 10);
	if(errno != 0)
		return -1;
	*t = (time_t)l;

	return 0;
}

/* print text, ignore tabs, newline and carriage return etc
 * print some HTML 2.0 / XML 1.0 as normal text */
void
printcontent(const char *s, FILE *fp)
{
	const char *p;

	for(p = s; *p; p++) {
		if(*p == '\\') {
			p++;
			if(*p == '\\')
				fputc('\\', fp);
			else if(*p == 't')
				fputc('\t', fp);
			else if(*p == 'n')
				fputc('\n', fp);
			else
				fputc(*p, fp); /* unknown */
		} else {
			fputc(*p, fp);
		}
	}
}

/* Some implementations of basename(3) return a pointer to a static
 * internal buffer (OpenBSD). Others modify the contents of `path` (POSIX).
 * This is a wrapper function that is compatible with both versions.
 * The program will error out if basename(3) failed, this can only happen
 * with the OpenBSD version.
 */
char *
xbasename(const char *path)
{
	char *p, *b;

	if(!(p = strdup(path)))
		err(1, "strdup");
	if(!(b = basename(p)))
		err(1, "basename");
	if(!(b = strdup(b)))
		err(1, "strdup");
	free(p);
	return b;
}
