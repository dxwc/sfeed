#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "util.h"

#ifndef USE_PLEDGE
int
pledge(const char *promises, const char *paths[])
{
	return 0;
}
#endif

int
parseuri(const char *s, struct uri *u, int rel)
{
	const char *p = s, *b;
	char *endptr = NULL;
	size_t i;
	unsigned long l;

	u->proto[0] = u->host[0] = u->path[0] = u->port[0] = '\0';
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
			if (p - s >= (ssize_t)sizeof(u->proto))
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
	/* IPv6 address */
	if (*p == '[') {
		/* bracket not found or host too long */
		if (!(b = strchr(p, ']')) || b - p >= (ssize_t)sizeof(u->host))
			return -1;
		memcpy(u->host, p + 1, b - p - 1);
		u->host[b - p] = '\0';
		p = b + 1;
	} else {
		/* domain / host part, skip until port, path or end. */
		if ((i = strcspn(p, ":/")) >= sizeof(u->host))
			return -1; /* host too long */
		memcpy(u->host, p, i);
		u->host[i] = '\0';
		p = &p[i];
	}
	/* port */
	if (*p == ':') {
		if ((i = strcspn(++p, "/")) >= sizeof(u->port))
			return -1; /* port too long */
		memcpy(u->port, p, i);
		u->port[i] = '\0';
		/* check for valid port */
		errno = 0;
		l = strtoul(u->port, &endptr, 10);
		if (errno || *endptr != '\0' || !l || l > 65535)
			return -1;
		p = &p[i];
	}
readpath:
	if (u->host[0]) {
		p = &p[strspn(p, "/")];
		strlcpy(u->path, "/", sizeof(u->path));
	} else {
		/* absolute uri must have a host specified */
		if (!rel)
			return -1;
	}
	/* treat truncation as an error */
	if (strlcat(u->path, p, sizeof(u->path)) >= sizeof(u->path))
		return -1;
	return 0;
}

/* Get absolute uri; if `link` is relative use `base` to make it absolute.
 * the returned string in `buf` is uri encoded, see: encodeuri(). */
int
absuri(const char *link, const char *base, char *buf, size_t bufsiz)
{
	struct uri ulink, ubase;
	char tmp[4096], *host, *p, *port;
	int r = -1, c;
	size_t i;

	buf[0] = '\0';
	if (parseuri(base, &ubase, 0) == -1 ||
	    parseuri(link, &ulink, 1) == -1 ||
	    (!ulink.host[0] && !ubase.host[0]))
		return -1;

	if (!strncmp(link, "//", 2)) {
		host = ulink.host;
		port = ulink.port;
	} else {
		host = ulink.host[0] ? ulink.host : ubase.host;
		port = ulink.port[0] ? ulink.port : ubase.port;
	}
	r = snprintf(tmp, sizeof(tmp), "%s://%s%s%s",
		ulink.proto[0] ?
			ulink.proto :
			(ubase.proto[0] ? ubase.proto : "http"),
		host,
		port[0] ? ":" : "",
		port);
	if (r == -1 || (size_t)r >= sizeof(tmp))
		return -1; /* error or truncation */

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
		} else if (strlcat(tmp, ubase.path, sizeof(tmp)) >=
		           sizeof(tmp)) {
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
	static const char *table = "0123456789ABCDEF";
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
			buf[b++] = table[((uint8_t)s[i] >> 4) & 15];
			buf[b++] = table[(uint8_t)s[i] & 15];
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
size_t
parseline(char *line, char *fields[FieldLast])
{
	char *prev, *s;
	size_t i;

	for (prev = line, i = 0;
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

	return i;
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

/* print `len' columns of characters. If string is shorter pad the rest
 * with characters `pad`. */
void
printutf8pad(FILE *fp, const char *s, size_t len, int pad)
{
	wchar_t w;
	size_t n = 0, i;
	int r;

	for (i = 0; *s && n < len; i++, s++) {
		if (ISUTF8(*s)) {
			if ((r = mbtowc(&w, s, 4)) == -1)
				break;
			if ((r = wcwidth(w)) == -1)
				r = 1;
			n += (size_t)r;
		}
		putc(*s, fp);
	}
	for (; n < len; n++)
		putc(pad, fp);
}

uint32_t
murmur3_32(const char *key, uint32_t len, uint32_t seed)
{
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m = 5;
	static const uint32_t n = 0xe6546b64;
	uint32_t hash = seed;
	const int nblocks = len / 4;
	const uint32_t *blocks = (const uint32_t *) key;
	int i;
	uint32_t k, k1;
	const uint8_t *tail;

	for (i = 0; i < nblocks; i++) {
		k = blocks[i];
		k *= c1;
		k = ROT32(k, r1);
		k *= c2;

		hash ^= k;
		hash = ROT32(hash, r2) * m + n;
	}
	tail = (const uint8_t *) (key + nblocks * 4);

	k1 = 0;
	switch (len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];

		k1 *= c1;
		k1 = ROT32(k1, r1);
		k1 *= c2;
		hash ^= k1;
	}

	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);

	return hash;
}
