#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "util.h"
#include "xml.h"

/* fast isspace(c) && c != ' ' check. */
#define ISWSNOSPACE(c)    (((unsigned)c - '\t') < 5)
#define ISINCONTENT(ctx)  ((ctx).iscontent && !((ctx).iscontenttag))
#define ISCONTENTTAG(ctx) (!((ctx).iscontent) && (ctx).iscontenttag)
/* string and size */
#define STRP(s)           s,sizeof(s)-1
/* length of string */
#define STRSIZ(s)         (sizeof(s)-1)

enum { FeedTypeNone = 0, FeedTypeRSS = 1, FeedTypeAtom = 2 };
static const char *feedtypes[] = { "", "rss", "atom" };

enum { ContentTypeNone = 0, ContentTypePlain = 1, ContentTypeHTML = 2 };
static const char *contenttypes[] = { "", "plain", "html" };

static const int FieldSeparator = '\t'; /* output field seperator character */

enum {
	TagUnknown = 0,
	/* RSS */
	RSSTagDcdate, RSSTagPubdate, RSSTagTitle,
	RSSTagLink, RSSTagDescription, RSSTagContentencoded,
	RSSTagGuid, RSSTagAuthor, RSSTagDccreator,
	/* Atom */
	AtomTagPublished, AtomTagUpdated, AtomTagTitle,
	AtomTagSummary, AtomTagContent,
	AtomTagId, AtomTagLink, AtomTagAuthor
};

/* String data / memory pool */
typedef struct string {
	char   *data;   /* data */
	size_t  len;    /* string length */
	size_t  bufsiz; /* allocated size */
} String;

/* Feed item */
typedef struct feeditem {
	String timestamp;
	String title;
	String link;
	String content;
	int    contenttype; /* ContentTypePlain or ContentTypeHTML */
	String id;
	String author;
	int    feedtype;    /* FeedTypeRSS or FeedTypeAtom */
} FeedItem;

typedef struct feedtag {
	char *name;
	size_t namelen;
	int id;
} FeedTag;

typedef struct feedcontext {
	String   *field;        /* pointer to current FeedItem field String */
	FeedItem  item;         /* data for current feed item */
	char      tag[256];     /* current tag _inside_ a feeditem */
	int       tagid;        /* unique number for parsed tag (faster comparison) */
	size_t    taglen;
	int       iscontent;    /* in content data */
	int       iscontenttag; /* in content tag */
	int       attrcount;
} FeedContext;

static size_t codepointtoutf8(uint32_t, uint32_t *);
static size_t entitytostr(const char *, char *, size_t);
static int    gettag(int, const char *, size_t);
static int    gettimetz(const char *, char *, size_t, int *);
static int    isattr(const char *, size_t, const char *, size_t);
static int    istag(const char *, size_t, const char *, size_t);
static size_t namedentitytostr(const char *, char *, size_t);
static int    parsetime(const char *, char *, size_t, time_t *);
static void   printfields(void);
static void   string_append(String *, const char *, size_t);
static void   string_buffer_init(String *, size_t);
static int    string_buffer_realloc(String *, size_t);
static void   string_clear(String *);
static void   string_print(String *);
static void   xml_handler_attr(XMLParser *, const char *, size_t,
                               const char *, size_t, const char *, size_t);
static void   xml_handler_attr_start(XMLParser *, const char *, size_t,
                                     const char *, size_t);
static void   xml_handler_attr_end(struct xmlparser *, const char *, size_t,
                                   const char *, size_t);
static void   xml_handler_cdata(XMLParser *, const char *, size_t);
static void   xml_handler_data(XMLParser *, const char *, size_t);
static void   xml_handler_data_entity(XMLParser *, const char *, size_t);
static void   xml_handler_end_element(XMLParser *, const char *, size_t, int);
static void   xml_handler_start_element(XMLParser *, const char *, size_t);
static void   xml_handler_start_element_parsed(XMLParser *, const char *,
                                               size_t, int);

static FeedContext ctx;
static XMLParser parser; /* XML parser state */
static char *append = NULL; /* append string after each output line */

