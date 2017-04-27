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

#define ISINCONTENT(ctx)  ((ctx).iscontent && !((ctx).iscontenttag))
#define ISCONTENTTAG(ctx) (!((ctx).iscontent) && (ctx).iscontenttag)
/* string and byte-length */
#define STRP(s)           s,sizeof(s)-1

enum FeedType {
	FeedTypeNone = 0,
	FeedTypeRSS  = 1,
	FeedTypeAtom = 2
};

enum ContentType {
	ContentTypeNone  = 0,
	ContentTypePlain = 1,
	ContentTypeHTML  = 2
};
static const char *contenttypes[] = { "", "plain", "html" };

/* String data / memory pool */
typedef struct string {
	char   *data;   /* data */
	size_t  len;    /* string length */
	size_t  bufsiz; /* allocated size */
} String;

/* NOTE: the order of these fields (content, date, author) indicate the
 *       priority to use them, from least important to high. */
enum TagId {
	TagUnknown = 0,
	/* RSS */
	RSSTagDcdate, RSSTagPubdate,
	RSSTagTitle,
	RSSTagMediaDescription, RSSTagDescription, RSSTagContentEncoded,
	RSSTagGuid,
	RSSTagLink,
	RSSTagAuthor, RSSTagDccreator,
	/* Atom */
	AtomTagUpdated, AtomTagPublished,
	AtomTagTitle,
	AtomTagMediaDescription, AtomTagSummary, AtomTagContent,
	AtomTagId,
	AtomTagLink,
	AtomTagAuthor,
	TagLast
};

typedef struct feedtag {
	char       *name; /* name of tag to match */
	size_t      len;  /* len of `name` */
	enum TagId  id;   /* unique ID */
} FeedTag;

typedef struct field {
	String     str;
	enum TagId tagid; /* tagid set previously, used for tag priority */
} FeedField;

enum {
	FeedFieldTime = 0, FeedFieldTitle, FeedFieldLink, FeedFieldContent,
	FeedFieldId, FeedFieldAuthor, FeedFieldLast
};

typedef struct feedcontext {
	String          *field;             /* current FeedItem field String */
	FeedField        fields[FeedFieldLast]; /* data for current item */
	enum TagId       tagid;             /* unique number for parsed tag */
	int              iscontent;         /* in content data */
	int              iscontenttag;      /* in content tag */
	enum ContentType contenttype;       /* content-type for item */
	enum FeedType    feedtype;
	int              attrcount; /* count item HTML element attributes */
} FeedContext;

static long long  datetounix(long long, int, int, int, int, int);
static enum TagId gettag(enum FeedType, const char *, size_t);
static long long  gettzoffset(const char *);
static int  isattr(const char *, size_t, const char *, size_t);
static int  istag(const char *, size_t, const char *, size_t);
static int  parsetime(const char *, time_t *);
static void printfields(void);
static void string_append(String *, const char *, size_t);
static void string_buffer_realloc(String *, size_t);
static void string_clear(String *);
static void string_print_encoded(String *);
static void string_print_trimmed(String *);
static void xml_handler_attr(XMLParser *, const char *, size_t,
                             const char *, size_t, const char *, size_t);
static void xml_handler_attr_end(XMLParser *, const char *, size_t,
                                 const char *, size_t);
static void xml_handler_attr_start(XMLParser *, const char *, size_t,
                                   const char *, size_t);
static void xml_handler_cdata(XMLParser *, const char *, size_t);
static void xml_handler_data(XMLParser *, const char *, size_t);
static void xml_handler_data_entity(XMLParser *, const char *, size_t);
static void xml_handler_end_el(XMLParser *, const char *, size_t, int);
static void xml_handler_start_el(XMLParser *, const char *, size_t);
static void xml_handler_start_el_parsed(XMLParser *, const char *,
                                        size_t, int);

