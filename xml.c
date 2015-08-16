#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xml.h"

struct xml_context_fd {
	char   buf[BUFSIZ];
	int    readerrno;
	int    fd;
	size_t nread;
	size_t offset;
};

struct xml_context_buf {
	const char *buf;
	size_t      len;
	size_t      offset;
};

static int
xml_getnext_buf(XMLParser *x)
{
	struct xml_context_buf *d = (struct xml_context_buf *)x->getnext_data;

	if (d->offset >= d->len)
		return EOF;
	return (int)d->buf[d->offset++];
}

static int /* read from fd with some buffering */
xml_getnext_fd(XMLParser *x)
{
	struct xml_context_fd *d = (struct xml_context_fd *)x->getnext_data;
	ssize_t r;

	/* previous read error was set */
	if (d->readerrno)
		return EOF;

	if (d->offset >= d->nread) {
		d->offset = 0;
again:
		r = read(d->fd, d->buf, sizeof(d->buf));
		if (r == -1) {
			if (errno == EINTR)
				goto again;
			d->readerrno = errno;
			d->nread = 0;
			return EOF;
		} else if (!r) {
			return EOF;
		}
		d->nread = r;
	}
	return (int)d->buf[d->offset++];
}

static int
xml_getnext(XMLParser *x)
{
	return x->getnext(x);
}