/* unique number for parsed tag (faster comparison) */
static int
gettag(int feedtype, const char *name, size_t namelen)
{
	/* RSS, alphabetical order */
	static FeedTag rsstag[] = {
		{ STRP("author"),          RSSTagAuthor },
		{ STRP("content:encoded"), RSSTagContentencoded },
		{ STRP("dc:creator"),      RSSTagDccreator },
		{ STRP("dc:date"),         RSSTagDcdate },
		{ STRP("description"),     RSSTagDescription },
		{ STRP("guid"),            RSSTagGuid },
		{ STRP("link"),            RSSTagLink },
		{ STRP("pubdate"),         RSSTagPubdate },
		{ STRP("title"),           RSSTagTitle },
		{ NULL, 0, -1 }
	};
	/* Atom, alphabetical order */
	static FeedTag atomtag[] = {
		{ STRP("author"),    AtomTagAuthor },
		{ STRP("content"),   AtomTagContent },
		{ STRP("id"),        AtomTagId },
		{ STRP("link"),      AtomTagLink },
		{ STRP("published"), AtomTagPublished },
		{ STRP("summary"),   AtomTagSummary },
		{ STRP("title"),     AtomTagTitle },
		{ STRP("updated"),   AtomTagUpdated },
		{ NULL, 0, -1 }
	};
	int i, n;

	if(namelen < 2 || namelen > 15) /* optimization */
		return TagUnknown;

	if(feedtype == FeedTypeRSS) {
		for(i = 0; rsstag[i].name; i++) {
			if(!(n = strncasecmp(rsstag[i].name, name, rsstag[i].namelen)))
				return rsstag[i].id;
			/* optimization: it's sorted so nothing after it matches. */
			if(n > 0)
				return TagUnknown;
		}
	} else if(feedtype == FeedTypeAtom) {
		for(i = 0; atomtag[i].name; i++) {
			if(!(n = strncasecmp(atomtag[i].name, name, atomtag[i].namelen)))
				return atomtag[i].id;
			/* optimization: it's sorted so nothing after it matches. */
			if(n > 0)
				return TagUnknown;
		}
	}
	return TagUnknown;
}

static size_t
codepointtoutf8(uint32_t cp, uint32_t *utf)
{
	if(cp >= 0x10000) { /* 4 bytes */
		*utf = 0xf0808080 | ((cp & 0xfc0000) << 6) | ((cp & 0x3f000) << 4) |
		       ((cp & 0xfc0) << 2) | (cp & 0x3f);
		return 4;
	} else if(cp >= 0x00800) { /* 3 bytes */
		*utf = 0xe08080 | ((cp & 0x3f000) << 4) | ((cp & 0xfc0) << 2) |
		       (cp & 0x3f);
		return 3;
	} else if(cp >= 0x80) { /* 2 bytes */
		*utf = 0xc080 | ((cp & 0xfc0) << 2) | (cp & 0x3f);
		return 2;
	}
	*utf = cp & 0xff;
	return *utf ? 1 : 0; /* 1 byte */
}

static size_t
namedentitytostr(const char *e, char *buffer, size_t bufsiz)
{
	char *entities[6][2] = {
		{ "&lt;", "<" },
		{ "&gt;", ">" },
		{ "&apos;", "'" },
		{ "&amp;", "&" },
		{ "&quot;", "\"" },
		{ NULL, NULL }
	};
	size_t i;

	if(*e != '&' || bufsiz < 2) /* doesn't start with & */
		return 0;
	for(i = 0; entities[i][0]; i++) {
		/* NOTE: compares max 7 chars */
		if(!strncasecmp(e, entities[i][0], 6)) {
			buffer[0] = *(entities[i][1]);
			buffer[1] = '\0';
			return 1;
		}
	}
	return 0;
}

/* convert named- or numeric entity string to buffer string
 * returns byte-length of string. */
static size_t
entitytostr(const char *e, char *buffer, size_t bufsiz)
{
	uint32_t l = 0, cp = 0;
	size_t len = 0, b;
	int c;

	/* doesn't start with & */
	if(*e != '&' || bufsiz < 5)
		return 0;
	/* numeric / hexadecimal entity */
	if(e[1] == '#') {
		e += 2; /* skip &# */
		errno = 0;
		if(*e == 'x')
			l = strtoul(e + 1, NULL, 16); /* hex */
		else
			l = strtoul(e, NULL, 10); /* decimal */
		if(errno != 0)
			return 0; /* invalid value */
		if(!(len = codepointtoutf8(l, &cp)))
			return 0;
		/* make string */
		for(b = 0; b < len; b++)
			buffer[b] = (cp >> (8 * (len - 1 - b))) & 0xff;
		buffer[len] = '\0';
		/* escape whitespace */
		if(ISWSNOSPACE(buffer[0])) {
			switch(buffer[0]) {
			case '\n': c = 'n';  break;
			case '\\': c = '\\'; break;
			case '\t': c = 't';  break;
			default:   c = '\0'; break;
			}
			if(c != '\0') {
				buffer[0] = '\\';
				buffer[1] = c;
				buffer[2] = '\0';
				len = 2;
			}
		}
	} else {
		len = namedentitytostr(e, buffer, bufsiz);
	}
	return len;
}