/* map tag name to tagid */
/* RSS, alphabetical order */
static FeedTag rsstags[] = {
	{ STRP("author"),            RSSTagAuthor            },
	{ STRP("content:encoded"),   RSSTagContentEncoded    },
	{ STRP("dc:creator"),        RSSTagDccreator         },
	{ STRP("dc:date"),           RSSTagDcdate            },
	{ STRP("description"),       RSSTagDescription       },
	{ STRP("guid"),              RSSTagGuid              },
	{ STRP("link"),              RSSTagLink              },
	{ STRP("media:description"), RSSTagMediaDescription  },
	{ STRP("pubdate"),           RSSTagPubdate           },
	{ STRP("title"),             RSSTagTitle             },
	{ NULL, 0, -1 }
};
/* Atom, alphabetical order */
static FeedTag atomtags[] = {
	/* <author><name></name></author> */
	{ STRP("author"),            AtomTagAuthor           },
	{ STRP("content"),           AtomTagContent          },
	{ STRP("id"),                AtomTagId               },
	/* <link href="" /> */
	{ STRP("link"),              AtomTagLink             },
	{ STRP("media:description"), AtomTagMediaDescription },
	{ STRP("published"),         AtomTagPublished        },
	{ STRP("summary"),           AtomTagSummary          },
	{ STRP("title"),             AtomTagTitle            },
	{ STRP("updated"),           AtomTagUpdated          },
	{ NULL, 0, -1 }
};

/* map tagid type to RSS/Atom field */
static int fieldmap[TagLast] = {
	/* RSS */
	[RSSTagDcdate]            = FeedFieldTime,
	[RSSTagPubdate]           = FeedFieldTime,
	[RSSTagTitle]             = FeedFieldTitle,
	[RSSTagMediaDescription]  = FeedFieldContent,
	[RSSTagDescription]       = FeedFieldContent,
	[RSSTagContentEncoded]    = FeedFieldContent,
	[RSSTagGuid]              = FeedFieldId,
	[RSSTagLink]              = FeedFieldLink,
	[RSSTagAuthor]            = FeedFieldAuthor,
	[RSSTagDccreator]         = FeedFieldAuthor,
	/* Atom */
	[AtomTagUpdated]          = FeedFieldTime,
	[AtomTagPublished]        = FeedFieldTime,
	[AtomTagTitle]            = FeedFieldTitle,
	[AtomTagMediaDescription] = FeedFieldContent,
	[AtomTagSummary]          = FeedFieldContent,
	[AtomTagContent]          = FeedFieldContent,
	[AtomTagId]               = FeedFieldId,
	[AtomTagLink]             = FeedFieldLink,
	[AtomTagAuthor]           = FeedFieldAuthor
};

static const int FieldSeparator = '\t';
static const char *baseurl = "";

static FeedContext ctx;
static XMLParser parser; /* XML parser state */

/* Unique tagid for parsed tag name. */
static enum TagId
gettag(enum FeedType feedtype, const char *name, size_t namelen)
{
	const FeedTag *tags;
	size_t i;

	/* optimization: these are always non-matching */
	if (namelen < 2 || namelen > 17)
		return TagUnknown;

	switch (feedtype) {
	case FeedTypeRSS:  tags = &rsstags[0];  break;
	case FeedTypeAtom: tags = &atomtags[0]; break;
	default:           return TagUnknown;
	}

	/* TODO: test if checking for sort order matters performance-wise */
	for (i = 0; tags[i].name; i++)
		if (istag(tags[i].name, tags[i].len, name, namelen))
			return tags[i].id;

	return TagUnknown;
}

/* Clear string only; don't free, prevents unnecessary reallocation. */
static void
string_clear(String *s)
{
	if (s->data)
		s->data[0] = '\0';
	s->len = 0;
}

static void
string_buffer_realloc(String *s, size_t newlen)
{
	size_t alloclen;

	for (alloclen = 64; alloclen <= newlen; alloclen *= 2)
		;
	if (!(s->data = realloc(s->data, alloclen)))
		err(1, "realloc");
	s->bufsiz = alloclen;
}

static void
string_append(String *s, const char *data, size_t len)
{
	if (!len)
		return;
	/* check if allocation is necesary, don't shrink buffer,
	 * should be more than bufsiz ofcourse. */
	if (s->len + len >= s->bufsiz)
		string_buffer_realloc(s, s->len + len + 1);
	memcpy(s->data + s->len, data, len);
	s->len += len;
	s->data[s->len] = '\0';
}

/* Print text, encode TABs, newlines and '\', remove other whitespace.
 * Remove leading and trailing whitespace. */
static void
string_print_encoded(String *s)
{
	const char *p, *e;

	if (!s->data || !s->len)
		return;

	/* skip leading whitespace */
	for (p = s->data; *p && isspace((int)*p); p++)
		;
	/* seek location of trailing whitespace */
	for (e = s->data + s->len; e > p && isspace((int)*(e - 1)); e--)
		;

	for (; *p && p != e; p++) {
		switch (*p) {
		case '\n': fputs("\\n",  stdout); break;
		case '\\': fputs("\\\\", stdout); break;
		case '\t': fputs("\\t",  stdout); break;
		default:
			/* ignore control chars */
			if (!iscntrl((int)*p))
				putchar(*p);
			break;
		}
	}
}

