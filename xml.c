#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xml.h"

static void
xml_parseattrs(XMLParser *x)
{
	size_t namelen = 0, valuelen;
	int c, endsep, endname = 0;

	while ((c = x->getnext()) != EOF) {
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
			for (valuelen = 0; (c = x->getnext()) != EOF;) {
				if (c == '&') { /* entities */
					x->data[valuelen] = '\0';
					/* call data function with data before entity if there is data */
					if (valuelen && x->xmlattr)
						x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
					x->data[0] = c;
					valuelen = 1;
					while ((c = x->getnext()) != EOF) {
						if (c == endsep)
							break;
						if (valuelen < sizeof(x->data) - 1)
							x->data[valuelen++] = c;
						else {
							/* entity too long for buffer, handle as normal data */
							x->data[valuelen] = '\0';
							if (x->xmlattr)
								x->xmlattr(x, x->tag, x->taglen, x->name, namelen, x->data, valuelen);
							x->data[0] = c;
							valuelen = 1;
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
			namelen = endname = 0;
		} else if (namelen < sizeof(x->name) - 1) {
			x->name[namelen++] = c;
		}
		if (c == '>') {
			break;
		} else if (c == '/') {
			x->isshorttag = 1;
			x->name[0] = '\0';
			namelen = 0;
		}
	}
}

static void
xml_parsecomment(XMLParser *x)
{
	size_t datalen = 0, i = 0;
	int c;

	if (x->xmlcommentstart)
		x->xmlcommentstart(x);
	while ((c = x->getnext()) != EOF) {
		if (c == '-' || c == '>') {
			if (x->xmlcomment) {
				x->data[datalen] = '\0';
				x->xmlcomment(x, x->data, datalen);
				datalen = 0;
			}
		}

		if (c == '-') {
			if (++i > 2) {
				if (x->xmlcomment)
					for (; i > 2; i--)
						x->xmlcomment(x, "-", 1);
				i = 2;
			}
			continue;
		} else if (c == '>' && i == 2) {
			if (x->xmlcommentend)
				x->xmlcommentend(x);
			return;
		} else if (i) {
			if (x->xmlcomment) {
				for (; i > 0; i--)
					x->xmlcomment(x, "-", 1);
			}
			i = 0;
		}

		if (datalen < sizeof(x->data) - 1) {
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
	size_t datalen = 0, i = 0;
	int c;

	if (x->xmlcdatastart)
		x->xmlcdatastart(x);
	while ((c = x->getnext()) != EOF) {
		if (c == ']' || c == '>') {
			if (x->xmlcdata) {
				x->data[datalen] = '\0';
				x->xmlcdata(x, x->data, datalen);
				datalen = 0;
			}
		}

		if (c == ']') {
			if (++i > 2) {
				if (x->xmlcdata)
					for (; i > 2; i--)
						x->xmlcdata(x, "]", 1);
				i = 2;
			}
			continue;
		} else if (c == '>' && i == 2) {
			if (x->xmlcdataend)
				x->xmlcdataend(x);
			return;
		} else if (i) {
			if (x->xmlcdata)
				for (; i > 0; i--)
					x->xmlcdata(x, "]", 1);
			i = 0;
		}

		if (datalen < sizeof(x->data) - 1) {
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

static int
codepointtoutf8(const uint32_t r, uint8_t *s)
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

static int
namedentitytostr(const char *e, char *buf, size_t bufsiz)
{
	static const struct {
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

static int
numericentitytostr(const char *e, char *buf, size_t bufsiz)
{
	uint32_t l = 0, cp = 0;
	size_t b, len;
	char *end;

	/* buffer is too small */
	if (bufsiz < 5)
		return -1;

	/* not a numeric entity */
	if (e[0] != '&' || e[1] != '#')
		return 0;

	/* e[1] == '#', numeric / hexadecimal entity */
	e += 2; /* skip "&#" */
	errno = 0;
	/* hex (16) or decimal (10) */
	if (*e == 'x')
		l = strtoul(e + 1, &end, 16);
	else
		l = strtoul(e, &end, 10);
	/* invalid value or not a well-formed entity or too high codepoint */
	if (errno || *end != ';' || l > 0x10FFFF)
		return 0;
	len = codepointtoutf8(l, buf);
	buf[len] = '\0';

	return len;
}

/* convert named- or numeric entity string to buffer string
 * returns byte-length of string. */
int
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
		return namedentitytostr(e, buf, bufsiz);
	else /* numeric entity */
		return numericentitytostr(e, buf, bufsiz);
}

void
xml_parse(XMLParser *x)
{
	int c, ispi;
	size_t datalen, tagdatalen, taglen;

	if (!x->getnext)
		return;
	while ((c = x->getnext()) != EOF && c != '<')
		; /* skip until < */

	while (c != EOF) {
		if (c == '<') { /* parse tag */
			if ((c = x->getnext()) == EOF)
				return;
			x->tag[0] = '\0';
			x->taglen = 0;
			if (c == '!') { /* cdata and comments */
				for (tagdatalen = 0; (c = x->getnext()) != EOF;) {
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
					while ((c = x->getnext()) != EOF && isspace(c))
						;
				if (c == EOF)
					return;
				x->tag[0] = c;
				ispi = (c == '?') ? 1 : 0;
				x->isshorttag = ispi;
				taglen = 1;
				while ((c = x->getnext()) != EOF) {
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
						x->tag[taglen++] = c; /* NOTE: tag name truncation */
				}
			}
		} else {
			/* parse tag data */
			datalen = 0;
			if (x->xmldatastart)
				x->xmldatastart(x);
			while ((c = x->getnext()) != EOF) {
				if (c == '&') {
					if (datalen) {
						x->data[datalen] = '\0';
						if (x->xmldata)
							x->xmldata(x, x->data, datalen);
					}
					x->data[0] = c;
					datalen = 1;
					while ((c = x->getnext()) != EOF) {
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