/* clear string only; don't free, prevents unnecessary reallocation */
static void
string_clear(String *s)
{
	if(s->data)
		s->data[0] = '\0';
	s->len = 0;
}

static void
string_buffer_init(String *s, size_t len)
{
	if(!(s->data = malloc(len)))
		err(1, "malloc");
	s->bufsiz = len;
	string_clear(s);
}

static int
string_buffer_realloc(String *s, size_t newlen)
{
	char *p;
	size_t alloclen;

	for(alloclen = 16; alloclen <= newlen; alloclen *= 2);
	if(!(p = realloc(s->data, alloclen)))
		err(1, "realloc");
	s->bufsiz = alloclen;
	s->data = p;
	return s->bufsiz;
}

static void
string_append(String *s, const char *data, size_t len)
{
	if(!len || *data == '\0')
		return;
	/* check if allocation is necesary, don't shrink buffer
	   should be more than bufsiz ofcourse */
	if(s->len + len >= s->bufsiz)
		string_buffer_realloc(s, s->len + len + 1);
	memcpy(s->data + s->len, data, len);
	s->len += len;
	s->data[s->len] = '\0';
}

/* get timezone from string, return as formatted string and time offset,
 * for the offset it assumes GMT */
static int
gettimetz(const char *s, char *buf, size_t bufsiz, int *tzoffset)
{
	char tzname[16] = "GMT";
	int tzhour = 0, tzmin = 0, r;
	char c = '+';
	size_t i;

	s = trimstart(s);
	if(!*s || *s == 'Z' || *s == 'z')
		goto time_ok;

	/* look until some common timezone delimiters are found */
	for(i = 0; s[i] && isalpha((int)s[i]); i++)
		;
	/* copy tz name */
	if(i >= sizeof(tzname))
		i = sizeof(tzname) - 1;
	if(i > 0)
		memcpy(tzname, s, i);
	tzname[i] = '\0';

	if((sscanf(s, "%c%02d:%02d", &c, &tzhour, &tzmin)) == 3) {
		;
	} else if(sscanf(s, "%c%02d%02d", &c, &tzhour, &tzmin) == 3) {
		;
	} else if(sscanf(s, "%c%d", &c, &tzhour) == 2) {
		tzmin = 0;
	} else {
		c = '+';
		tzhour = 0;
		tzmin = 0;
	}
time_ok:
	r = snprintf(buf, bufsiz, "%s%c%02d%02d",
	             tzname[0] ? tzname : "GMT",
	             c, tzhour, tzmin);
	if(r < 0 || (size_t)r >= bufsiz)
		return -1; /* truncation or error */
	if(tzoffset)
		*tzoffset = (tzhour * 3600) + (tzmin * 60) * (c == '-' ? -1 : 1);
	return 0;
}

static int
parsetime(const char *s, char *buf, size_t bufsiz, time_t *tp)
{
	time_t t;
	char tz[64] = "";
	struct tm tm;
	const char *formats[] = {
		"%a, %d %b %Y %H:%M:%S",
		"%Y-%m-%d %H:%M:%S",
		"%Y-%m-%dT%H:%M:%S",
		NULL
	};
	char *p;
	size_t i;
	int tzoffset, r;

	memset(&tm, 0, sizeof(tm));
	for(i = 0; formats[i]; i++) {
		if(!(p = strptime(s, formats[i], &tm)))
			continue;
		tm.tm_isdst = -1; /* don't use DST */
		if((t = mktime(&tm)) == -1) /* error */
			return -1;
		if(gettimetz(p, tz, sizeof(tz), &tzoffset) != -1)
			t -= tzoffset;
		if(buf) {
			r = snprintf(buf, bufsiz,
			         "%04d-%02d-%02d %02d:%02d:%02d %s",
			         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			         tm.tm_hour, tm.tm_min, tm.tm_sec, tz);
			if(r == -1 || (size_t)r >= bufsiz)
				return -1; /* truncation */
		}
		if(tp)
			*tp = t;
		return 0;
	}
	return -1;
}