/* Print text, replace TABs, carriage return and other whitespace with ' '.
 * Other control chars are removed. Remove leading and trailing whitespace. */
static void
string_print_trimmed(String *s)
{
	const char *p, *e;

	if (!s->data || !s->len)
		return;

	/* skip leading whitespace */
	for (p = s->data; *p && isspace((int)*p); p++)
		;
	/* seek location of trailing whitespace */
	for (e = s->data + s->len; e > p && isspace((int)*(e - 1)); e--)
		;

	for (; *p && p != e; p++) {
		if (isspace((int)*p))
			putchar(' '); /* any whitespace to space */
		else if (!iscntrl((int)*p))
			/* ignore other control chars */
			putchar((int)*p);
	}
}

long long
datetounix(long long year, int mon, int day, int hour, int min, int sec)
{
	static const int secs_through_month[] = {
		0, 31 * 86400, 59 * 86400, 90 * 86400,
		120 * 86400, 151 * 86400, 181 * 86400, 212 * 86400,
		243 * 86400, 273 * 86400, 304 * 86400, 334 * 86400 };
	int is_leap = 0, cycles, centuries = 0, leaps = 0, rem;
	long long t;

	if (year - 2ULL <= 136) {
		leaps = (year - 68) >> 2;
		if (!((year - 68) & 3)) {
			leaps--;
			is_leap = 1;
		} else {
			is_leap = 0;
		}
		t = 31536000 * (year - 70) + 86400 * leaps;
	} else {
		cycles = (year - 100) / 400;
		rem = (year - 100) % 400;
		if (rem < 0) {
			cycles--;
			rem += 400;
		}
		if (!rem) {
			is_leap = 1;
		} else {
			if (rem >= 300)
				centuries = 3, rem -= 300;
			else if (rem >= 200)
				centuries = 2, rem -= 200;
			else if (rem >= 100)
				centuries = 1, rem -= 100;
			if (rem) {
				leaps = rem / 4U;
				rem %= 4U;
				is_leap = !rem;
			}
		}
		leaps += 97 * cycles + 24 * centuries - is_leap;
		t = (year - 100) * 31536000LL + leaps * 86400LL + 946684800 + 86400;
	}
	t += secs_through_month[mon];
	if (is_leap && mon >= 2)
		t += 86400;
	t += 86400LL * (day - 1);
	t += 3600LL * hour;
	t += 60LL * min;
	t += sec;

	return t;
}

/* Get timezone from string, return time offset in seconds from UTC.
 * NOTE: only parses timezones in RFC-822, other timezones are ambiguous
 * anyway. If needed you can add some yourself, like "cest", "cet" etc. */
static long long
gettzoffset(const char *s)
{
	static struct {
		char *name;
		size_t len;
		const int offhour;
	} tzones[] = {
		{ STRP("A"),   -1 * 3600 },
		{ STRP("CDT"), -5 * 3600 },
		{ STRP("CST"), -6 * 3600 },
		{ STRP("EDT"), -4 * 3600 },
		{ STRP("EST"), -5 * 3600 },
		{ STRP("MDT"), -6 * 3600 },
		{ STRP("MST"), -7 * 3600 },
		{ STRP("PDT"), -7 * 3600 },
		{ STRP("PST"), -8 * 3600 },
		{ STRP("M"),   -2 * 3600 },
		{ STRP("N"),    1 * 3600 },
		{ STRP("Y"),   12 * 3600 },
	};
	const char *p;
	int tzhour = 0, tzmin = 0;
	size_t i, namelen;

	for (; *s && isspace((int)*s); s++)
		;
	switch (s[0]) {
	case '-': /* offset */
	case '+':
		for (i = 0, p = s + 1; i < 2 && *p && isdigit(*p); i++, p++)
			tzhour = (tzhour * 10) + (*p - '0');
		if (*p && !isdigit(*p))
			p++;
		for (i = 0; i < 2 && *p && isdigit(*p); i++, p++)
			tzmin = (tzmin * 10) + (*p - '0');
		return ((tzhour * 3600) + (tzmin * 60)) * (s[0] == '-' ? -1 : 1);
	default: /* timezone name */
		for (i = 0; *s && isalpha((int)s[i]); i++)
			;
		namelen = i; /* end of name */
		/* optimization: these are always non-matching */
		if (namelen < 1 || namelen > 3)
			return 0;
		/* compare tz and adjust offset relative to UTC */
		for (i = 0; i < sizeof(tzones) / sizeof(*tzones); i++) {
			if (tzones[i].len == namelen &&
			    !strncmp(s, tzones[i].name, namelen))
				return tzones[i].offhour;
		}
	}
	return 0;
}

