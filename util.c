#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"

static void
encodehex(unsigned char c, char *s)
{
	static const char *table = "0123456789ABCDEF";

	s[0] = table[((c - (c % 16)) / 16) % 16];
	s[1] = table[c % 16];
}

int
parseuri(const char *s, struct uri *u, int rel)
{
	const char *p = s;
	size_t i;

	u->proto[0] = u->host[0] = u->path[0] = '\0';
	if (!*s)
		return 0;

	/* prefix is "//", don't read protocol, skip to domain parsing */
	if (!strncmp(p, "//", 2)) {
		p += 2; /* skip "//" */
	} else {
		/* protocol part */
		for (p = s; *p && (isalpha((int)*p) || isdigit((int)*p) ||
			       *p == '+' || *p == '-' || *p == '.'); p++)
			;
		if (!strncmp(p, "://", 3)) {
			if (p - s + 1 >= (ssize_t)sizeof(u->proto))
				return -1; /* protocol too long */
			memcpy(u->proto, s, p - s);
			u->proto[p - s] = '\0';
			p += 3; /* skip "://" */
		} else {
			p = s; /* no protocol format, set to start */
			/* relative url: read rest as path, else as domain */
			if (rel)
				goto readpath;
		}
	}
	/* domain / host part, skip until "/" or end. */
	i = strcspn(p, "/");
	if (i + 1 >= sizeof(u->host))
		return -1; /* host too long */
	memcpy(u->host, p, i);
	u->host[i] = '\0';
	p = &p[i];

readpath:
	if (u->host[0]) {
		p = &p[strspn(p, "/")];
		strlcpy(u->path, "/", sizeof(u->path));
	} else {
		/* having no host is an error in this case */
		if (!rel)
			return -1;
	}
	/* treat truncation as an error */
	return strlcat(u->path, p, sizeof(u->path)) >= sizeof(u->path) ? -1 : 0;
}

/* Get absolute uri; if `link` is relative use `base` to make it absolute.
 * the returned string in `buf` is uri encoded, see: encodeuri(). */
int
absuri(const char *link, const char *base, char *buf, size_t bufsiz)
{
	struct uri ulink, ubase;
	char tmp[4096] = "", *p;
	int r = -1, c;
	size_t i;

	buf[0] = '\0';
	if (parseuri(base, &ubase, 0) == -1 ||
	    parseuri(link, &ulink, 1) == -1)
		return -1;

	if (!ulink.host[0] && !ubase.host[0])
		return -1;

	r = snprintf(tmp, sizeof(tmp), "%s://%s",
		ulink.proto[0] ?
			ulink.proto :
			(ubase.proto[0] ? ubase.proto : "http"),
		!strncmp(link, "//", 2) ?
			ulink.host :
			(ulink.host[0] ? ulink.host : ubase.host));
	if (r == -1 || (size_t)r >= sizeof(tmp))
		return -1;

	/* relative to root */
	if (!ulink.host[0] && ulink.path[0] != '/') {
		/* relative to base url path */
		if (ulink.path[0]) {
			if ((p = strrchr(ubase.path, '/'))) {
				/* temporary null-terminate */
				c = *(++p);
				*p = '\0';
				i = strlcat(tmp, ubase.path, sizeof(tmp));
				*p = c; /* restore */
				if (i >= sizeof(tmp))
					return -1;
			}
		} else {
			if (strlcat(tmp, ubase.path, sizeof(tmp)) >= sizeof(tmp))
				return -1;
		}
	}
	if (strlcat(tmp, ulink.path, sizeof(tmp)) >= sizeof(tmp))
		return -1;

	return encodeuri(tmp, buf, bufsiz);
}

int
encodeuri(const char *s, char *buf, size_t bufsiz)
{
	size_t i, b;

	if (!bufsiz)
		return -1;
	for (i = 0, b = 0; s[i]; i++) {
		if ((int)s[i] == ' ' ||
		    (unsigned char)s[i] > 127 ||
		    iscntrl((int)s[i])) {
			if (b + 3 >= bufsiz)
				return -1;
			buf[b++] = '%';
			encodehex(s[i], &buf[b]);
			b += 2;
		} else {
			if (b >= bufsiz)
				return -1;
			buf[b++] = s[i];
		}
	}
	if (b >= bufsiz)
		return -1;
	buf[b] = '\0';

	return 0;
}

/* Read a field-separated line from 'fp',
 * separated by a character 'separator',
 * 'fields' is a list of pointers with a size of FieldLast (must be >0).
 * 'line' buffer is allocated using malloc, 'size' will contain the allocated
 * buffer size.
 * returns: amount of fields read (>0) or -1 on error. */
ssize_t
parseline(char **line, size_t *size, char *fields[FieldLast], FILE *fp)
{
	char *prev, *s;
	size_t i;

	if (getline(line, size, fp) <= 0)
		return -1;

	for (prev = *line, i = 0;
	    (s = strchr(prev, '\t')) && i < FieldLast - 1;
	    i++) {
		*s = '\0';
		fields[i] = prev;
		prev = s + 1;
	}
	fields[i++] = prev;
	/* make non-parsed fields empty. */
	for (; i < FieldLast; i++)
		fields[i] = "";

	return (ssize_t)i;
}

/* Parse time to time_t, assumes time_t is signed. */
int
strtotime(const char *s, time_t *t)
{
	long l;
	char *e;

	errno = 0;
	l = strtol(s, &e, 10);
	if (*s == '\0' || *e != '\0')
		return -1;
	if (t)
		*t = (time_t)l;

	return 0;
}

/* Escape characters below as HTML 2.0 / XML 1.0. */
void
xmlencode(const char *s, FILE *fp)
{
	for (; *s; s++) {
		switch(*s) {
		case '<':  fputs("&lt;",   fp); break;
		case '>':  fputs("&gt;",   fp); break;
		case '\'': fputs("&apos;", fp); break;
		case '&':  fputs("&amp;",  fp); break;
		case '"':  fputs("&quot;", fp); break;
		default:   fputc(*s, fp);
		}
	}
}

/* Some implementations of basename(3) return a pointer to a static
 * internal buffer (OpenBSD). Others modify the contents of `path` (POSIX).
 * This is a wrapper function that is compatible with both versions.
 * The program will error out if basename(3) failed, this can only happen
 * with the OpenBSD version. */
char *
xbasename(const char *path)
{
	char *p, *b;

	if (!(p = strdup(path)))
		err(1, "strdup");
	if (!(b = basename(p)))
		err(1, "basename");
	if (!(b = strdup(b)))
		err(1, "strdup");
	free(p);
	return b;
}