/* print text, escape tabs, newline and carriage return etc */
static void
string_print(String *s)
{
	const char *p, *e;

	/* skip leading whitespace */
	p = trimstart(s->data);
	e = trimend(p);

	for(; *p && p != e; p++) {
		if(ISWSNOSPACE(*p)) {
			switch(*p) {
			case '\n': fputs("\\n", stdout); break;
			case '\\': fputs("\\\\", stdout); break;
			case '\t': fputs("\\t", stdout); break;
			default: break; /* ignore other whitespace chars */
			}
		} else {
			putchar(*p);
		}
	}
}

static void
printfields(void)
{
	char timebuf[64];
	time_t t;
	int r;

	/* parse time, timestamp and formatted timestamp field is empty
	 * if the parsed time is invalid */
	r = parsetime((&ctx.item.timestamp)->data, timebuf,
	              sizeof(timebuf), &t);
	if(r != -1)
		printf("%ld", (long)t);
	putchar(FieldSeparator);
	if(r != -1)
		fputs(timebuf, stdout);
	putchar(FieldSeparator);
	string_print(&ctx.item.title);
	putchar(FieldSeparator);
	string_print(&ctx.item.link);
	putchar(FieldSeparator);
	string_print(&ctx.item.content);
	putchar(FieldSeparator);
	fputs(contenttypes[ctx.item.contenttype], stdout);
	putchar(FieldSeparator);
	string_print(&ctx.item.id);
	putchar(FieldSeparator);
	string_print(&ctx.item.author);
	putchar(FieldSeparator);
	fputs(feedtypes[ctx.item.feedtype], stdout);
	if(append) {
		putchar(FieldSeparator);
		fputs(append, stdout);
	}
	putchar('\n');
}

static int
istag(const char *name, size_t len, const char *name2, size_t len2)
{
	return (len == len2 && !strcasecmp(name, name2));
}

static int
isattr(const char *name, size_t len, const char *name2, size_t len2) {
	return (len == len2 && !strcasecmp(name, name2));
}

/* NOTE: this handler can be called multiple times if the data in this
 * block is bigger than the buffer */
static void
xml_handler_data(XMLParser *p, const char *s, size_t len)
{
	if(ctx.field) {
		/* add only data from <name> inside <author> tag
		 * or any other non-<author> tag  */
		if(ctx.tagid != AtomTagAuthor || !strcmp(p->tag, "name"))
			string_append(ctx.field, s, len);
	}
}

static void
xml_handler_cdata(XMLParser *p, const char *s, size_t len)
{
	(void)p;

	if(ctx.field)
		string_append(ctx.field, s, len);
}

static void
xml_handler_attr_start(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen)
{
	(void)tag;
	(void)taglen;

	if(!ISINCONTENT(ctx))
		return;

	/* handles transforming inline XML to data */
	if(!ctx.attrcount)
		xml_handler_data(p, " ", 1);
	ctx.attrcount++;
	xml_handler_data(p, name, namelen);
	xml_handler_data(p, "=\"", 2);
}

static void
xml_handler_attr_end(struct xmlparser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen)
{
	(void)tag;
	(void)taglen;
	(void)name;
	(void)namelen;

	if(!ISINCONTENT(ctx))
		return;

	/* handles transforming inline XML to data */
	xml_handler_data(p, "\"", 1);
	ctx.attrcount = 0;
}

static void
xml_handler_start_element_parsed(XMLParser *p, const char *tag, size_t taglen,
	int isshort)
{
	(void)tag;
	(void)taglen;

	if(!ISINCONTENT(ctx))
		return;

	if(isshort)
		xml_handler_data(p, "/>", 2);
	else
		xml_handler_data(p, ">", 1);
}

static void
xml_handler_attr(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen, const char *value,
	size_t valuelen)
{
	(void)tag;
	(void)taglen;

	/* handles transforming inline XML to data */
	if(ISINCONTENT(ctx)) {
		xml_handler_data(p, value, valuelen);
		return;
	}

	if(ctx.item.feedtype == FeedTypeAtom) {
		if(ISCONTENTTAG(ctx)) {
			if(isattr(name, namelen, STRP("type")) &&
			   (isattr(value, valuelen, STRP("xhtml")) ||
			   isattr(value, valuelen, STRP("text/xhtml")) ||
			   isattr(value, valuelen, STRP("html")) ||
			   isattr(value, valuelen, STRP("text/html"))))
			{
				ctx.item.contenttype = ContentTypeHTML;
				ctx.iscontent = 1;
/*				p->xmldataentity = NULL;*/ /* TODO: don't convert entities? test this */
				p->xmlattrstart = xml_handler_attr_start;
				p->xmlattrend = xml_handler_attr_end;
				p->xmltagstartparsed = xml_handler_start_element_parsed;
			}
		} else if(ctx.tagid == AtomTagLink &&
		          isattr(name, namelen, STRP("href")))
		{
			/* link href attribute */
			string_append(&ctx.item.link, value, valuelen);
		}
	}
}