static int
parsetime(const char *s, time_t *tp)
{
	static struct {
		char *name;
		int len;
	} mons[] = {
		{ STRP("January"),   },
		{ STRP("February"),  },
		{ STRP("March"),     },
		{ STRP("April"),     },
		{ STRP("May"),       },
		{ STRP("June"),      },
		{ STRP("July"),      },
		{ STRP("August"),    },
		{ STRP("September"), },
		{ STRP("October"),   },
		{ STRP("November"),  },
		{ STRP("December"),  },
	};
	const char *end = NULL;
	int va[6] = { 0 }, i, j, v, vi;
	size_t m;

	for (; *s && isspace((int)*s); s++)
		;
	if (!isdigit((int)*s) && !isalpha((int)*s))
		return -1;

	if (isdigit((int)*s)) {
		/* format "%Y-%m-%d %H:%M:%S" or "%Y-%m-%dT%H:%M:%S" */
		vi = 0;
time:
		for (; *s && vi < 6; vi++) {
			for (i = 0, v = 0; *s && i < 4 && isdigit((int)*s); s++, i++)
				v = (v * 10) + (*s - '0');
			va[vi] = v;
			if ((vi < 2 && *s == '-') ||
			    (vi == 2 && (*s == 'T' || isspace((int)*s))) ||
			    (vi > 2 && *s == ':'))
				s++;
		}
		/* TODO: only if seconds are parsed (vi == 5)? */
		/* skip milliseconds for: %Y-%m-%dT%H:%M:%S.000Z */
		if (*s == '.') {
			for (s++; *s && isdigit((int)*s); s++)
				;
		}
		end = s;
	} else if (isalpha((int)*s)) {
		/* format: "%a, %d %b %Y %H:%M:%S" */
		/* parse "%a, %d %b %Y " part, then use time parsing as above */
		for (; *s && isalpha((int)*s); s++)
			;
		for (; *s && isspace((int)*s); s++)
			;
		if (*s != ',')
			return -1;
		for (s++; *s && isspace((int)*s); s++)
			;
		for (v = 0, i = 0; *s && i < 4 && isdigit((int)*s); s++, i++)
			v = (v * 10) + (*s - '0');
		va[2] = v; /* day */
		for (; *s && isspace((int)*s); s++)
			;
		/* end of word month */
		for (j = 0; *s && isalpha((int)s[j]); j++)
			;
		/* check month name */
		if (j < 3 || j > 9)
			return -1; /* month cannot match */
		for (m = 0; m < sizeof(mons) / sizeof(*mons); m++) {
			/* abbreviation (3 length) or long name */
			if ((j == 3 || j == mons[m].len) &&
			    !strncasecmp(mons[m].name, s, j)) {
				va[1] = m + 1;
				s += j;
				break;
			}
		}
		if (m >= 12)
			return -1; /* no month found */
		for (; *s && isspace((int)*s); s++)
			;
		for (v = 0, i = 0; *s && i < 4 && isdigit((int)*s); s++, i++)
			v = (v * 10) + (*s - '0');
		va[0] = v; /* year */
		for (; *s && isspace((int)*s); s++)
			;
		/* parse regular time, see above */
		vi = 3;
		goto time;
	} else {
		return -1;
	}

	/* invalid range */
	if (va[0] < 0 || va[0] > 9999 ||
	    va[1] < 1 || va[1] > 12 ||
	    va[2] < 1 || va[2] > 31 ||
	    va[3] < 0 || va[3] > 23 ||
	    va[4] < 0 || va[4] > 59 ||
	    va[5] < 0 || va[5] > 59)
		return -1;

	if (tp)
		*tp = datetounix(va[0] - 1900, va[1] - 1, va[2], va[3], va[4], va[5]) -
		      gettzoffset(end);
	return 0;
}