static void
xml_parseattrs(XMLParser *x)
{
	size_t namelen = 0, valuelen;
	int c, endsep, endname = 0;

	while ((c = xml_getnext(x)) != EOF) {
		if (isspace(c)) { /* TODO: simplify endname ? */
			if (namelen)
				endname = 1;
			continue;
		}
		if (c == '?')
			; /* ignore */
		else if (c == '=') {
			x->name[namelen] = '\0';
		} else if (namelen && ((endname && isalpha(c)) || (c == '>' || c == '/'))) {
			/* attribute without value */
			x->name[namelen] = '\0';
			if (x->xmlattrstart)
				x->xmlattrstart(x, x->tag, x->taglen, x->name, namelen);
			if (x->xmlattr)
				x->xmlattr(x, x->tag, x->taglen, x->name, namelen, "", 0);
			if (x->xmlattrend)
				x->xmlattrend(x, x->tag, x->taglen, x->name, namelen);
			endname = 0;
			x->name[0] = c;
			namelen = 1;
		} else if (namelen && (c == '\'' || c == '"')) {
			/* attribute with value */
			endsep = c; /* c is end separator */
			if (x->xmlattrstart)
				x->xmlattrstart(x, x->tag, x->taglen, x->name, namelen);
			for (valuelen = 0; (c = xml_getnext(x)) != EOF;) {
				if (c == '&') { /* entities */
					x->data[valuelen] = '\0';
					/* call data function with data before entity if there is data */
					if (valuelen && x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
					x->data[0] = c;
					valuelen = 1;
					while ((c = xml_getnext(x)) != EOF) {
						if (c == endsep)
							break;
						if (valuelen < sizeof(x->data) - 1)
							x->data[valuelen++] = c;
						else {
							/* TODO: entity too long? this should be very strange. */
							x->data[valuelen] = '\0';
							if (x->xmlattr)
								x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
							valuelen = 0;
							break;
						}
						if (c == ';') {
							x->data[valuelen] = '\0';
							if (x->xmlattrentity)
								x->xmlattrentity(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
							valuelen = 0;
							break;
						}
					}
				} else if (c != endsep) {
					if (valuelen < sizeof(x->data) - 1) {
						x->data[valuelen++] = c;
					} else {
						x->data[valuelen] = '\0';
						if (x->xmlattr)
							x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
						x->data[0] = c;
						valuelen = 1;
					}
				}
				if (c == endsep) {
					x->data[valuelen] = '\0';
					if (x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
					if (x->xmlattrend)
						x->xmlattrend(x, x->tag, x->taglen, x->name, namelen);
					break;
				}
			}
			namelen = 0;
			endname = 0;
		} else if (namelen < sizeof(x->name) - 1) {
			x->name[namelen++] = c;
		}
		if (c == '>') {
			break;
		} else if (c == '/') {
			x->isshorttag = 1;
			namelen = 0;
			x->name[0] = '\0';
		}
	}
}

static void
xml_parsecomment(XMLParser *x)
{
	static const char *end = "-->";
	size_t datalen = 0, i = 0;
	char tmp[4];
	int c;

	if (x->xmlcommentstart)
		x->xmlcommentstart(x);
	while ((c = xml_getnext(x)) != EOF) {
		if (c == end[i]) {
			if (end[++i] == '\0') { /* end */
				x->data[datalen] = '\0';
				if (x->xmlcomment)
					x->xmlcomment(x, x->data, datalen);
				if (x->xmlcommentend)
					x->xmlcommentend(x);
				return;
			}
		} else if (i) {
			if (x->xmlcomment) {
				x->data[datalen] = '\0';
				if (datalen)
					x->xmlcomment(x, x->data, datalen);
				memcpy(tmp, end, i);
				tmp[i] = '\0';
				x->xmlcomment(x, tmp, i);
			}
			i = 0;
			x->data[0] = c;
			datalen = 1;
		} else if (datalen < sizeof(x->data) - 1) {
			x->data[datalen++] = c;
		} else {
			x->data[datalen] = '\0';
			if (x->xmlcomment)
				x->xmlcomment(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

static void
xml_parsecdata(XMLParser *x)
{
	static const char *end = "]]>";
	size_t datalen = 0, i = 0;
	char tmp[4];
	int c;

	if (x->xmlcdatastart)
		x->xmlcdatastart(x);
	while ((c = xml_getnext(x)) != EOF) {
		if (c == end[i]) {
			if (end[++i] == '\0') { /* end */
				x->data[datalen] = '\0';
				if (x->xmlcdata)
					x->xmlcdata(x, x->data, datalen);
				if (x->xmlcdataend)
					x->xmlcdataend(x);
				return;
			}
		} else if (i) {
			x->data[datalen] = '\0';
			if (x->xmlcdata) {
				if (datalen)
					x->xmlcdata(x, x->data, datalen);
				memcpy(tmp, end, i);
				tmp[i] = '\0';
				x->xmlcdata(x, tmp, i);
			}
			i = 0;
			x->data[0] = c;
			datalen = 1;
		} else if (datalen < sizeof(x->data) - 1) {
			x->data[datalen++] = c;
		} else {
			x->data[datalen] = '\0';
			if (x->xmlcdata)
				x->xmlcdata(x, x->data, datalen);
			x->data[0] = c;
			datalen = 1;
		}
	}
}

int
xml_codepointtoutf8(uint32_t cp, uint32_t *utf)
{
	if (cp >= 0x10000) {
		/* 4 bytes */
		*utf = 0xf0808080 | ((cp & 0xfc0000) << 6) |
		       ((cp & 0x3f000) << 4) | ((cp & 0xfc0) << 2) |
		       (cp & 0x3f);
		return 4;
	} else if (cp >= 0x00800) {
		/* 3 bytes */
		*utf = 0xe08080 |
		       ((cp & 0x3f000) << 4) | ((cp & 0xfc0) << 2) |
		       (cp & 0x3f);
		return 3;
	} else if (cp >= 0x80) {
		/* 2 bytes */
		*utf = 0xc080 |
		       ((cp & 0xfc0) << 2) | (cp & 0x3f);
		return 2;
	}
	*utf = cp & 0xff;
	return *utf ? 1 : 0; /* 1 byte */
}

ssize_t
xml_namedentitytostr(const char *e, char *buf, size_t bufsiz)
{
	const struct {
		char *entity;
		int c;
	} entities[] = {
		{ .entity = "&amp;",  .c = '&'  },
		{ .entity = "&lt;",   .c = '<'  },
		{ .entity = "&gt;",   .c = '>'  },
		{ .entity = "&apos;", .c = '\'' },
		{ .entity = "&quot;", .c = '"'  },
		{ .entity = "&AMP;",  .c = '&'  },
		{ .entity = "&LT;",   .c = '<'  },
		{ .entity = "&GT;",   .c = '>'  },
		{ .entity = "&APOS;", .c = '\'' },
		{ .entity = "&QUOT;", .c = '"'  }
	};
	size_t i;

	/* buffer is too small */
	if (bufsiz < 2)
		return -1;

	/* doesn't start with &: can't match */
	if (*e != '&')
		return 0;

	for (i = 0; i < sizeof(entities) / sizeof(*entities); i++) {
		if (!strcmp(e, entities[i].entity)) {
			buf[0] = entities[i].c;
			buf[1] = '\0';
			return 1;
		}
	}
	return 0;
}

ssize_t
xml_numericentitytostr(const char *e, char *buf, size_t bufsiz)
{
	uint32_t l = 0, cp = 0;
	size_t b, len;
	char *end;

	/* buffer is too small */
	if (bufsiz < 5)
		return -1;

	/* not a numeric entity */
	if (!(e[0] == '&' && e[1] == '#'))
		return 0;

	/* e[1] == '#', numeric / hexadecimal entity */
	e += 2; /* skip "&#" */
	errno = 0;
	/* hex (16) or decimal (10) */
	if (*e == 'x')
		l = strtoul(e + 1, &end, 16);
	else
		l = strtoul(e, &end, 10);
	/* invalid value or not a well-formed entity */
	if (errno != 0 || (*end != '\0' && *end != ';'))
		return 0;
	if (!(len = xml_codepointtoutf8(l, &cp)))
		return 0;
	/* make string */
	for (b = 0; b < len; b++)
		buf[b] = (cp >> (8 * (len - 1 - b))) & 0xff;
	buf[len] = '\0';

	return (ssize_t)len;
}

/* convert named- or numeric entity string to buffer string
 * returns byte-length of string. */
ssize_t
xml_entitytostr(const char *e, char *buf, size_t bufsiz)
{
	/* buffer is too small */
	if (bufsiz < 5)
		return -1;
	/* doesn't start with & */
	if (e[0] != '&')
		return 0;
	/* named entity */
	if (e[1] != '#')
		return xml_namedentitytostr(e, buf, bufsiz);
	else /* numeric entity */
		return xml_numericentitytostr(e, buf, bufsiz);
}

void
xml_parse(XMLParser *x)
{
	int c, ispi;
	size_t datalen, tagdatalen, taglen;

	while ((c = xml_getnext(x)) != EOF && c != '<')
		; /* skip until < */

	while (c != EOF) {
		if (c == '<') { /* parse tag */
			if ((c = xml_getnext(x)) == EOF)
				return;
			x->tag[0] = '\0';
			x->taglen = 0;
			if (c == '!') { /* cdata and comments */
				for (tagdatalen = 0; (c = xml_getnext(x)) != EOF;) {
					if (tagdatalen <= sizeof("[CDATA[") - 1) /* if (d < sizeof(x->data)) */
						x->data[tagdatalen++] = c; /* TODO: prevent overflow */
					if (c == '>')
						break;
					else if (c == '-' && tagdatalen == sizeof("--") - 1 &&
							(x->data[0] == '-')) {
						xml_parsecomment(x);
						break;
					} else if (c == '[') {
						if (tagdatalen == sizeof("[CDATA[") - 1 &&
						    !strncmp(x->data, "[CDATA[", tagdatalen)) {
							xml_parsecdata(x);
							break;
						}
					}
				}
			} else {
				/* normal tag (open, short open, close), processing instruction. */
				if (isspace(c))
					while ((c = xml_getnext(x)) != EOF && isspace(c))
						;
				if (c == EOF)
					return;
				x->tag[0] = c;
				ispi = (c == '?') ? 1 : 0;
				x->isshorttag = ispi;
				taglen = 1;
				while ((c = xml_getnext(x)) != EOF) {
					if (c == '/') /* TODO: simplify short tag? */
						x->isshorttag = 1; /* short tag */
					else if (c == '>' || isspace(c)) {
						x->tag[taglen] = '\0';
						if (x->tag[0] == '/') { /* end tag, starts with </ */
							x->taglen = --taglen; /* len -1 because of / */
							if (taglen && x->xmltagend)
								x->xmltagend(x, &(x->tag)[1], x->taglen, 0);
						} else {
							x->taglen = taglen;
							/* start tag */
							if (x->xmltagstart)
								x->xmltagstart(x, x->tag, x->taglen);
							if (isspace(c))
								xml_parseattrs(x);
							if (x->xmltagstartparsed)
								x->xmltagstartparsed(x, x->tag, x->taglen, x->isshorttag);
						}
						/* call tagend for shortform or processing instruction */
						if ((x->isshorttag || ispi) && x->xmltagend)
							x->xmltagend(x, x->tag, x->taglen, 1);
						break;
					} else if (taglen < sizeof(x->tag) - 1)
						x->tag[taglen++] = c;
				}
			}
		} else {
			/* parse tag data */
			datalen = 0;
			if (x->xmldatastart)
				x->xmldatastart(x);
			while ((c = xml_getnext(x)) != EOF) {
				if (c == '&') {
					if (datalen) {
						x->data[datalen] = '\0';
						if (x->xmldata)
							x->xmldata(x, x->data, datalen);
					}
					x->data[0] = c;
					datalen = 1;
					while ((c = xml_getnext(x)) != EOF) {
						if (c == '<')
							break;
						if (datalen < sizeof(x->data) - 1)
							x->data[datalen++] = c;
						if (isspace(c))
							break;
						else if (c == ';') {
							x->data[datalen] = '\0';
							if (x->xmldataentity)
								x->xmldataentity(x, x->data, datalen);
							datalen = 0;
							break;
						}
					}
				} else if (c != '<') {
					if (datalen < sizeof(x->data) - 1) {
						x->data[datalen++] = c;
					} else {
						x->data[datalen] = '\0';
						if (x->xmldata)
							x->xmldata(x, x->data, datalen);
						x->data[0] = c;
						datalen = 1;
					}
				}
				if (c == '<') {
					x->data[datalen] = '\0';
					if (x->xmldata && datalen)
						x->xmldata(x, x->data, datalen);
					if (x->xmldataend)
						x->xmldataend(x);
					break;
				}
			}
		}
	}
}

void
xml_parse_buf(XMLParser *x, const char *buf, size_t len)
{
	struct xml_context_buf ctx = { .buf = buf, .len = len };

	x->getnext = xml_getnext_buf;
	x->getnext_data = (void *)&ctx;
	xml_parse(x);
}

void
xml_parse_fd(XMLParser *x, int fd)
{
	struct xml_context_fd ctx;

	memset(&ctx, 0, sizeof(ctx));
	ctx.fd = fd;

	x->getnext = xml_getnext_fd;
	x->getnext_data = (void *)&ctx;
	xml_parse(x);
}