static void
xml_handler_start_element(XMLParser *p, const char *name, size_t namelen)
{
	if(ISCONTENTTAG(ctx)) {
		/* starts with div, handle as XML, don't convert entities (set handle to NULL) */
		if(ctx.item.feedtype == FeedTypeAtom &&
		   namelen == STRSIZ("div") &&
		   !strncmp(name, STRP("div"))) {
			p->xmldataentity = NULL;
		}
	}
	/* TODO: changed, iscontenttag can be 0 or 1 ? */
	if(ISINCONTENT(ctx)) {
		ctx.attrcount = 0;
		ctx.iscontenttag = 0;
		xml_handler_data(p, "<", 1);
		xml_handler_data(p, name, namelen);
		return;
	}

	/* TODO: cleanup, merge with code below ?, return function if FeedTypeNone */
/*	ctx.iscontenttag = 0;*/

	/* start of RSS or Atom item / entry */
	if(ctx.item.feedtype == FeedTypeNone) {
		if(istag(name, namelen, STRP("entry"))) { /* Atom */
			ctx.item.feedtype = FeedTypeAtom;
			ctx.item.contenttype = ContentTypePlain; /* Default content type */
			ctx.field = NULL; /* XXX: optimization */
		} else if(istag(name, namelen, STRP("item"))) { /* RSS */
			ctx.item.feedtype = FeedTypeRSS;
			ctx.item.contenttype = ContentTypeHTML; /* Default content type */
			ctx.field = NULL; /* XXX: optimization */
		}
		return;
	}

	/* tag already set: return */
	if(ctx.tag[0] != '\0')
		return;
	/* in item */
	strlcpy(ctx.tag, name, sizeof(ctx.tag));
	ctx.taglen = namelen;
	ctx.tagid = gettag(ctx.item.feedtype, ctx.tag, ctx.taglen);
	if(ctx.tagid == TagUnknown)
		ctx.field = NULL;
	if(ctx.item.feedtype == FeedTypeRSS) {
		if(ctx.tagid == RSSTagPubdate || ctx.tagid == RSSTagDcdate)
			ctx.field = &ctx.item.timestamp;
		else if(ctx.tagid == RSSTagTitle)
			ctx.field = &ctx.item.title;
		else if(ctx.tagid == RSSTagLink)
			ctx.field = &ctx.item.link;
		else if(ctx.tagid == RSSTagDescription ||
				ctx.tagid == RSSTagContentencoded) {
			/* clear content, assumes previous content was not a summary text */
			if(ctx.tagid == RSSTagContentencoded && ctx.item.content.len)
				string_clear(&ctx.item.content);
			/* ignore, prefer content:encoded over description */
			if(!(ctx.tagid == RSSTagDescription && ctx.item.content.len)) {
				ctx.iscontenttag = 1;
				ctx.field = &ctx.item.content;
				return;
			}
		} else if(ctx.tagid == RSSTagGuid)
			ctx.field = &ctx.item.id;
		else if(ctx.tagid == RSSTagAuthor || ctx.tagid == RSSTagDccreator)
			ctx.field = &ctx.item.author;
		/* clear field */
		if(ctx.field)
			string_clear(ctx.field);
	} else if(ctx.item.feedtype == FeedTypeAtom) {
		if(ctx.tagid == AtomTagPublished || ctx.tagid == AtomTagUpdated)
			ctx.field = &ctx.item.timestamp;
		else if(ctx.tagid == AtomTagTitle)
			ctx.field = &ctx.item.title;
		else if(ctx.tagid == AtomTagSummary || ctx.tagid == AtomTagContent) {
			/* clear content, assumes previous content was not a summary text */
			if(ctx.tagid == AtomTagContent && ctx.item.content.len)
				string_clear(&ctx.item.content);
			/* ignore, prefer content:encoded over description */
			if(!(ctx.tagid == AtomTagSummary && ctx.item.content.len)) {
				ctx.iscontenttag = 1;
				ctx.field = &ctx.item.content;
				return;
			}
		} else if(ctx.tagid == AtomTagId)
			ctx.field = &ctx.item.id;
		else if(ctx.tagid == AtomTagLink)
			ctx.field = &ctx.item.link;
		else if(ctx.tagid == AtomTagAuthor)
			ctx.field = &ctx.item.author;
		/* clear field */
		if(ctx.field)
			string_clear(ctx.field);
	}
}