static void
printfields(void)
{
	char link[4096];
	time_t t;
	int r = -1;

	/* parse time, timestamp and formatted timestamp field is empty
	 * if the parsed time is invalid */
	if (ctx.fields[FeedFieldTime].str.data)
		r = parsetime(ctx.fields[FeedFieldTime].str.data, &t);
	if (r != -1)
		printf("%lld", (long long)t);
	putchar(FieldSeparator);
	string_print_trimmed(&ctx.fields[FeedFieldTitle].str);
	putchar(FieldSeparator);
	/* always print absolute urls */
	if (ctx.fields[FeedFieldLink].str.data &&
	    absuri(link, sizeof(link), ctx.fields[FeedFieldLink].str.data,
	           baseurl) != -1)
		fputs(link, stdout);
	putchar(FieldSeparator);
	string_print_encoded(&ctx.fields[FeedFieldContent].str);
	putchar(FieldSeparator);
	fputs(contenttypes[ctx.contenttype], stdout);
	putchar(FieldSeparator);
	string_print_trimmed(&ctx.fields[FeedFieldId].str);
	putchar(FieldSeparator);
	string_print_trimmed(&ctx.fields[FeedFieldAuthor].str);
	putchar('\n');
}

static int
istag(const char *name, size_t len, const char *name2, size_t len2)
{
	return (len == len2 && !strcasecmp(name, name2));
}

static int
isattr(const char *name, size_t len, const char *name2, size_t len2)
{
	return (len == len2 && !strcasecmp(name, name2));
}

static void
xml_handler_attr(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen, const char *value,
	size_t valuelen)
{
	/* handles transforming inline XML to data */
	if (ISINCONTENT(ctx)) {
		if (ctx.contenttype == ContentTypeHTML)
			xml_handler_data(p, value, valuelen);
		return;
	}

	if (ctx.feedtype == FeedTypeAtom) {
		if (ISCONTENTTAG(ctx)) {
			if (isattr(name, namelen, STRP("type")) &&
			   (isattr(value, valuelen, STRP("xhtml")) ||
			    isattr(value, valuelen, STRP("text/xhtml")) ||
			    isattr(value, valuelen, STRP("html")) ||
			    isattr(value, valuelen, STRP("text/html"))))
			{
				ctx.contenttype = ContentTypeHTML;
			}
		} else if (ctx.tagid == AtomTagLink &&
			   isattr(name, namelen, STRP("href")) &&
			   ctx.field)
		{
			/* link href attribute */
			string_append(ctx.field, value, valuelen);
		}
	}
}

static void
xml_handler_attr_end(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen)
{
	if (!ISINCONTENT(ctx) || ctx.contenttype != ContentTypeHTML)
		return;

	/* handles transforming inline XML to data */
	xml_handler_data(p, "\"", 1);
	ctx.attrcount = 0;
}

static void
xml_handler_attr_start(XMLParser *p, const char *tag, size_t taglen,
	const char *name, size_t namelen)
{
	if (!ISINCONTENT(ctx) || ctx.contenttype != ContentTypeHTML)
		return;

	/* handles transforming inline XML to data */
	if (!ctx.attrcount)
		xml_handler_data(p, " ", 1);
	ctx.attrcount++;
	xml_handler_data(p, name, namelen);
	xml_handler_data(p, "=\"", 2);
}

static void
xml_handler_cdata(XMLParser *p, const char *s, size_t len)
{
	if (!ctx.field)
		return;

	string_append(ctx.field, s, len);
}

/* NOTE: this handler can be called multiple times if the data in this
 *       block is bigger than the buffer. */
static void
xml_handler_data(XMLParser *p, const char *s, size_t len)
{
	if (!ctx.field)
		return;

	/* add only data from <name> inside <author> tag
	 * or any other non-<author> tag */
	if (ctx.tagid != AtomTagAuthor || istag(p->tag, p->taglen, "name", 4))
		string_append(ctx.field, s, len);
}

static void
xml_handler_data_entity(XMLParser *p, const char *data, size_t datalen)
{
	char buffer[16];
	ssize_t len;

	if (!ctx.field)
		return;

	/* try to translate entity, else just pass as data to
	 * xml_data_handler. */
	len = xml_entitytostr(data, buffer, sizeof(buffer));
	/* this should never happen (buffer too small) */
	if (len < 0)
		return;

	if (len > 0)
		xml_handler_data(p, buffer, (size_t)len);
	else
		xml_handler_data(p, data, datalen);
}