static void
xml_handler_data_entity(XMLParser *p, const char *data, size_t datalen)
{
	char buffer[16];
	size_t len;

	/* try to translate entity, else just pass as data */
	if((len = entitytostr(data, buffer, sizeof(buffer))) > 0)
		xml_handler_data(p, buffer, len);
	else
		xml_handler_data(p, data, datalen);
}

static void
xml_handler_end_element(XMLParser *p, const char *name, size_t namelen, int isshort)
{
	int tagid;

	if(ctx.iscontent) {
		ctx.attrcount = 0;
		/* TODO: optimize */
		tagid = gettag(ctx.item.feedtype, name, namelen);
		if(ctx.tagid == tagid) { /* close content */
			ctx.iscontent = 0;
			ctx.iscontenttag = 0;

			p->xmldataentity = xml_handler_data_entity;
			p->xmlattrstart = NULL;
			p->xmlattrend = NULL;
			p->xmltagstartparsed = NULL;

			ctx.tag[0] = '\0'; /* unset tag */
			ctx.taglen = 0;
			ctx.tagid = TagUnknown;

			return; /* TODO: not sure if !isshort check below should be skipped */
		}
		if(!isshort) {
			xml_handler_data(p, "</", 2);
			xml_handler_data(p, name, namelen);
			xml_handler_data(p, ">", 1);
		}
		return;
	}
	if(ctx.item.feedtype == FeedTypeNone)
		return;
	/* end of RSS or Atom entry / item */
	/* TODO: optimize, use gettag() ? to tagid? */
	if((ctx.item.feedtype == FeedTypeAtom &&
	   istag(name, namelen, STRP("entry"))) || /* Atom */
	   (ctx.item.feedtype == FeedTypeRSS &&
	   istag(name, namelen, STRP("item")))) /* RSS */
	{
		printfields();

		/* clear strings */
		string_clear(&ctx.item.timestamp);
		string_clear(&ctx.item.title);
		string_clear(&ctx.item.link);
		string_clear(&ctx.item.content);
		string_clear(&ctx.item.id);
		string_clear(&ctx.item.author);
		ctx.item.feedtype = FeedTypeNone;
		ctx.item.contenttype = ContentTypePlain;
		ctx.tag[0] = '\0'; /* unset tag */
		ctx.taglen = 0;
		ctx.tagid = TagUnknown;

		/* not sure if needed */
		ctx.iscontenttag = 0;
		ctx.iscontent = 0;
	} else if(ctx.taglen == namelen && !strcmp(ctx.tag, name)) {
		/* clear */
		/* XXX: optimize ? */
		ctx.field = NULL;
		ctx.tag[0] = '\0'; /* unset tag */
		ctx.taglen = 0;
		ctx.tagid = TagUnknown;

		/* not sure if needed */
		ctx.iscontenttag = 0;
		ctx.iscontent = 0;
	}
}

int
main(int argc, char *argv[])
{
	if(argc > 1) {
		append = argv[1];
		if(!strcmp(argv[1], "-v")) {
			printf("%s\n", VERSION);
			return 0;
		}
	}

	/* init strings and initial memory pool size */
	string_buffer_init(&ctx.item.timestamp, 64);
	string_buffer_init(&ctx.item.title, 256);
	string_buffer_init(&ctx.item.link, 1024);
	string_buffer_init(&ctx.item.content, 4096);
	string_buffer_init(&ctx.item.id, 1024);
	string_buffer_init(&ctx.item.author, 256);
	ctx.item.contenttype = ContentTypePlain;
	ctx.item.feedtype = FeedTypeNone;

	xmlparser_init(&parser, stdin);
	parser.xmltagstart = xml_handler_start_element;
	parser.xmltagend = xml_handler_end_element;
	parser.xmldata = xml_handler_data;
	parser.xmldataentity = xml_handler_data_entity;
	parser.xmlattr = xml_handler_attr;
	parser.xmlcdata = xml_handler_cdata;
	xmlparser_parse(&parser);

	return 0;
}