static void
xml_handler_start_el(XMLParser *p, const char *name, size_t namelen)
{
	enum TagId tagid;

	if (ISINCONTENT(ctx)) {
		ctx.attrcount = 0;
		if (ctx.contenttype == ContentTypeHTML) {
			xml_handler_data(p, "<", 1);
			xml_handler_data(p, name, namelen);
		}
		return;
	}

	/* start of RSS or Atom item / entry */
	if (ctx.feedtype == FeedTypeNone) {
		if (istag(name, namelen, STRP("entry"))) {
			/* Atom */
			ctx.feedtype = FeedTypeAtom;
			/* default content type for Atom */
			ctx.contenttype = ContentTypePlain;
		} else if (istag(name, namelen, STRP("item"))) {
			/* RSS */
			ctx.feedtype = FeedTypeRSS;
			/* default content type for RSS */
			ctx.contenttype = ContentTypeHTML;
		}
		return;
	}

	/* field tagid already set, nested tags are not allowed: return */
	if (ctx.tagid)
		return;

	/* in item */
	tagid = gettag(ctx.feedtype, name, namelen);
	ctx.tagid = tagid;

	/* map tag type to field: unknown or lesser priority is ignored,
	   when tags of the same type are repeated only the first is used. */
	if (tagid == TagUnknown || tagid <= ctx.fields[fieldmap[tagid]].tagid) {
		ctx.field = NULL;
		return;
	}
	ctx.iscontenttag = (fieldmap[ctx.tagid] == FeedFieldContent);
	ctx.field = &(ctx.fields[fieldmap[ctx.tagid]].str);
	ctx.fields[fieldmap[ctx.tagid]].tagid = tagid;
	/* clear field */
	string_clear(ctx.field);
}

static void
xml_handler_start_el_parsed(XMLParser *p, const char *tag, size_t taglen,
	int isshort)
{
	if (ctx.iscontenttag) {
		ctx.iscontent = 1;
		ctx.iscontenttag = 0;
		return;
	}

	if (!ISINCONTENT(ctx) || ctx.contenttype != ContentTypeHTML)
		return;

	if (isshort)
		xml_handler_data(p, "/>", 2);
	else
		xml_handler_data(p, ">", 1);
}

static void
xml_handler_end_el(XMLParser *p, const char *name, size_t namelen, int isshort)
{
	size_t i;

	if (ctx.feedtype == FeedTypeNone)
		return;

	if (ISINCONTENT(ctx)) {
		/* not close content field */
		if (gettag(ctx.feedtype, name, namelen) != ctx.tagid) {
			if (!isshort && ctx.contenttype == ContentTypeHTML) {
				xml_handler_data(p, "</", 2);
				xml_handler_data(p, name, namelen);
				xml_handler_data(p, ">", 1);
			}
			return;
		}
	} else if (!ctx.tagid && ((ctx.feedtype == FeedTypeAtom &&
	   istag(name, namelen, STRP("entry"))) || /* Atom */
	   (ctx.feedtype == FeedTypeRSS &&
	   istag(name, namelen, STRP("item"))))) /* RSS */
	{
		/* end of RSS or Atom entry / item */
		printfields();

		/* clear strings */
		for (i = 0; i < FeedFieldLast; i++) {
			string_clear(&ctx.fields[i].str);
			ctx.fields[i].tagid = TagUnknown;
		}
		ctx.contenttype = ContentTypeNone;
		/* allow parsing of Atom and RSS in one XML stream. */
		ctx.feedtype = FeedTypeNone;
	} else if (!ctx.tagid ||
	           gettag(ctx.feedtype, name, namelen) != ctx.tagid) {
		/* not end of field */
		return;
	}
	/* close field */
	ctx.iscontent = 0;
	ctx.tagid = TagUnknown;
	ctx.field = NULL;
}

int
main(int argc, char *argv[])
{
	if (pledge("stdio", NULL) == -1)
		err(1, "pledge");

	if (argc > 1)
		baseurl = argv[1];

	parser.xmlattr = xml_handler_attr;
	parser.xmlattrend = xml_handler_attr_end;
	parser.xmlattrstart = xml_handler_attr_start;
	parser.xmlcdata = xml_handler_cdata;
	parser.xmldata = xml_handler_data;
	parser.xmldataentity = xml_handler_data_entity;
	parser.xmltagend = xml_handler_end_el;
	parser.xmltagstart = xml_handler_start_el;
	parser.xmltagstartparsed = xml_handler_start_el_parsed;

	parser.getnext = getchar;
	xml_parse(&parser);

	return 0;
}
